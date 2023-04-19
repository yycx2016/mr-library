/*
 * Copyright (c), mr-library Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-03-16     MacRsh       first version
 */

#include <device/pin/pin.h>

#if (MR_DEVICE_PIN == MR_CONF_ENABLE)

static mr_err_t mr_pin_ioctl(mr_device_t device, int cmd, void *args)
{
	mr_pin_t pin = (mr_pin_t)device;
	mr_err_t ret = MR_ERR_OK;

	switch (cmd & _MR_CMD_MASK)
	{
		case MR_CMD_CONFIG:
		{
			if (args)
				ret = pin->ops->configure(pin,
										  ((struct mr_pin_config *)args)->number,
										  ((struct mr_pin_config *)args)->mode);

			break;
		}

		default: ret = - MR_ERR_UNSUPPORTED;
	}

	return ret;
}

static mr_size_t mr_pin_read(mr_device_t device, mr_off_t pos, void *buffer, mr_size_t size)
{
	mr_pin_t pin = (mr_pin_t)device;
	mr_uint8_t *recv_buffer = (mr_uint8_t *)buffer;

	if (size != sizeof(mr_uint8_t))
		return 0;

	*recv_buffer = pin->ops->read(pin, (mr_uint16_t)pos);

	return size;
}

static mr_size_t mr_pin_write(mr_device_t device, mr_off_t pos, const void *buffer, mr_size_t size)
{
	mr_pin_t pin = (mr_pin_t)device;
	mr_uint8_t *send_buffer = (mr_uint8_t *)buffer;

	if (size != sizeof(mr_uint8_t))
		return 0;

	pin->ops->write(pin, (mr_uint16_t)pos, *send_buffer);

	return size;
}

static mr_err_t _err_io_pin_configure(mr_pin_t pin, mr_uint16_t number, mr_uint16_t mode)
{
	MR_ASSERT(0);
	return - MR_ERR_IO;
}

static void _err_io_pin_write(mr_pin_t pin, mr_uint16_t number, mr_uint8_t value)
{
	MR_ASSERT(0);
}

static mr_uint8_t _err_io_pin_read(mr_pin_t pin, mr_uint16_t number)
{
	MR_ASSERT(0);
	return 0;
}

mr_err_t mr_hw_pin_add_to_container(mr_pin_t pin, const char *name, struct mr_pin_ops *ops, void *data)
{
	mr_err_t ret = MR_ERR_OK;
	const static struct mr_device_ops device_ops =
		{
			MR_NULL,
			MR_NULL,
			mr_pin_ioctl,
			mr_pin_read,
			mr_pin_write,
		};

	MR_ASSERT(pin != MR_NULL);
	MR_ASSERT(ops != MR_NULL);

	/* Add the pin-device to the container */
	ret = mr_device_add_to_container(&pin->device, name, MR_DEVICE_TYPE_PIN, MR_OPEN_RDWR, &device_ops, data);
	if (ret != MR_ERR_OK)
		return ret;

	/* Set pin operations as protect functions if ops is null */
	ops->configure = ops->configure ? ops->configure : _err_io_pin_configure;
	ops->write = ops->write ? ops->write : _err_io_pin_write;
	ops->read = ops->read ? ops->read : _err_io_pin_read;
	pin->ops = ops;

	return MR_ERR_OK;
}

#endif