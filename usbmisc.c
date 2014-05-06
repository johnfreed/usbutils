/*****************************************************************************/
/*
 *      usbmisc.c  --  Misc USB routines
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
 *
 */

/*****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#ifdef HAVE_NL_LANGINFO
#include <langinfo.h>
#endif

#include "usbmisc.h"

#ifdef OS_LINUX
/* ---------------------------------------------------------------------- */

static const char *devbususb = "/dev/bus/usb";

/* ---------------------------------------------------------------------- */

static int readlink_recursive(const char *path, char *buf, size_t bufsize)
{
	char temp[PATH_MAX + 1];
	char *ptemp;
	int ret;

	ret = readlink(path, buf, bufsize-1);

	if (ret > 0) {
		buf[ret] = 0;
		if (*buf != '/') {
			strncpy(temp, path, sizeof(temp));
			ptemp = temp + strlen(temp);
			while (*ptemp != '/' && ptemp != temp)
				ptemp--;
			ptemp++;
			strncpy(ptemp, buf, bufsize + temp - ptemp);
		} else
			strncpy(temp, buf, sizeof(temp));
		return readlink_recursive(temp, buf, bufsize);
	} else {
		strncpy(buf, path, bufsize);
		return strlen(buf);
	}
}

static char *get_absolute_path(const char *path, char *result,
			       size_t result_size)
{
	const char *ppath;	/* pointer on the input string */
	char *presult;		/* pointer on the output string */

	ppath = path;
	presult = result;
	result[0] = 0;

	if (path == NULL)
		return result;

	if (*ppath != '/') {
		result = getcwd(result, result_size);
		presult += strlen(result);
		result_size -= strlen(result);

		*presult++ = '/';
		result_size--;
	}

	while (*ppath != 0 && result_size > 1) {
		if (*ppath == '/') {
			do
				ppath++;
			while (*ppath == '/');
			*presult++ = '/';
			result_size--;
		} else if (*ppath == '.' && *(ppath + 1) == '.' &&
			   *(ppath + 2) == '/' && *(presult - 1) == '/') {
			if ((presult - 1) != result) {
				/* go one directory upper */
				do {
					presult--;
					result_size++;
				} while (*(presult - 1) != '/');
			}
			ppath += 3;
		} else if (*ppath == '.'  &&
			   *(ppath + 1) == '/' &&
			   *(presult - 1) == '/') {
			ppath += 2;
		} else {
			*presult++ = *ppath++;
			result_size--;
		}
	}
	/* Don't forget to mark the end of the string! */
	*presult = 0;

	return result;
}

int linux_get_usb_device(libusb_device *dev, libusb_context *ctx, const char *path)
{
	libusb_device **list;
	ssize_t num_devs, i;
	char device_path[PATH_MAX + 1];
	char absolute_path[PATH_MAX + 1];

	readlink_recursive(path, device_path, sizeof(device_path));
	get_absolute_path(device_path, absolute_path, sizeof(absolute_path));

	dev = NULL;
	num_devs = libusb_get_device_list(ctx, &list);

	for (i = 0; i < num_devs; ++i) {
		uint8_t bnum = libusb_get_bus_number(list[i]);
		uint8_t dnum = libusb_get_device_address(list[i]);

		snprintf(device_path, sizeof(device_path), "%s/%03u/%03u",
			 devbususb, bnum, dnum);
		if (!strcmp(device_path, absolute_path)) {
			dev = list[i];
			break;
		}
	}

	libusb_free_device_list(list, 0);
	return LIBUSB_SUCCESS;
}

int linux_get_device_info_path(char *path, size_t size, unsigned int location_id)
{
	unsigned int mask;
	unsigned int busnum;
	int fd;
	ssize_t r;
	int i = 0;

	if (size < strlen(SBUD))
		return 0;

	snprintf(path, size, "%s", SBUD);

	busnum = location_id >> 24;
	mask = 0x00ffffff;

	location_id &= mask;
	if ( location_id == 0 )
	{
		/* root hub is special case */
		snprintf(path + strlen(path), size - strlen(path), "%s", "usb");

	}
	snprintf(path + strlen(path), size - strlen(path), "%d", busnum);
	for (i=0; i<6; i++) {
		int next = location_id >> (20 - i*4);
		if (!next)
			break;
		snprintf(path + strlen(path), size - strlen(path), "%s%d", ((i>0) ? "." : "-" ), next);
		mask >>= 4;
		location_id &= mask;
	}
	return LIBUSB_SUCCESS;
}
#endif

