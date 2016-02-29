/*
 * minimal, experimental program to read EDID from a Fresco Logic FL2000
 *
 * Copyright (C) 2014 cy384
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 * 
 * 20480 + 20480 + 14076 = 55036 bytes as a "frame" or "tile" or whatever
 * 640 x 480 = 307200 pixels
 * 
 * 307200 / 55036 = 5.58180100298 byte / pixel
 * 
 * if we would have only 51200 bytes as rgb the ratio would be 6, a nice and round number
 * 
 * we have 55036 - 51200 = 3836 bytes "too much". This is probably timing and padding and shit then?
 * approximately 7% is magic.
 * 
 * gcc fl2000_poke_everything.c -o fl2000_poke_everything -I/usr/include/libusb-1.0 -lusb-1.0 -Wall -DDEBUG
 * 
 * from 15271 in dump we have some control packets, changing resolution from 640x480 to 800x600 here?
 */

#include <stdio.h>
#include <sys/types.h>
#include <libusb.h>
#include <unistd.h>

/* for control stuff, we only really work with four bytes at a time, but w/e */
#define RESPONSE_BUFFER_SIZE 512

/* convenience macros */
#define EXPECT_FOUR_BYTES \
if (bytes_back != 4) \
{ \
	fprintf(stderr, "%d bytes from transfer?\n", bytes_back); \
	ret = bytes_back; \
	goto cleanup; \
}

#define PRINT_FOUR_BYTES(wIndex, readwrite) \
printf("%d %c: ", wIndex, readwrite); print_data(data);

#define SILENT_TRANSFER_OUT(wIndex) \
bytes_back = libusb_control_transfer(fl2000_handle, 0x40, 65, 0, wIndex, data, 4, 0); \
EXPECT_FOUR_BYTES;

#define SILENT_TRANSFER_IN(wIndex) \
bytes_back = libusb_control_transfer(fl2000_handle, 0xc0, 64, 0, wIndex, data, 4, 0); \
EXPECT_FOUR_BYTES;

#define SET_DATA(byte0, byte1, byte2, byte3) \
data[0] = byte0; data[1] = byte1; data[2] = byte2; data[3] = byte3;

#define DATA_EQ(byte0, byte1, byte2, byte3) \
(byte0 == data[0] && byte1 == data[1] && byte2 == data[2] && byte3 == data[3])

/* make all transfers silent except in debug mode */
#ifdef DEBUG
#define DEBUG 1
#else
#define DEBUG 0
#endif

#if DEBUG

#define TRANSFER_IN(wIndex) SILENT_TRANSFER_IN(wIndex) \
EXPECT_FOUR_BYTES; PRINT_FOUR_BYTES(wIndex, 'R');

#define TRANSFER_OUT(wIndex) SILENT_TRANSFER_OUT(wIndex) \
EXPECT_FOUR_BYTES; PRINT_FOUR_BYTES(wIndex, 'W');

#else

#define TRANSFER_IN(wIndex) SILENT_TRANSFER_IN(wIndex)
#define TRANSFER_OUT(wIndex) SILENT_TRANSFER_OUT(wIndex)

#endif

void print_data(uint8_t* data)
{
	int i;
	for (i = 0; i < 4; i++) printf("%.2x ", data[i]);
	printf("\n");
}

int main(void)
{
	int ret = 0;
	int bytes_back = 0;
	uint8_t edid_offset = 0;
	uint8_t data[RESPONSE_BUFFER_SIZE] = {0};
	libusb_device_handle* fl2000_handle = 0;

	if (DEBUG) printf ("compiled in debug mode\n");

	ret = libusb_init(NULL);
	if (ret < 0) goto cleanup;

	fl2000_handle = libusb_open_device_with_vid_pid(0, 0x1d5c, 0x2000);
	if (!fl2000_handle)
	{
		fprintf(stderr, "couldn't find an fl2000 device\n");
		goto cleanup;
	}

	ret = libusb_set_configuration(fl2000_handle, 1);
	if (ret < 0) goto cleanup;

	ret = libusb_claim_interface(fl2000_handle, 1);
	if (ret < 0) goto cleanup;

	ret = libusb_set_interface_alt_setting(fl2000_handle, 1, 0);
	if (ret < 0) goto cleanup;

	ret = libusb_claim_interface(fl2000_handle, 2);
	if (ret < 0) goto cleanup;

	ret = libusb_set_interface_alt_setting(fl2000_handle, 2, 0);
	if (ret < 0) goto cleanup;

	TRANSFER_IN(32800);

	/* either for priming the interrupt or the EDID stuff? */
	while (!DATA_EQ(0xcc, 0x00, 0x00, 0x8f))
	{
		SET_DATA(0xcc, 0x00, 0x00, 0x10);
		SILENT_TRANSFER_OUT(32800);
		SILENT_TRANSFER_IN(32800);
	}

	/* reset the monitor attachment thing? */
	SET_DATA(0xe1, 0x00, 0x00, 0x00);
	TRANSFER_OUT(32768);
	TRANSFER_IN(32768);

	if (!DATA_EQ(0x00, 0x00, 0x00, 0x00))
	{
		fprintf(stderr, "did you leave your monitor attached?\n");
	}

	/* check for monitor; if none, wait for attach interrupt */
	TRANSFER_IN(32768);
	if (DATA_EQ(0x00, 0x00, 0x00, 0x00))
	{
		bytes_back = 0;
		ret = libusb_interrupt_transfer(fl2000_handle, 0x83, data, 1, &bytes_back, 0);

		if (ret != 0)
		{
			goto cleanup;
		}

		if (bytes_back != 1)
		{
			fprintf(stderr, "%d bytes from interrupt?\n", bytes_back);
		}
	}

	/* now begin the real EDID config and transfer */
	/*
	SET_DATA(0xd0, 0x00, 0x00, 0xcf);
	TRANSFER_OUT(32800);
	TRANSFER_IN(32800);

	SET_DATA(0xd0, 0x00, 0x00, 0x48);
	TRANSFER_OUT(32800);
	TRANSFER_IN(32800);

	SET_DATA(0x00, 0x00, 0x00, 0x00);

	for (edid_offset = 4; edid_offset < 132; edid_offset += 4)
	{
		while (!DATA_EQ(0xd0, edid_offset, 0x00, 0xc0))
		{
			SET_DATA(0xd0, edid_offset, 0x00, 0xc0);
			SILENT_TRANSFER_OUT(32800);
			SILENT_TRANSFER_IN(32800);
		}

		TRANSFER_IN(32804);

		if (!DEBUG)
		{
			int i;
			for (i = 0; i < 4; i++) printf("%c", data[i]);
		}

		SET_DATA(0xd0, edid_offset, 0x00, 0x50);
		SILENT_TRANSFER_OUT(32800);
		SILENT_TRANSFER_IN(32800);
	}
*/

	//to start with, let's just replay packets
	//
	#include "640x480/115.c"
	#include "640x480/116.c"
	#include "640x480/117.c"
	unsigned char __118_bin[] = {};
	unsigned int __118_bin_len = 0;

//dev_handle		a handle for the device to communicate with
//endpoint		the address of a valid endpoint to communicate with
//data			a suitably-sized data buffer for either input or output (depending on endpoint)
//length			for bulk writes, the number of bytes from data to be sent. for bulk reads, the maximum number of bytes to receive into the data buffer.
//transferred		output location for the number of bytes actually transferred.
//timeout			timeout (in millseconds) that this function should wait before giving up due to no response being received. For an unlimited timeout, use value 0.
/*
	//this wakes up the screen but doesn't show anything
	for(;;){
	libusb_bulk_transfer(fl2000_handle, 0x01, __115_bin, __115_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	libusb_bulk_transfer(fl2000_handle, 0x01, __116_bin, __116_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	libusb_bulk_transfer(fl2000_handle, 0x01, __117_bin, __117_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	libusb_bulk_transfer(fl2000_handle, 0x01, __118_bin, __118_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
}*/


//Let's try some magic control packets which supposedly changes res from 640x480 to 800x600
//Ignore the "in" packets because they don't sound cool (no writing?)
//I should probably read som USB papers

	TRANSFER_IN(32840);//15271
	SET_DATA(0x04, 0x80, 0x7d, 0x28);
	TRANSFER_OUT(32840);
	
	TRANSFER_IN(32828);
	
	SET_DATA(0x4d, 0x08, 0x01, 0xd4);
	TRANSFER_OUT(32828);
	
	SET_DATA(0x10, 0x62, 0x7f, 0x00);
	TRANSFER_OUT(32812);
	
	TRANSFER_IN(32840);
	
	SET_DATA(0x04, 0x80, 0x7d, 0x28);
	TRANSFER_OUT(32840);
	TRANSFER_IN(32812);
	TRANSFER_IN(32828);
	
	SET_DATA(0x4d, 0x08, 0x01, 0xc4);
	TRANSFER_OUT(32828);//15289
	TRANSFER_IN(32828);
	
	SET_DATA(0x4d, 0x08, 0x01, 0xd4);
	TRANSFER_OUT(32828);
	TRANSFER_IN(32772);//15295
	
	SET_DATA(0xe1, 0x00, 0x00, 0x00);
	TRANSFER_OUT(32772);
	TRANSFER_IN(32772);
	
	SET_DATA(0x10, 0x03, 0x10, 0x00);
	TRANSFER_OUT(32772);//15297
	TRANSFER_IN(32772);
	
	SET_DATA(0x9d, 0x03, 0x10, 0x81);
	TRANSFER_OUT(32772);//15301
	
	SET_DATA(0x20, 0x04, 0x20, 0x03);
	TRANSFER_OUT(32776);
	TRANSFER_IN(32776);
	
	SET_DATA(0xd9, 0x00, 0x80, 0x00);
	TRANSFER_OUT(32780);
	TRANSFER_IN(32780);
	
	SET_DATA(0x74, 0x02, 0x58, 0x02);
	TRANSFER_OUT(32784);
	TRANSFER_IN(32784);
	
	SET_DATA(0x1c, 0x00, 0xc4, 0x01);
	TRANSFER_OUT(32788);
	TRANSFER_IN(32788);
	TRANSFER_IN(32796);
	
	SET_DATA(0x00, 0x00, 0x00, 0x00);
	TRANSFER_OUT(32796);
	TRANSFER_IN(112);
	
	SET_DATA(0x85, 0x40, 0x18, 0x04);
	TRANSFER_OUT(112);
	// so far nothing happens :(
	// but screen stays on after this!!! even after program has ended!! whoop whoop
	
	unsigned char __zero_bin[] = {};
	unsigned int __zero_bin_len = 0;
	
	for(;;){
	#include "after_control/15327.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15327_bin, __15327_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	#include "after_control/15328.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15328_bin, __15328_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	#include "after_control/15329.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15329_bin, __15329_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	libusb_bulk_transfer(fl2000_handle, 0x01, __zero_bin, __zero_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	
	#include "after_control/15335.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15335_bin, __15335_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	#include "after_control/15336.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15336_bin, __15336_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	#include "after_control/15337.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15337_bin, __15337_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	libusb_bulk_transfer(fl2000_handle, 0x01, __zero_bin, __zero_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	
	#include "after_control/15343.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15343_bin, __15343_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	#include "after_control/15344.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15344_bin, __15344_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	#include "after_control/15345.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15345_bin, __15345_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	libusb_bulk_transfer(fl2000_handle, 0x01, __zero_bin, __zero_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	
	#include "after_control/15351.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15351_bin, __15351_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	#include "after_control/15352.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15352_bin, __15352_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	#include "after_control/15353.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15353_bin, __15353_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	libusb_bulk_transfer(fl2000_handle, 0x01, __zero_bin, __zero_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	
	
		#include "after_control/15359.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15359_bin, __15359_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	#include "after_control/15360.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15360_bin, __15360_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	#include "after_control/15361.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15361_bin, __15361_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	libusb_bulk_transfer(fl2000_handle, 0x01, __zero_bin, __zero_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	
		#include "after_control/15367.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15367_bin, __15367_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	#include "after_control/15368.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15368_bin, __15368_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	#include "after_control/15369.bin.c"
	libusb_bulk_transfer(fl2000_handle, 0x01, __15369_bin, __15369_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
	
	libusb_bulk_transfer(fl2000_handle, 0x01, __zero_bin, __zero_bin_len, &bytes_back, 5000);
	fprintf(stderr, "%d bytes from bulk\n", bytes_back);
}
	
	
	
	
	sleep(10);



	/* clean up before exiting */
	cleanup:

	/* is there a way to check for open interfaces? w/e */	
	if (fl2000_handle)
	{
		libusb_release_interface(fl2000_handle, 1);
		libusb_release_interface(fl2000_handle, 2);
		/* do we need to set configuration back to 0? not really */
		libusb_set_configuration(fl2000_handle, 0);
		libusb_close(fl2000_handle);
	}

	libusb_exit(NULL);
	if (ret < 0) fprintf(stderr, "%s (%d)\n", libusb_error_name(ret), ret);
	return ret;
}

