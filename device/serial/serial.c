/*
 * Copyright (c) 2023, mr-library Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-04-23     MacRsh       first version
 */

#include "device/serial/serial.h"

#if (MR_CFG_SERIAL == MR_CFG_ENABLE)

static mr_err_t err_io_serial_configure(mr_serial_t serial, struct mr_serial_config *config)
{
    return -MR_ERR_IO;
}

static void err_io_serial_write(mr_serial_t serial, mr_uint8_t data)
{

}

static mr_uint8_t err_io_serial_read(mr_serial_t serial)
{
    return 0;
}

static void err_io_serial_start_tx(mr_serial_t serial)
{

}

static void err_io_serial_stop_tx(mr_serial_t serial)
{

}

static mr_err_t mr_serial_set_fifo(mr_rb_t rb, mr_size_t fifo_size)
{
    mr_uint8_t *pool = MR_NULL;

    /* Free old buffer */
    if (rb->size != 0)
    {
        mr_free(rb->buffer);
    }

    /* Allocate new buffer */
    pool = mr_malloc(fifo_size);
    if (pool == MR_NULL && fifo_size != 0)
    {
        return -MR_ERR_NO_MEMORY;
    }
    mr_rb_init(rb, pool, fifo_size);

    return MR_ERR_OK;
}

static mr_err_t mr_serial_open(mr_device_t device)
{
    mr_serial_t serial = (mr_serial_t)device;
    struct mr_serial_config default_config = MR_SERIAL_CONFIG_DEFAULT;

    /* Enable serial using the default config */
    if (serial->config.baud_rate == 0)
    {
        serial->config = default_config;
    }

    return serial->ops->configure(serial, &serial->config);
}

static mr_err_t mr_serial_close(mr_device_t device)
{
    mr_serial_t serial = (mr_serial_t)device;

    /* Disable serial */
    serial->config.baud_rate = 0;
    mr_serial_set_fifo(&serial->rx_fifo, 0);
    mr_serial_set_fifo(&serial->tx_fifo, 0);

    return serial->ops->configure(serial, &serial->config);
}

static mr_err_t mr_serial_ioctl(mr_device_t device, int cmd, void *args)
{
    mr_serial_t serial = (mr_serial_t)device;
    mr_err_t ret = MR_ERR_OK;

    switch (cmd & MR_CTRL_FLAG_MASK)
    {
        case MR_CTRL_SET_CONFIG:
        {
            if (args)
            {
                mr_serial_config_t config = (mr_serial_config_t)args;
                ret = serial->ops->configure(serial, config);
                if (ret == MR_ERR_OK)
                {
                    serial->config = *config;
                }
                return ret;
            }
            return -MR_ERR_INVALID;
        }

        case MR_CTRL_GET_CONFIG:
        {
            if (args)
            {
                mr_serial_config_t config = (mr_serial_config_t)args;
                *config = serial->config;
                return MR_ERR_OK;
            }
            return -MR_ERR_INVALID;
        }

        case MR_CTRL_SET_RX_CB:
        {
            device->rx_cb = args;
            return MR_ERR_OK;
        }

        case MR_CTRL_SET_TX_CB:
        {
            device->tx_cb = args;
            return MR_ERR_OK;
        }

        case MR_CTRL_SET_RX_BUFSZ:
        {
            if (args)
            {
                mr_size_t bufsz = *((mr_size_t *)args);
                return mr_serial_set_fifo(&serial->rx_fifo, bufsz);
            }
            return -MR_ERR_INVALID;
        }

        case MR_CTRL_SET_TX_BUFSZ:
        {
            if (args)
            {
                mr_size_t bufsz = *((mr_size_t *)args);
                return mr_serial_set_fifo(&serial->tx_fifo, bufsz);
            }
            return -MR_ERR_INVALID;
        }

        default:
            return -MR_ERR_UNSUPPORTED;
    }
}

static mr_ssize_t mr_serial_read(mr_device_t device, mr_pos_t pos, void *buffer, mr_size_t size)
{
    mr_serial_t serial = (mr_serial_t)device;
    mr_uint8_t *read_buffer = (mr_uint8_t *)buffer;
    mr_size_t read_size = 0;

    if (serial->rx_fifo.size == 0)
    {
        /* Blocking read */
        while ((read_size += sizeof(*read_buffer)) <= size)
        {
            *read_buffer = serial->ops->read(serial);
            read_buffer++;
        }
    } else
    {
        /* Non-blocking read */
        while (read_size < size)
        {
            read_size += mr_rb_read(&serial->rx_fifo, read_buffer + read_size, size - read_size);
        }
    }

    return (mr_ssize_t)read_size;
}