/*
This call is to get a cached descriptor string, and thus is OS-dependent.
It is not for use in two special cases that are not in the descriptors:
	libusb_get_device_speed
	libusb_get_kernel_driver_name
*/
#ifdef OS_LINUX
int get_string_from_cache(char *buf, size_t size, libusb_device *dev, unsigned int referrer)
{
	/* referrer is something like LIBUSB_DEVICE_BCDUSB */
	/* return is length of string in buf */
	const char *cacheID;
	int fd, r;
	unsigned int location_id;
	char path[MY_PATH_MAX];

	if (size < 1)
		return LIBUSB_SUCCESS;
	*buf = 0;
	path[0] = '\0';
	location_id = get_location_id(dev);

	linux_get_device_info_path(path, sizeof(path), location_id);
	snprintf(path + strlen(path), size - strlen(path), "%s", "/");

	switch (referrer) {
	case LIBUSB_HUB_N_NBRPORTS:
		cacheID = "/maxchild"; break;
	case LIBUSB_DEVICE_BCD_DEVICE:
		cacheID = "/bcdDevice"; break;
	case LIBUSB_CONFIG_B_CONFIGURATIONVALUE:
		cacheID = "/bConfigurationValue"; break;
	case LIBUSB_DEVICE_B_DEVICECLASS:
		cacheID = "/bDeviceClass"; break;
	case LIBUSB_DEVICE_B_DEVICEPROTOCOL:
		cacheID = "/bDeviceProtocol"; break;
	case LIBUSB_DEVICE_B_DEVICESUBCLASS:
		cacheID = "/bDeviceSubClass"; break;
	case LIBUSB_CONFIG_BM_ATTRIBUTES:
		cacheID = "/bmAttributes"; break;
	case LIBUSB_DEVICE_B_MAXPACKETSIZE0:
		cacheID = "/bMaxPacketSize0"; break;
	case LIBUSB_CONFIG_B_MAXPOWER:
		cacheID = "/bMaxPower"; break;
	case LIBUSB_DEVICE_B_NUMCONFIGURATIONS:
		cacheID = "/bNumConfigurations"; break;
	case LIBUSB_CONFIG_B_NUMINTERFACES:
		cacheID = "/bNumInterfaces"; break;
	case LIBUSB_CONFIG_S_CONFIGURATION:
		cacheID = "/configuration"; break;
	case LIBUSB_DEVICE_ID_PRODUCT:
		cacheID = "/idProduct"; break;
	case LIBUSB_DEVICE_ID_VENDOR:
		cacheID = "/idVendor"; break;
	case LIBUSB_DEVICE_BCD_USB:
		cacheID = "/version"; break;
	case LIBUSB_DEVICE_S_MANUFACTURER:
		cacheID = "/manufacturer"; break;
	case LIBUSB_DEVICE_S_PRODUCT:
		cacheID = "/product"; break;
	case LIBUSB_DEVICE_S_SERIALNUMBER:
		cacheID = "/serial"; break;
	case LIBUSB_ENDPOINT_B_ENDPOINTADDRESS:
		cacheID = "/ep00/bEndpointAddress"; break;
	case LIBUSB_ENDPOINT_B_INTERVAL:
		cacheID = "/ep00/bInterval"; break;
	case LIBUSB_ENDPOINT_B_LENGTH:
		cacheID = "/ep00/bLength"; break;
	case LIBUSB_ENDPOINT_BM_ATTRIBUTES:
		cacheID = "/ep00/bmAttributes"; break;
	case LIBUSB_ENDPOINT_W_MAXPACKETSIZE:
		cacheID = "/ep00/wMaxPacketSize"; break; /* hex */
	}

	strncat(path, cacheID, sizeof(path) - strlen(path) - 1);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return 0;
	r = read(fd, buf, size - 1);
	close(fd);
	if (r < 0)
		return 0;

	/* buf has \n appended */
	if (r < 2)
		return 0;

	buf[r - 1] = '\0';
	return strlen(buf);
}

#endif

static void get_dev_string_ascii(char *buf, size_t size, libusb_device_handle *dev,
				  u_int8_t id)
{
	int ret = libusb_get_string_descriptor_ascii(dev, id,
	                                             (unsigned char *) buf,
	                                             size);

	if (ret < 0) {
		snprintf(buf, size, "%s", "(error)");
	}

}

#if defined(HAVE_NL_LANGINFO) && defined(HAVE_ICONV)
static u_int16_t get_any_langid(libusb_device_handle *dev)
{
	unsigned char buf[4];
	int ret = libusb_get_string_descriptor(dev, 0, 0, buf, sizeof buf);
	if (ret != sizeof buf) return 0;
	return buf[2] | (buf[3] << 8);
}

