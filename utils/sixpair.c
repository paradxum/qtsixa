/*
 * sixpair.c version 2007-04-18
 * Compile with: gcc -o sixpair sixpair.c -lusb
 */

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <usb.h>

#define VENDOR 0x054c
#define PRODUCT 0x0268

#define USB_DIR_IN 0x80
#define USB_DIR_OUT 0

void fatal(char *msg) {
    perror(msg);
    exit(1);
}

void show_master(usb_dev_handle *devh, int itfnum) {
    printf("Current Bluetooth master address on Controller   : ");
    unsigned char msg[8];
    int res = usb_control_msg
              (devh, USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
               0x01, 0x03f5, itfnum, (void*)msg, sizeof(msg), 5000);
    if ( res < 0 ) {
        perror("USB_REQ_GET_CONFIGURATION");
        return;
    }
    printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
           msg[2], msg[3], msg[4], msg[5], msg[6], msg[7]);
}
static char* get_bdaddr(usb_dev_handle *devh, int itfnum){
	unsigned char msg[17];
	char* address = malloc(20);
	int res;

	res = usb_control_msg
		(devh,USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		0x01, 0x03f2, itfnum, (void*) msg, sizeof(msg), 5000);
	if (res < 0) {
		perror("Getting the device Bluetooth address failed");
		return NULL;
		}
	sprintf(address,"%02x:%02x:%02x:%02x:%02x:%02x",msg[4], msg[5], msg[6], msg[7], msg[8], msg[9]);
	return address;
	}

void set_master(usb_dev_handle *devh, int itfnum, int *mac) {
    printf("Setting Bluetooth master address on Controller to: ");
    printf("%02x:%02x:%02x:%02x:%02x:%02x\n",mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    char msg[8]= { 0x01, 0x00, mac[0],mac[1],mac[2],mac[3],mac[4],mac[5] };
    int res = usb_control_msg
              (devh,
               USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
               0x09,
               0x03f5, itfnum, msg, sizeof(msg),
               5000);
    if ( res < 0 ) fatal("USB_REQ_SET_CONFIGURATION");
}

void process_device(struct usb_device *dev, struct usb_config_descriptor *cfg, 
						int itfnum, int *mac, char *paired_dev_filename) {
    char* address;

    usb_dev_handle *devh = usb_open(dev);
    if ( ! devh ) fatal("usb_open");

    usb_detach_kernel_driver_np(devh, itfnum);

    int res = usb_claim_interface(devh, itfnum);
    if ( res < 0 ) fatal("usb_claim_interface");

    address = get_bdaddr(devh, itfnum);
    printf("Controller Found - Bluetooth address: %s\n", address);

    show_master(devh, itfnum);

    set_master(devh, itfnum, mac);

    usb_close(devh);
    printf("Master address successfully stored in controller.\n");

    if(paired_dev_filename && paired_dev_filename != '\0'){
	    FILE *pdfh;
	    pdfh = fopen(paired_dev_filename, "a");
	    if(pdfh == NULL){
		    perror("Opening paired devices file failed");
	    }
	    else{
		    //fprintf(pdfh,"%02x:%02x:%02x:%02x:%02x:%02x\n",mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		    fprintf(pdfh,"%s\n",address);
		    printf("Controller address successfully stored in paired devices file.\n");
	    }
	    fclose(pdfh);
    }
}

void find_master(int *mac){
	FILE *f = popen("hcitool dev", "r");
	if ( !f ||
			fscanf(f, "%*s\n%*s %x:%x:%x:%x:%x:%x",
				&mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]) != 6 ) {
		printf("Unable to retrieve local bd_addr from `hcitool dev`.\n");
		printf("Please enable Bluetooth or specify an address manually.\n");
		exit(1);
	}
	pclose(f);
}

void print_usage(char **argv){
	printf("usage: %s [-n] [-m <bd_addr of master>] [-f <paired devices file>]\n"
"	-n = Don't save to paired devices file\n"
"	-m = instead of searching for an adapter, set this address in the controller\n"
"	-f = Use this paird devices file instead of the default\n", argv[0]);
	exit(EXIT_FAILURE);
}

void process_args(int argc, char **argv, int *mac, char **paired_dev_filename){ 
	// Maintain backwards compatability but complain about it
    	if ( argc == 2 && sscanf(argv[1], "%x:%x:%x:%x:%x:%x",&mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]) == 6 ) {
		*paired_dev_filename = strdup("/var/lib/bluetooth/paired_ps3_controllers");
		printf("NOTIE: Using old format.. you really should add -m\n"
			"usage: %s [-n] [-m <bd_addr of master>] [-f <paired devices file>]\n\n",argv[0]);
		return;
        }

	extern char *optarg;
	int opt;

	int macFound;
	int saveToFile;
	char *tmp_paired_dev_filename = 0;
	
	macFound = 0;
	saveToFile = 1;
	while ((opt = getopt(argc, argv, "nm:f:")) != -1) {
		switch (opt) {
			case 'n':
				saveToFile = 0;
				break;
			case 'f':
				tmp_paired_dev_filename = optarg;
				break;
			case 'm':
        		if ( sscanf(optarg, "%x:%x:%x:%x:%x:%x",
                    &mac[0],&mac[1],&mac[2],&mac[3],&mac[4],&mac[5]) != 6 ) {
						print_usage(argv);
				}
				macFound=1;
				break;
			default: /* '?' */
				print_usage(argv);
		}
	}
	if( saveToFile == 1){
		if( tmp_paired_dev_filename && tmp_paired_dev_filename != '\0'){
			*paired_dev_filename = strdup(tmp_paired_dev_filename);
		}
		else{
			// TODO: Allow compiler default filename here
			*paired_dev_filename = strdup("/var/lib/sixad/paired_ps3_controllers");
		}
	}
	if( macFound != 1){
		find_master(mac);
	}
}

int main(int argc, char *argv[]) {

    usb_init();
    if ( usb_find_busses() < 0 ) fatal("usb_find_busses");
    if ( usb_find_devices() < 0 ) fatal("usb_find_devices");
    struct usb_bus *busses = usb_get_busses();
    if ( ! busses ) fatal("usb_get_busses");

    int mac[6];
	char *pdf_tmp = 0;
	char **paired_dev_filename = &pdf_tmp;
	
	process_args(argc, argv, mac, paired_dev_filename);

    int found = 0;
    struct usb_bus *bus;
    for ( bus=busses; bus; bus=bus->next ) {
        struct usb_device *dev;
        for ( dev=bus->devices; dev; dev=dev->next) {
            struct usb_config_descriptor *cfg;
            for ( cfg = dev->config;
                    cfg < dev->config + dev->descriptor.bNumConfigurations;
                    ++cfg ) {
                int itfnum;
                for ( itfnum=0; itfnum<cfg->bNumInterfaces; ++itfnum ) {
                    struct usb_interface *itf = &cfg->interface[itfnum];
                    struct usb_interface_descriptor *alt;
                    for ( alt = itf->altsetting;
                            alt < itf->altsetting + itf->num_altsetting;
                            ++alt ) {
                        if ( dev->descriptor.idVendor == VENDOR &&
                                dev->descriptor.idProduct == PRODUCT &&
                                alt->bInterfaceClass == 3 ) {
                            process_device(dev, cfg, itfnum, mac, *paired_dev_filename);
                            ++found;
                        }
                    }
                }
            }
        }
    }

    if ( ! found ) printf("No controller found on USB busses.\n");
    return 0;

}
// vim: set ts=4 sw=4:
