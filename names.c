/*****************************************************************************/
/*
 *      names.c  --  USB name database manipulation routines
 *
 *      Copyright (C) 1999, 2000  Thomas Sailer (sailer@ife.ee.ethz.ch)
 *      Copyright (C) 2013  Tom Gundersen (teg@jklm.no)
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#ifdef USE_UDEV
#include <libudev.h>
#endif

#include "usb-spec.h"
#include "usb-vendors.h"
#include "names.h"
#include "usbmisc.h"


#define HASH1  0x10
#define HASH2  0x02
#define HASHSZ 512

static unsigned int hashnum(unsigned int num)
{
	unsigned int mask1 = HASH1 << 27, mask2 = HASH2 << 27;

	for (; mask1 >= HASH1; mask1 >>= 1, mask2 >>= 1)
		if (num & mask1)
			num ^= mask2;
	return num & (HASHSZ-1);
}

/* ---------------------------------------------------------------------- */

#ifdef USE_UDEV
static struct udev *udev = NULL;
static struct udev_hwdb *hwdb = NULL;
#endif
static struct audioterminal *audioterminals_hash[HASHSZ] = { NULL, };
static struct videoterminal *videoterminals_hash[HASHSZ] = { NULL, };
static struct genericstrtable *hiddescriptors_hash[HASHSZ] = { NULL, };
static struct genericstrtable *reports_hash[HASHSZ] = { NULL, };
static struct genericstrtable *huts_hash[HASHSZ] = { NULL, };
static struct genericstrtable *biass_hash[HASHSZ] = { NULL, };
static struct genericstrtable *physdess_hash[HASHSZ] = { NULL, };
static struct genericstrtable *hutus_hash[HASHSZ] = { NULL, };
static struct genericstrtable *langids_hash[HASHSZ] = { NULL, };
static struct genericstrtable *countrycodes_hash[HASHSZ] = { NULL, };
static struct genericstrtable *classes_hash[HASHSZ] = { NULL, };
static struct genericstrtable *subclasses_hash[HASHSZ] = { NULL, };
static struct genericstrtable *protocols_hash[HASHSZ] = { NULL, };
static struct genericstrtable *vendors_hash[HASHSZ] = { NULL, };


/* ---------------------------------------------------------------------- */

static const char *names_genericstrtable(struct genericstrtable *t[HASHSZ],
					 unsigned int idx)
{
	struct genericstrtable *h;

	for (h = t[hashnum(idx)]; h; h = h->next)
		if (h->num == idx)
			return h->name;
	return NULL;
}

const char *names_hid(u_int8_t hidd)
{
	return names_genericstrtable(hiddescriptors_hash, hidd);
}

const char *names_reporttag(u_int8_t rt)
{
	return names_genericstrtable(reports_hash, rt);
}

const char *names_huts(unsigned int data)
{
	return names_genericstrtable(huts_hash, data);
}

const char *names_hutus(unsigned int data)
{
	return names_genericstrtable(hutus_hash, data);
}

const char *names_langid(u_int16_t langid)
{
	return names_genericstrtable(langids_hash, langid);
}

const char *names_physdes(u_int8_t ph)
{
	return names_genericstrtable(physdess_hash, ph);
}

const char *names_bias(u_int8_t b)
{
	return names_genericstrtable(biass_hash, b);
}

const char *names_countrycode(unsigned int countrycode)
{
	return names_genericstrtable(countrycodes_hash, countrycode);
}

#ifdef USE_UDEV
static const char *hwdb_get(const char *modalias, const char *key)
{
	struct udev_list_entry *entry;

	udev_list_entry_foreach(entry, udev_hwdb_get_properties_list_entry(hwdb, modalias, 0))
		if (strcmp(udev_list_entry_get_name(entry), key) == 0)
			return udev_list_entry_get_value(entry);

	return NULL;
}
#endif