static mr_ssize_t mr_serial_write(mr_device_t device, mr_pos_t pos, const void *buffer, mr_size_t size)
{
    mr_serial_t serial = (mr_serial_t)device;
    mr_uint8_t *write_buffer = (mr_uint8_t *)buffer;
    mr_size_t write_size = 0;

    if (serial->tx_fifo.size == 0 || ((device->open_flag & MR_OPEN_NONBLOCKING) == MR_FALSE))
    {
        /* Blocking write */
        while ((write_size += sizeof(*write_buffer)) <= size)
        {
            serial->ops->write(serial, *write_buffer);
            write_buffer++;
        }
    } else
    {
        /* Non-blocking write */
        while (write_size < size)
        {
            /* If this is the first write, start sending */
            if (mr_rb_get_data_size(&serial->tx_fifo) != 0)
            {
                write_size += mr_rb_write(&serial->tx_fifo, write_buffer + write_size, size - write_size);
            } else
            {
                write_size += mr_rb_write(&serial->tx_fifo, write_buffer + write_size, size - write_size);

                /* Start interrupt send */
                serial->ops->start_tx(serial);
            }
        }
    }

    return (mr_ssize_t)write_size;
}

/**
 * @brief This function adds the serial device.
 *
 * @param serial The serial device to be added.
 * @param name The name of the device.
 * @param ops The operations of the device.
 * @param data The private data of the device.
 *
 * @return MR_ERR_OK on success, otherwise an error code.
 */
mr_err_t mr_serial_device_add(mr_serial_t serial, const char *name, struct mr_serial_ops *ops, void *data)
{
    static struct mr_device_ops device_ops =
        {
            mr_serial_open,
            mr_serial_close,
            mr_serial_ioctl,
            mr_serial_read,
            mr_serial_write,
        };
    mr_uint16_t support_flag = MR_OPEN_RDWR;

    MR_ASSERT(serial != MR_NULL);
    MR_ASSERT(name != MR_NULL);
    MR_ASSERT(ops != MR_NULL);

    /* Non-blocking mode */
    if (ops->start_tx != MR_NULL && ops->stop_tx != MR_NULL)
    {
        support_flag |= MR_OPEN_NONBLOCKING;
    }

    /* Initialize the private fields */
    serial->config.baud_rate = 0;
    mr_rb_init(&serial->rx_fifo, MR_NULL, 0);
    mr_rb_init(&serial->tx_fifo, MR_NULL, 0);

    /* Protect every operation of the serial device */
    ops->configure = ops->configure ? ops->configure : err_io_serial_configure;
    ops->write = ops->write ? ops->write : err_io_serial_write;
    ops->read = ops->read ? ops->read : err_io_serial_read;
    ops->start_tx = ops->start_tx ? ops->start_tx : err_io_serial_start_tx;
    ops->stop_tx = ops->stop_tx ? ops->stop_tx : err_io_serial_stop_tx;
    serial->ops = ops;

    /* Add the device */
    return mr_device_add(&serial->device, name, Mr_Device_Type_Serial, support_flag, &device_ops, data);
}

/**
 * @brief This function service interrupt routine of the serial device.
 *
 * @param serial The serial device.
 * @param event The interrupt event.
 */
void mr_serial_device_isr(mr_serial_t serial, mr_uint32_t event)
{
    MR_ASSERT(serial != MR_NULL);

    switch (event & MR_SERIAL_EVENT_MASK)
    {
        case MR_SERIAL_EVENT_RX_INT:
        {
            /* Read data into the fifo */
            mr_uint8_t data = serial->ops->read(serial);
            mr_rb_put_force(&serial->rx_fifo, data);

            /* Call the receiving completion function */
            if (serial->device.rx_cb != MR_NULL)
            {
                mr_size_t length = mr_rb_get_data_size(&serial->rx_fifo);
                serial->device.rx_cb(&serial->device, &length);
            }
            break;
        }

        case MR_SERIAL_EVENT_TX_INT:
        {
            /* Write data from the fifo */
            mr_uint8_t data = 0;
            if (mr_rb_get(&serial->tx_fifo, &data) == sizeof(data))
            {
                serial->ops->write(serial, data);
            } else
            {
                /* Stop interrupt send */
                serial->ops->stop_tx(serial);

                /* Call the sending completion function */
                if (serial->device.tx_cb != MR_NULL)
                {
                    mr_size_t length = 0;
                    serial->device.tx_cb(&serial->device, &length);
                }
            }
            break;
        }

        default:
            break;
    }
}

#endif