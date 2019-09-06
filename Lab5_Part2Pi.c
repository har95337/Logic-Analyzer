#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <wiringPi.h>
#include <errno.h>
#include <libusb.h>

int main (int argc, char * argv[]){
	libusb_device_handle* psoc;	// the PSoC pointer

	char go[64]; // Start transfer block
	char rx_data[64]; // Receive data block
	int tx;	// Bytes transmitted
	int rx;	// Bytes received
	int return_val;

	libusb_init(NULL); // Initialize the LIBUSB library

	//Open the Cypress PSoC with the given IDs
	psoc = libusb_open_device_with_vid_pid(NULL, 0x04B4, 0x8051);

	if (psoc == NULL)
	{
		perror("device not  found\n");
	}

	// If the reset fails then kill the program
	if (libusb_reset_device(psoc) != 0)
	{
		perror("Device reset failed\n");
	}	

	if(libusb_set_configuration(psoc, 1) != 0)
	{
		perror("Set configuration failed\n");
	}
	// Claim the interface. This step is needed before any I/Os can be issued to the USB device.
	if (libusb_claim_interface(psoc, 0)!=0)
	{
		perror("Cannot claim interface");
	}

	// Set up data block	
	for (int i=0; i<64; i++) go[i] = i;
	// Clear receive buffer
	for (int i=0; i<64; i++) rx_data[i] = 0;

	//The go signal is a random data block that is sent to the PSoC simply to start the program
	return_val = libusb_bulk_transfer(psoc,	0x02, go, 64, &tx, 0);
	if(return_val < 0) perror("write error!");
	
	//Transfer/receive data forever
	while(1)
	{
		//Receive data from the PSoC and transfer the same data immediately.
		return_val = libusb_bulk_transfer(psoc, (0x01 |	0x80), rx_data,	64, &rx, 0);
		if(return_val < 0) perror("read error!");
		return_val = libusb_bulk_transfer(psoc, 0x02, rx_data, 64, &tx, 0);
		if(return_val < 0) perror("write error!");
	}
	libusb_close(psoc);
}




