/*
 * Copyright 2012 Jared Boone
 * Copyright 2013 Benjamin Vernoux
 *
 * This file is part of GreatFET.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "usb_api_spiflash.h"
#include "usb_queue.h"

#include <stddef.h>
#include <greatfet_core.h>
#include <spiflash.h>
#include <spiflash_target.h>
#include <gpio_lpc.h>

/* Buffer size == spi_flash.page_len */
uint8_t spiflash_buffer[256U];

#define W25Q80BV_DEVICE_ID_RES  0x14 /* Expected device_id for W25Q16DV */

#define W25Q80BV_PAGE_LEN 256U
#define W25Q80BV_NUM_PAGES 8192U
#define W25Q80BV_NUM_BYTES (W25Q80BV_PAGE_LEN * W25Q80BV_NUM_PAGES)

static struct gpio_t gpio_spiflash_hold   = GPIO(1, 14);
static struct gpio_t gpio_spiflash_wp     = GPIO(1, 15);
static struct gpio_t gpio_spiflash_select = GPIO(5, 11);
static spi_target_t spi_target = {
	.bus = &spi_bus_ssp0,
	.gpio_hold = &gpio_spiflash_hold,
	.gpio_wp = &gpio_spiflash_wp,
	.gpio_select = &gpio_spiflash_select,
};

static spiflash_driver_t spi_flash_drv = {
	.target = &spi_target,
    .target_init = spiflash_target_init,
	.page_len = W25Q80BV_PAGE_LEN,
	.num_pages = W25Q80BV_NUM_PAGES,
	.num_bytes = W25Q80BV_NUM_BYTES,
    .device_id = W25Q80BV_DEVICE_ID_RES,
};

usb_request_status_t usb_vendor_request_erase_spiflash(
		usb_endpoint_t* const endpoint, const usb_transfer_stage_t stage)
{
	if (stage == USB_TRANSFER_STAGE_SETUP) {
		spi_bus_start(spi_flash_drv.target, &ssp_config_spi);
		spiflash_setup(&spi_flash_drv);
		/* only chip erase is implemented */
		spiflash_chip_erase(&spi_flash_drv);
		usb_transfer_schedule_ack(endpoint->in);
	}
	return USB_REQUEST_STATUS_OK;
}

usb_request_status_t usb_vendor_request_write_spiflash(
	usb_endpoint_t* const endpoint, const usb_transfer_stage_t stage)
{
	uint32_t addr = 0;
	uint16_t len = 0;

	if (stage == USB_TRANSFER_STAGE_SETUP) {
		addr = (endpoint->setup.value << 16) | endpoint->setup.index;
		len = endpoint->setup.length;
		if ((len > spi_flash_drv.page_len) || (addr > spi_flash_drv.num_bytes)
				|| ((addr + len) > spi_flash_drv.num_bytes)) {
			return USB_REQUEST_STATUS_STALL;
		} else {
			usb_transfer_schedule_block(endpoint->out, &spiflash_buffer[0], len,
						    NULL, NULL);
			spi_bus_start(spi_flash_drv.target, &ssp_config_spi);
			spiflash_setup(&spi_flash_drv);
			return USB_REQUEST_STATUS_OK;
		}
	} else if (stage == USB_TRANSFER_STAGE_DATA) {
		addr = (endpoint->setup.value << 16) | endpoint->setup.index;
		len = endpoint->setup.length;
		/* This check is redundant but makes me feel better. */
		if ((len > spi_flash_drv.page_len) || (addr > spi_flash_drv.num_bytes)
				|| ((addr + len) > spi_flash_drv.num_bytes)) {
			return USB_REQUEST_STATUS_STALL;
		} else {
			spiflash_program(&spi_flash_drv, addr, len, &spiflash_buffer[0]);
			usb_transfer_schedule_ack(endpoint->in);
			return USB_REQUEST_STATUS_OK;
		}
	} else {
		return USB_REQUEST_STATUS_OK;
	}
}

usb_request_status_t usb_vendor_request_read_spiflash(
	usb_endpoint_t* const endpoint, const usb_transfer_stage_t stage)
{
	uint32_t addr;
	uint16_t len;

	if (stage == USB_TRANSFER_STAGE_SETUP) 
	{
		addr = (endpoint->setup.value << 16) | endpoint->setup.index;
		len = endpoint->setup.length;
		if ((len > spi_flash_drv.page_len) || (addr > spi_flash_drv.num_bytes)
			    || ((addr + len) > spi_flash_drv.num_bytes)) {
			return USB_REQUEST_STATUS_STALL;
		} else {
			spiflash_read(&spi_flash_drv, addr, len, &spiflash_buffer[0]);
			usb_transfer_schedule_block(endpoint->in, &spiflash_buffer[0], len,
						    NULL, NULL);
			return USB_REQUEST_STATUS_OK;
		}
	} else if (stage == USB_TRANSFER_STAGE_DATA) 
	{
			addr = (endpoint->setup.value << 16) | endpoint->setup.index;
			len = endpoint->setup.length;
			/* This check is redundant but makes me feel better. */
			if ((len > spi_flash_drv.page_len) || (addr > spi_flash_drv.num_bytes)
					|| ((addr + len) > spi_flash_drv.num_bytes)) 
			{
				return USB_REQUEST_STATUS_STALL;
			} else
			{
				usb_transfer_schedule_ack(endpoint->out);
				return USB_REQUEST_STATUS_OK;
			}
	} else 
	{
		return USB_REQUEST_STATUS_OK;
	}
}