int get_vendor_string(char *buf, size_t size, libusb_device *dev)
{
	/* return value is length of string in buf */

	u_int16_t vendorid;
	struct libusb_device_descriptor desc;
	const char *vendorName;

	if (size < 1)
		return 0;
	*buf = 0;

	libusb_get_device_descriptor(dev, &desc);
	vendorid = desc.idVendor;
#ifdef USE_UDEV
	char modalias[64];
	sprintf(modalias, "usb:v%04Xp%04X*", vendorid);
	return snprintf(buf, size, "%s", hwdb_get(modalias, "ID_VENDOR_FROM_DATABASE"));
#endif
	/* since iManufacturer theoretically gives the OEM and the vendorID gives the vendor, try the lookup table first */
	vendorName = names_genericstrtable(vendors_hash, vendorid);
	int r = snprintf(buf, size, "%s", vendorName);
	if (r > 0)
		return r;
#ifdef OS_LINUX
	/* lookup failed, so try to get name from /sys/bus/usb/devices */
	char *tmp;
	get_string_from_cache(tmp, 128, dev, LIBUSB_DEVICE_S_MANUFACTURER);
	return snprintf(buf, size, "%s", tmp);
#endif
#ifdef OS_DARWIN
	/* lookup failed, so try to get name from IORegistry */
	int vid = (int)vendorid;
	CFMutableDictionaryRef matchingDict;
	CFNumberRef refVendorId = CFNumberCreate(NULL, kCFNumberIntType, &vid);
	matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
	CFDictionarySetValue (matchingDict, CFSTR(kUSBVendorID), refVendorId);
	CFRelease(refVendorId);
	CFDictionarySetValue (matchingDict, CFSTR(kUSBProductID), CFSTR("*"));
	io_service_t device = IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);
	darwin_get_ioreg_string(buf, size, device, CFSTR(kUSBVendorString));
	IOObjectRelease(device);
	return strlen(buf);
#endif
	return 0;
}

/* get_product_string
 * Use the device descriptor to try to find the name of the product.
 * If USE_UDEV is defined, then look in the udev hardware database.
 * If USE_UDEV is undefined and hdev is NULL, return a null string.
 * If USE_UDEV is undefined but hdev is passed, return the iProduct string.
 * return value is length of string in buf */

int get_product_string(char *buf, size_t size, libusb_device *dev)
{
	struct libusb_device_descriptor desc;
	
	if (size < 1)
		return 0;
	*buf = 0;
#ifdef USE_UDEV
	u_int16_t vendorid, productid;

	libusb_get_device_descriptor(dev, &desc);
	vendorid = desc.idVendor;
	productid = desc.idProduct;
	char modalias[64];
	sprintf(modalias, "usb:v%04Xp%04X*", vendorid, productid);
	strncpy(buf, hwdb_get(modalias, "ID_MODEL_FROM_DATABASE"), 128);
	return strlen(buf);
#endif
#ifdef OS_LINUX
	/* try to get name from /sys/bus/usb/devices */
	get_string_from_cache(buf, 128, dev, LIBUSB_DEVICE_S_PRODUCT);
	return strlen(buf);
#endif
#ifdef OS_DARWIN
	/* try to get name from IORegistry */
	get_string_from_cache(buf, 128, dev, LIBUSB_DEVICE_S_PRODUCT);
	return strlen(buf);
#endif

	/* try to get name from iProduct string */
	if (dev) {
		int r;
		libusb_device_handle *hdev;
		if (libusb_open(dev,&hdev) != LIBUSB_SUCCESS )
			return 0;
		if (libusb_get_device_descriptor(dev, &desc) != LIBUSB_SUCCESS)
			return 0;
		get_dev_string(buf, size, hdev, desc.iProduct);
		libusb_close(hdev);
		return strlen(buf);
	}
	return 0;
}

const char *names_class(u_int8_t classid)
{
	return names_genericstrtable(classes_hash, classid);
}

const char *names_subclass(u_int8_t classid, u_int8_t subclassid)
{
	u_int16_t subclass = ( classid << 8 ) + subclassid;
	return names_genericstrtable(subclasses_hash, subclass);
}

