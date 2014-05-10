#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
#include <libusb.h>

#include "config.h"
#include "list.h"
#include "lsusb.h"
#include "names.h"
#include "usbmisc.h"

#define MY_STRING_MAX 256
#define MY_PATH_MAX 4096
#define MY_PARAM_MAX 64

#define SBUD "/sys/bus/usb/devices/"

static void sort_dev_strings(char *dev_strings[],  int cnt )
{
	if ( cnt < 2 )
		return;
	int sorted = 0;
	int i = 0;
	while (!(sorted)) {
		sorted = 1;
		for (i=0; i<cnt-1; i++) {
			if ( strtoul(dev_strings[i],NULL,16) > strtoul(dev_strings[i+1],NULL,16) ) {
				char tmp[MY_PATH_MAX];
				sorted = 0;
				strncpy(tmp,dev_strings[i],MY_PATH_MAX-1);
				strncpy(dev_strings[i],dev_strings[i+1],MY_PATH_MAX-1);
				strncpy(dev_strings[i+1],tmp,MY_PATH_MAX-1);
				break;
			}
		}
	}
}

static unsigned int get_location_id(libusb_device *dev) {
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

static unsigned int get_maxchild(unsigned int location_id)
{
/* nNbrPorts in Hub Descriptor */
	unsigned int maxchild;
	/* build path */
	unsigned int mask;
	unsigned int busnum;
	char buf[MY_PARAM_MAX];
	char path[MY_PATH_MAX];
	int fd, i;
	ssize_t r;

	strcpy(path, SBUD);

	busnum = location_id >> 24;
	mask = 0x00ffffff;
	location_id &= mask;
	if ( location_id == 0 )
	{
		/* root hub is special case */
		snprintf(path + strlen(path), sizeof(path) - strlen(path), "%s", "usb");

	}
	snprintf(path + strlen(path), sizeof(path) - strlen(path), "%d", busnum);

	for (i=0; i<6; i++) {
		int next = location_id >> (20 - i*4);
		if (!next)
			break;
		snprintf(path + strlen(path), sizeof(path) - strlen(path), "%s%d",  ((i>0) ? "." : "-" ), next);

		mask >>= 4;
		location_id &= mask;
	}
	snprintf(path + strlen(path), sizeof(path) - strlen(path), "%s", "/maxchild");

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		return 0;
	}
	r = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (r < 0) {
		perror(path);
		return 0;
	}
	buf[sizeof(buf) - 1] = '\0';
	maxchild = (unsigned int)strtoul(buf, NULL, 10);
	return maxchild;
}


static void get_driver(char **driver, unsigned int location_id, int ifnum)
{
	/* build path */
	unsigned int mask;
	unsigned int busnum;

	int l;
	char path[PATH_MAX];
	char newpath[PATH_MAX];
	int i;

	strcpy(path, SBUD);
	strcpy(*driver, "Unknown");

	busnum = location_id >> 24;
	mask = 0x00ffffff;
	location_id &= mask;
	if ( location_id == 0 ) {
		/* root hub is special case */
		snprintf(path + strlen(path), sizeof(path) - strlen(path), "usb%d", busnum);
		if (strlen(path) < PATH_MAX) {
			l = readlink(path, newpath, PATH_MAX);
			if (l >= 0) {
				if (l < PATH_MAX - 1)
					newpath[l] = '\0';
				else
					newpath[0] = '\0';
				char *p = strrchr(newpath, '/');
				if (p)
					*p = '\0';

				snprintf(path, sizeof(path), "%s/%s/driver", "/sys/bus/usb/devices/", newpath);

				goto read_driver;
			}
		}
	}
	/* not root hub */
	snprintf(path + strlen(path), sizeof(path) - strlen(path), "%d", busnum);
	for (i=0; i<6; i++) {
		int next = location_id >> (20 - i*4);
		if (!next)
			break;
		snprintf(path + strlen(path), sizeof(path) - strlen(path), "%s%d", ((i>0) ? "." : "-" ), next);
		mask >>= 4;
		location_id &= mask;
	}
	snprintf(path + strlen(path), sizeof(path) - strlen(path), ":1.%d/driver", ifnum);
read_driver:
	if (strlen(path) < PATH_MAX) {
		l = readlink(path, newpath, PATH_MAX);
		if (l >= 0) {
			if (l < PATH_MAX - 1)
				newpath[l] = '\0';
			else
				newpath[0] = '\0';
			char *p = strrchr(newpath, '/');
			if (p)
				snprintf(*driver, MY_STRING_MAX, "%s", p + 1);
		}
	}
}


