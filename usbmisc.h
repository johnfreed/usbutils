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

/* ---------------------------------------------------------------------- */

extern libusb_device *get_usb_device(libusb_context *ctx, const char *path);
extern char *get_dev_string(libusb_device_handle *hdev, u_int8_t id);

#ifdef OS_DARWIN
extern SInt32 GetSInt32CFProperty(io_service_t obj, CFStringRef key);
extern IOReturn darwin_get_service_from_location_id ( unsigned int location_id, io_service_t *service );
extern char *darwin_get_ioreg_string(io_service_t service, CFStringRef property);
#endif /* OS_DARWIN */

/* ---------------------------------------------------------------------- */
#endif /* _USBMISC_H */