const char *names_protocol(u_int8_t classid, u_int8_t subclassid, u_int8_t protocolid)
{
	unsigned int protocol = ( classid << 16 ) + ( subclassid << 8 ) + protocolid;
	return names_genericstrtable(protocols_hash, protocol);
}

const char *names_audioterminal(u_int16_t termt)
{
	struct audioterminal *at;

	at = audioterminals_hash[hashnum(termt)];
	for (; at; at = at->next)
		if (at->termt == termt)
			return at->name;
	return NULL;
}

const char *names_videoterminal(u_int16_t termt)
{
	struct videoterminal *vt;

	vt = videoterminals_hash[hashnum(termt)];
	for (; vt; vt = vt->next)
		if (vt->termt == termt)
			return vt->name;
	return NULL;
}

/* ---------------------------------------------------------------------- */


int get_class_string(char *buf, size_t size, u_int8_t cls)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_class(cls)))
		return 0;
	return snprintf(buf, size, "%s", cp);
}

int get_subclass_string(char *buf, size_t size, u_int8_t cls, u_int8_t subcls)
{
	const char *cp;

	if (size < 1)
		return 0;
	*buf = 0;
	if (!(cp = names_subclass(cls, subcls)))
		return 0;
	return snprintf(buf, size, "%s", cp);
}

/* ---------------------------------------------------------------------- */

static int hash_audioterminal(struct audioterminal *at)
{
	struct audioterminal *at_old;
	unsigned int h = hashnum(at->termt);

	for (at_old = audioterminals_hash[h]; at_old; at_old = at_old->next)
		if (at_old->termt == at->termt)
			return -1;
	at->next = audioterminals_hash[h];
	audioterminals_hash[h] = at;
	return 0;
}

static int hash_audioterminals(void)
{
	int r = 0, i, k;

	for (i = 0; audioterminals[i].name; i++)
	{
		k = hash_audioterminal(&audioterminals[i]);
		if (k < 0)
			r = k;
	}

	return r;
}

static int hash_videoterminal(struct videoterminal *vt)
{
	struct videoterminal *vt_old;
	unsigned int h = hashnum(vt->termt);

	for (vt_old = videoterminals_hash[h]; vt_old; vt_old = vt_old->next)
		if (vt_old->termt == vt->termt)
			return -1;
	vt->next = videoterminals_hash[h];
	videoterminals_hash[h] = vt;
	return 0;
}

static int hash_videoterminals(void)
{
	int r = 0, i, k;

	for (i = 0; videoterminals[i].name; i++)
	{
		k = hash_videoterminal(&videoterminals[i]);
		if (k < 0)
			r = k;
	}

	return r;
}

static int hash_genericstrtable(struct genericstrtable *t[HASHSZ],
			       struct genericstrtable *g)
{
	struct genericstrtable *g_old;
	unsigned int h = hashnum(g->num);

	for (g_old = t[h]; g_old; g_old = g_old->next)
		if (g_old->num == g->num)
			return -1;
	g->next = t[h];
	t[h] = g;
	return 0;
}

#define HASH_EACH(array, hash) \
	for (i = 0; array[i].name; i++) { \
		k = hash_genericstrtable(hash, &array[i]); \
		if (k < 0) { \
			r = k; \
		}\
	}

static int hash_tables(void)
{
	int r = 0, k, i;

	k = hash_audioterminals();
	if (k < 0)
		r = k;

	k = hash_videoterminals();
	if (k < 0)
		r = k;

	HASH_EACH(hiddescriptors, hiddescriptors_hash);

	HASH_EACH(reports, reports_hash);

	HASH_EACH(huts, huts_hash);

	HASH_EACH(hutus, hutus_hash);

	HASH_EACH(langids, langids_hash);

	HASH_EACH(physdess, physdess_hash);

	HASH_EACH(biass, biass_hash);

	HASH_EACH(countrycodes, countrycodes_hash);

	HASH_EACH(classes, classes_hash);

	HASH_EACH(subclasses, subclasses_hash);

	HASH_EACH(protocols, protocols_hash);

	HASH_EACH(vendors, vendors_hash);

	return r;
}