static char *usb_string_to_native(char * str, size_t len)
{
	size_t num_converted;
	iconv_t conv;
	char *result, *result_end;
	size_t in_bytes_left, out_bytes_left;

	conv = iconv_open(nl_langinfo(CODESET), "UTF-16LE");

	if (conv == (iconv_t) -1)
		return NULL;

	in_bytes_left = len * 2;
	out_bytes_left = len * MB_CUR_MAX;
	result = result_end = malloc(out_bytes_left + 1);

	num_converted = iconv(conv, &str, &in_bytes_left,
			      &result_end, &out_bytes_left);

	iconv_close(conv);
	if (num_converted == (size_t) -1) {
		free(result);
		return NULL;
	}

	*result_end = 0;
	return result;
}
#endif

int get_dev_string(char *buf, size_t size, libusb_device_handle *hdev, u_int8_t id)
{
#if defined(HAVE_NL_LANGINFO) && defined(HAVE_ICONV)
	int ret;
	unsigned char unicode_buf[254];
	u_int16_t langid;
#endif

	if (!hdev || !id) {
		return 0;
	}
#if defined(HAVE_NL_LANGINFO) && defined(HAVE_ICONV)
	langid = get_any_langid(hdev);
	if (!langid) {
		return snprintf(buf, size, "%s", "(error)");
	}

	ret = libusb_get_string_descriptor(hdev, id, langid,
					   (unsigned char *) unicode_buf,
					   sizeof unicode_buf);
	if (ret < 2) {
		return snprintf(buf, size, "%s", "(error)");
	}
	char *tmp = usb_string_to_native(unicode_buf + 2,
				   ((unsigned char) unicode_buf[0] - 2) / 2);

	if (tmp) {
		snprintf(buf, size, "%s", tmp);
		free(tmp);
	} else {
		get_dev_string_ascii(buf, size, hdev, id);
	}
	return strlen(buf);
#else
	get_dev_string_ascii(buf, size, hdev, id);
	return strlen(buf);
#endif
}

unsigned int get_location_id(libusb_device *dev)
{
	uint8_t port_numbers[7];
	int j;
	int count = libusb_get_port_numbers(dev,port_numbers, 7);
	unsigned int location_id = 0;
	for (j = 0; j < count; j++) {
		location_id |= (port_numbers[j] & 0xf) << (20 - 4*j);
	}
	location_id |= (libusb_get_bus_number(dev) << 24);
	return location_id;
}

#ifdef OS_DARWIN
SInt32 GetSInt32CFProperty(io_service_t obj, CFStringRef key)
{
	SInt32 value = 0;

	CFNumberRef valueRef = IORegistryEntryCreateCFProperty(obj, key, kCFAllocatorDefault, 0);
	if (valueRef) {
		CFNumberGetValue(valueRef, kCFNumberSInt32Type, &value);
		CFRelease(valueRef);
	}
	return value;
}

UInt32 GetLocationID(io_service_t obj);

UInt32 GetLocationID(io_service_t obj) {
	return (UInt32) GetSInt32CFProperty(obj, CFSTR(kUSBDevicePropertyLocationID));
}

IOReturn darwin_get_service_from_location_id ( unsigned int location_id, io_service_t *service )
{
	io_iterator_t deviceIterator;
	unsigned int found_location_id;

	CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);

	if (!matchingDict)
		return kIOReturnError;

	IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &deviceIterator);

	while ((*service = IOIteratorNext (deviceIterator))) {
		/* get the location from the i/o registry */

		found_location_id = GetLocationID(*service);

		if (location_id == found_location_id) {
		  found_location_id = GetLocationID(*service);
		  IOObjectRelease(deviceIterator);
		  return kIOReturnSuccess;
		}
	}
	/* not found, shouldn't happen */
	IOObjectRelease(deviceIterator);
	return kIOReturnError;
}