static void build_dev_strings(libusb_device **devs, char *dev_strings[],  int cnt )
{
/*
 * Build a formatted line to be sorted for the tree.
 * The LocationID has the tree structure (0xbbdddddd):
 *   0x  -- always
 *   bb  -- bus number in hexadecimal (use ones complement to sort)
 *   dddddd -- up to six levels for the tree, each digit represents its
 *             position on that level
 *
 * So we start each line with the LocationID for sorting purposes, then append the rest of the line as we want it
 * Later, we will do the sort, then strip off the LocationID for display purposes
 */
	libusb_device *dev;
	unsigned int location_id;
	int i = 0, j = 0;

	for (i=0;i<cnt;i++) {
		struct libusb_device_descriptor desc;
		dev = devs[i];
		location_id = get_location_id(dev);
		char portnum[2];
		unsigned int busnum;
		unsigned int devnum;
		unsigned int maxchild;
		char * driver = malloc(MY_STRING_MAX);
		char speed[MY_PARAM_MAX];	/* '1.5','12','480','5000' + '\n' */
		char spaces[MY_STRING_MAX];
		char tmp[12];
		int scmp;

		sprintf(tmp, "%06x", location_id & 0x00ffffff);
		scmp = strchr(tmp,'0') - tmp;
		if (scmp < 0)
			scmp = strlen(tmp);
		strcpy(spaces,"");
		if (scmp == 0) {
			strcpy(portnum,"1");
			strcpy(spaces,"/:  ");
		} else {
			strncpy(portnum,&tmp[scmp-1],1);
			portnum[1] = '\0';
			for (j=0; j<scmp; j++) {
				snprintf(spaces + strlen(spaces), sizeof(spaces) - strlen(spaces), "%s", "    ");
			}
			snprintf(spaces + strlen(spaces), sizeof(spaces) - strlen(spaces), "%s", "|__ ");

		}
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			fprintf(stderr, "failed to get device descriptor");
			return;
		}
		busnum = libusb_get_bus_number(dev);
		devnum = libusb_get_device_address(dev);
		switch (libusb_get_device_speed(dev)) {
		case LIBUSB_SPEED_LOW:      	strcpy(speed, "1.5"); break;
		case LIBUSB_SPEED_FULL:     	strcpy(speed, "12"); break;
		case LIBUSB_SPEED_HIGH:     	strcpy(speed, "480"); break;
		case LIBUSB_SPEED_SUPER:	strcpy(speed, "5000"); break;
		default:			strcpy(speed, "Unknown"); break;
		}
		if (scmp == 0) {
			/* for root hub */
			maxchild = get_maxchild(location_id);
			get_driver(&driver, location_id, 0);
			/* invert busnum part of location_id for sorting purposes */
			snprintf(dev_strings[i], MY_PATH_MAX,
				"0x%08x %sBus %02u.Port %s: Dev %u, Class=root_hub, Driver=%s/%up, %sM\n",
				location_id ^ 0xff000000, spaces, busnum, portnum, devnum, driver, maxchild, speed);
		} else {
			for (j = 0; j < desc.bNumConfigurations; ++j) {
				struct libusb_config_descriptor *config;
				int ifnum;
				int ret = libusb_get_config_descriptor(dev, j, &config);
				if (ret) {
					fprintf(stderr, "Couldn't get configuration "
							"descriptor %d, some information will "
							"be missing\n", j);
					continue;
				}
				for (ifnum = 0 ; ifnum < config->bNumInterfaces ; ifnum++) {
					const struct libusb_interface *intf;
					intf = &config->interface[ifnum];
					const struct libusb_interface_descriptor *alt;
					alt = &intf->altsetting[0];
					char ifcls[MY_PARAM_MAX];
					get_class_string(ifcls, sizeof(ifcls), alt->bInterfaceClass);
					maxchild = get_maxchild(location_id);
					get_driver(&driver, location_id, ifnum);
					if(ifnum == 0) {
						/* invert busnum part of location_id for sorting purposes */
						sprintf(tmp,"0x%08x ", location_id ^ 0xff000000);
						strcpy(dev_strings[i], tmp);
					}
					/* put all the interfaces together into one long line for
					 * sorting purposes, and leave room for the final newline */
					if (alt->bInterfaceClass == 9) {
						snprintf(dev_strings[i] + strlen(dev_strings[i]), MY_PATH_MAX - strlen(dev_strings[i]),
								"%sPort %s: Dev %u, If %u, Class=%s, Driver=%s"
								"/%up"
								", %sM%s",
								spaces, portnum, devnum, ifnum, ifcls, driver,
								maxchild,
 								speed, ((ifnum < config->bNumInterfaces - 1 ) ? "\n" : "" ));
					} else {
						snprintf(dev_strings[i] + strlen(dev_strings[i]), MY_PATH_MAX - strlen(dev_strings[i]),
								"%sPort %s: Dev %u, If %u, Class=%s, Driver=%s"
								", %sM%s",
								spaces, portnum, devnum, ifnum, ifcls, driver,
								speed, ((ifnum < config->bNumInterfaces - 1 ) ? "\n" : "" ));
					}
				}
				snprintf(dev_strings[i] + strlen(dev_strings[i]), MY_PATH_MAX - strlen(dev_strings[i]), "%s", "\n");
				libusb_free_config_descriptor(config);
			}
		}
		free(driver);
	}
	return;
}

int lsusb_t(void)
{
	libusb_device **devs;
	int r, i;
	ssize_t cnt;

	r = libusb_init(NULL);
	if (r < 0)
		return r;
	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
		return cnt;
	char *strings[cnt];
	for (i=0; i<cnt; i++) {
		strings[i] = malloc(MY_PATH_MAX);
		strings[i][0] = '\0';
	}

	build_dev_strings(devs, strings,  cnt);
	/* Sort by Location ID */
	sort_dev_strings(strings, cnt);

	/* Now strip off leading Location ID and print to stdout */
	for (i=0;i<cnt;i++) {
		char *p = strchr(strings[i],' ');
		if (p)
			snprintf(strings[i], MY_PATH_MAX, "%s", p + 1);
		printf("%s",strings[i]);
		free(strings[i]);
	}

	libusb_free_device_list(devs, 1);
	libusb_exit(NULL);
	return 0;
}
