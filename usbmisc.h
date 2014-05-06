/*****************************************************************************/
/*
 *      usbmisc.h  --  Misc USB routines
 *
 *      Copyright (C) 2003  Aurelien Jarno (aurelien@aurel32.net)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 */

/*****************************************************************************/

#ifndef _USBMISC_H
#define _USBMISC_H

#include <libusb.h>

#ifdef OS_DARWIN
#include <IOKit/IOCFBundle.h>
#include <IOKit/usb/IOUSBLib.h>
#endif

#define MY_STRING_MAX 256
#define MY_PATH_MAX 4096
#define MY_PARAM_MAX 64

#ifdef OS_LINUX
#define SBUD "/sys/bus/usb/devices/"
extern int linux_get_device_info_path(char *buf, size_t size, unsigned int location_id);
extern int linux_get_usb_device(libusb_device *dev, libusb_context *ctx, const char *path);
#endif

/* ---------------------------------------------------------------------- */

extern int get_dev_string(char *buf, size_t size, libusb_device_handle *hdev, u_int8_t id);
extern unsigned int get_location_id(libusb_device *dev);
extern int get_string_from_cache(char *buf, size_t size, libusb_device *dev, unsigned int referrer);

#ifdef OS_DARWIN
extern SInt32 GetSInt32CFProperty(io_service_t obj, CFStringRef key);
extern IOReturn darwin_get_service_from_location_id ( unsigned int location_id, io_service_t *service );
extern int darwin_get_ioreg_string(char *buf, size_t size, io_service_t service, CFStringRef property);
#endif /* OS_DARWIN */

/* ---------------------------------------------------------------------- */
#endif /* _USBMISC_H */