int get_string_from_cache(char *buf, size_t size, libusb_device *dev, unsigned int referrer)
{
	CFStringRef property;
	unsigned int location_id;
	io_service_t service;
	unsigned int typeID; /* either the base of the number, or 0 for string */
	unsigned int child; /* if it's a child property, indicate here */

	if (size < 1)
		return LIBUSB_SUCCESS;
	*buf = 0;

	child = 0;
	typeID = 0;

	switch (referrer) {
	case LIBUSB_DEVICE_B_MAXPACKETSIZE0:
		typeID = 16; property = CFSTR("bMaxPacketSize0"); break;
	case LIBUSB_DEVICE_ID_PRODUCT:
		typeID = 16; property = CFSTR("idProduct"); break;
	case LIBUSB_DEVICE_ID_VENDOR:
		typeID = 16; property = CFSTR("idVendor"); break;
	case LIBUSB_DEVICE_BCD_USB:
		typeID = 16; property = CFSTR("bcdUSB"); break;
	case LIBUSB_DEVICE_S_MANUFACTURER:
		typeID = 0; property = CFSTR("USB Vendor Name"); break;
	case LIBUSB_DEVICE_S_PRODUCT:
		typeID = 0; property = CFSTR("USB Product Name"); break;
	case LIBUSB_DEVICE_S_SERIALNUMBER:
		typeID = 0; property = CFSTR("USB Serial Number"); break;
	case LIBUSB_DEVICE_BCD_DEVICE:
		typeID = 16; property = CFSTR("bcdDevice"); break;
	case LIBUSB_CONFIG_B_CONFIGURATIONVALUE:
		child = 1; typeID = 16; property = CFSTR("bConfigurationValue"); break;
	case LIBUSB_INTERFACE_B_INTERFACENUMBER:
		child = 1; typeID = 16; property = CFSTR("bInterfaceNumber"); break;
	case LIBUSB_INTERFACE_B_ALTERNATESETTING:
		child = 1; typeID = 16; property = CFSTR("bAlternateSetting"); break;
	case LIBUSB_INTERFACE_B_NUMENDPOINTS:
		child = 1; typeID = 16; property = CFSTR("bNumEndpoints"); break;
	case LIBUSB_INTERFACE_B_INTERFACECLASS:
		child = 1; typeID = 16; property = CFSTR("bInterfaceClass"); break;
	case LIBUSB_INTERFACE_B_INTERFACESUBCLASS:
		child = 1; typeID = 16; property = CFSTR("bInterfaceSubClass"); break;
	case LIBUSB_INTERFACE_B_INTERFACEPROTOCOL:
		child = 1; typeID = 16; property = CFSTR("bInterfaceProtocol"); break;
	case LIBUSB_DEVICE_B_DEVICECLASS:
		typeID = 16; property = CFSTR("bDeviceClass"); break;
	case LIBUSB_DEVICE_B_DEVICEPROTOCOL:
		typeID = 16; property = CFSTR("bDeviceProtocol"); break;
	case LIBUSB_DEVICE_B_DEVICESUBCLASS:
		typeID = 16; property = CFSTR("bDeviceSubClass"); break;
	case LIBUSB_CONFIG_BM_ATTRIBUTES:
		typeID = 16; property = CFSTR("bmAttributes"); break;
	case LIBUSB_CONFIG_B_MAXPOWER:
		typeID = 16; property = CFSTR("Requested Power"); break;
	case LIBUSB_DEVICE_B_NUMCONFIGURATIONS:
		typeID = 16; property = CFSTR("bNumConfigurations"); break;
	case LIBUSB_CONFIG_B_NUMINTERFACES:
		typeID = 16; property = CFSTR("bNumInterfaces"); break;
	case LIBUSB_CONFIG_S_CONFIGURATION:
		typeID = 0; property = CFSTR("Configuration"); break;
	case LIBUSB_HUB_N_NBRPORTS:
		typeID = 16; property = CFSTR("Ports"); break;
	case LIBUSB_HUB_B_HUBCONTRCURRENT:
		typeID = 16; property = CFSTR("Bus Power Available"); break;
	}

	location_id = get_location_id(dev);
	darwin_get_service_from_location_id (location_id, &service);
	if (typeID == 0) {
		darwin_get_ioreg_string(buf, size, service, property);
	} else {
		if (typeID == 10) {
			snprintf(buf, size, "%d", (UInt32) GetSInt32CFProperty(service, property));
		} else if (typeID == 16) {
			snprintf(buf, size,"%x", (UInt32) GetSInt32CFProperty(service, property));
		}
	}
	return LIBUSB_SUCCESS;
}

int darwin_get_ioreg_string(char *buff, size_t len, io_service_t service, CFStringRef property) {
	CFIndex length;
	CFIndex maxSize;

	if (len < 1)
		return 0;

	buff[0] = '\0';
	CFStringRef cfBuff = (CFStringRef)IORegistryEntryCreateCFProperty (service, property, kCFAllocatorDefault, 0);
	if (cfBuff) {
	        length = CFStringGetLength(cfBuff);
	        maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
	        char *utf_buff = malloc(maxSize);
	        CFStringGetCString(cfBuff, utf_buff, maxSize, kCFStringEncodingUTF8);
	        CFRelease(cfBuff);
		strncpy(buff, utf_buff, len);
		free(utf_buff);
	}

	return strlen(buff);
}


#endif /* OS_DARWIN */