/* ---------------------------------------------------------------------- */

/*
static void print_tables(void)
{
	int i;
	struct audioterminal *at;
	struct videoterminal *vt;
	struct genericstrtable *li;
	struct genericstrtable *hu;
	struct genericstrtable *cl;
	struct genericstrtable *sc;
	struct genericstrtable *pr;
	struct genericstrtable *ve;


	printf("--------------------------------------------\n");
	printf("\t\t Audio Terminals\n");
	printf("--------------------------------------------\n");

	for (i = 0; i < HASHSZ; i++) {
		printf("hash: %d\n", i);
		at = audioterminals_hash[i];
		for (; at; at = at->next)
			printf("\tentry: %s\n", at->name);
	}

	printf("--------------------------------------------\n");
	printf("\t\t Video Terminals\n");
	printf("--------------------------------------------\n");

	for (i = 0; i < HASHSZ; i++) {
		printf("hash: %d\n", i);
		vt = videoterminals_hash[i];
		for (; vt; vt = vt->next)
			printf("\tentry: %s\n", vt->name);
	}

	printf("--------------------------------------------\n");
	printf("\t\t Languages\n");
	printf("--------------------------------------------\n");

	for (i = 0; i < HASHSZ; i++) {
		li = langids_hash[i];
		if (li)
			printf("hash: %d\n", i);
		for (; li; li = li->next)
			printf("\tid: %x, entry: %s\n", li->num, li->name);
	}

	printf("--------------------------------------------\n");
	printf("\t\t Country Codes\n");
	printf("--------------------------------------------\n");

	for (i = 0; i < HASHSZ; i++) {
		hu = countrycodes_hash[i];
		if (hu)
			printf("hash: %d\n", i);
		for (; hu; hu = hu->next)
			printf("\tid: %x, entry: %s\n", hu->num, hu->name);
	}

	printf("--------------------------------------------\n");
	printf("\t\t Classes\n");
	printf("--------------------------------------------\n");

	for (i = 0; i < HASHSZ; i++) {
		cl = classes_hash[i];
		if (cl)
			printf("hash: %d\n", i);
		for (; cl; cl = cl->next)
			printf("\tid: %x, entry: %s\n", cl->num, cl->name);
	}

	printf("--------------------------------------------\n");
	printf("\t\t Subclasses\n");
	printf("--------------------------------------------\n");

	for (i = 0; i < HASHSZ; i++) {
		sc = subclasses_hash[i];
		if (sc)
			printf("hash: %d\n", i);
		for (; sc; sc = sc->next)
			printf("\tid: %x, entry: %s\n", sc->num, sc->name);
	}

	printf("--------------------------------------------\n");
	printf("\t\t Protocols\n");
	printf("--------------------------------------------\n");

	for (i = 0; i < HASHSZ; i++) {
		pr = protocols_hash[i];
		if (pr)
			printf("hash: %d\n", i);
		for (; pr; pr = pr->next)
			printf("\tid: %x, entry: %s\n", pr->num, pr->name);
	}

	printf("--------------------------------------------\n");
	printf("\t\t Vendors\n");
	printf("--------------------------------------------\n");

	for (i = 0; i < HASHSZ; i++) {
		ve = vendors_hash[i];
		if (ve)
			printf("hash: %d\n", i);
		for (; ve; ve = ve->next)
			printf("\tid: %x, entry: %s\n", ve->num, ve->name);
	}

	printf("--------------------------------------------\n");
}
*/

int names_init(void)
{
	int r;

#ifdef USE_UDEV
	udev = udev_new();
	if (!udev)
		r = -1;
	else {
		hwdb = udev_hwdb_new(udev);
		if (!hwdb)
			r = -1;
	}
#endif

	r = hash_tables();

	return r;
}

void names_exit(void)
{
#ifdef USE_UDEV
	hwdb = udev_hwdb_unref(hwdb);
	udev = udev_unref(udev);
#endif
}
