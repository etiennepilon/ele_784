/*
 * File         : usbcam.c
 * Description  : ELE784 Lab2 source
 *
 * Etudiants:  JORA09019100 (Alexandru Jora)
 *             MUKM28098503 (Mukandila Mukandila)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <uapi/asm-generic/errno-base.h>
#include <linux/fcntl.h>
#include <uapi/asm-generic/fcntl.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <uapi/asm-generic/ioctl.h>
#include <uapi/asm-generic/int-ll64.h>
#include <linux/usb.h>
#include <linux/completion.h>
#include "usbvideo.h"
#include "dht_data.h"
#include "usbcam.h"

//#define THIS_MODULE 	"usbcam"
//#define KBUILD_MODNAME	"ele784-lab2"

// Module Information
MODULE_AUTHOR("JORA Alexandru, MUKANDILA Mukandila");
MODULE_LICENSE("Dual BSD/GPL");

// Prototypes
static int __init usbcam_init(void);
static void __exit usbcam_exit(void);
static int usbcam_probe (struct usb_interface *intf, const struct usb_device_id *devid);
static void usbcam_disconnect(struct usb_interface *intf);
static int usbcam_open (struct inode *inode, struct file *filp);
static int usbcam_release (struct inode *inode, struct file *filp) ;
static ssize_t usbcam_read (struct file *filp, char __user *ubuf, size_t count, loff_t *f_ops);
static ssize_t usbcam_write (struct file *filp, const char __user *ubuf, size_t count, loff_t *f_ops);
static long usbcam_ioctl (struct file *filp, unsigned int cmd, unsigned long arg);
module_init(usbcam_init);
module_exit(usbcam_exit);

// Private function prototypes
static int urbInit(struct urb *urb, struct usb_interface *intf);
static void urbCompletionCallback(struct urb *urb);


static unsigned int myStatus;
static unsigned int myLength;
static unsigned int myLengthUsed;
static char * myData;

// Structure to keep track of endpoints
typedef struct {
	unsigned int length;
	unsigned char endpoint_direction; // bitmask comparison
	unsigned char endpoint_type; // bitmask comparison
	unsigned int endpoint_max_packet_size;
	unsigned int endpoint_interval_time;
} USB_Endpoint_Info;

// Structure to keep track of interfaces
typedef struct {
	unsigned int interface_number;
	unsigned int num_endpoints;
	unsigned int interface_class;
	unsigned int interface_subclass;
	USB_Endpoint_Info *usb_endpoint_info;
} USB_Interface_Info;

// General data structure for driver
typedef struct {
    struct usb_device *usbdev;
    unsigned int number_interfaces;
    int active_interface;
    USB_Interface_Info *usb_int_info;
    struct urb *myUrb[5];
} USBCam_Dev;

struct class *my_class;

static struct usb_device_id usbcam_table[] = {
// { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
{ USB_DEVICE(0x046d, 0x08cc) },
{ USB_DEVICE(0x046d, 0x0994) },
{}
};
MODULE_DEVICE_TABLE(usb, usbcam_table);

// USB Driver structure
static struct usb_driver usbcam_driver = {
	.name       = "usbcam",
	.id_table   = usbcam_table,
	.probe      = usbcam_probe,
	.disconnect = usbcam_disconnect,
};

// File operation structure
struct file_operations usbcam_fops = {
	.owner          = THIS_MODULE,
	.open           = usbcam_open,
	.release        = usbcam_release,
	.read           = usbcam_read,
	.write          = usbcam_write,
	.unlocked_ioctl = usbcam_ioctl,
};

#define USBCAM_MINOR 0
static struct usb_class_driver usbcam_class = {
	.name       = "usb/usbcam%d",
	.fops       = &usbcam_fops,
	.minor_base = USBCAM_MINOR,
};

static int __init usbcam_init(void) {
    int res;
    printk(KERN_WARNING "===usbcam_init: registering usb device with usbcore\n");
    res = usb_register(&usbcam_driver);
    if(res)
        printk(KERN_WARNING "===usbcam_init: ERROR registering USB device %d\n", res);
    else
        printk(KERN_WARNING "===usbcam_init: device registered with return value: %d\n", res);

    return 0;
}

static void __exit usbcam_exit(void) {
    printk(KERN_WARNING "===usbcam_exit: deregistering usb device\n");
    usb_deregister(&usbcam_driver);
}

static int usbcam_probe (struct usb_interface *intf, const struct usb_device_id *devid) {

    const struct usb_host_interface *interface;
    const struct usb_endpoint_descriptor *endpoint;
    struct usb_device *dev = interface_to_usbdev(intf);
    USBCam_Dev *cam_dev = NULL;
    int n, m;//, altSetNum;
	//int activeInterface = -1;

	// Allocate memory to local driver structure
    cam_dev = kmalloc(sizeof(USBCam_Dev), GFP_KERNEL);

    if(!cam_dev)
    	printk(KERN_WARNING "usbcam_probe: Cannot allocate memory to USBCam_Dev (%s,%s,%u)\n",__FILE__,__FUNCTION__,__LINE__);

    cam_dev->usbdev = usb_get_dev(dev);
    cam_dev->active_interface = -1;

    /*
     * 1) find out the amount of interfaces(configurations) available for the usb_device
     * 2) loop through all the interfaces and store them in our general structure
     *
     * struct usb_interface
     * 		unsigned num_altsetting		- number of alternate settings (interfaces)
     * 		struct usb_host_interface
     * 		struct usb_interface_descriptor desc
     * 			__u8 bNumEndpoints		- accessible as desc.bNumEndpoints
     * 			__u8 bInterfaceNumber	- accessible as desc.bInterfaceNumber
     * 			struct usb_host_endpoint endpoint
     * 				struct usb_endpoint_descriptor desc
     * 					__u8 bEndpointAddress		- bitmask with USB_DIR_IN or USB_DIR_OUT to find out the type of endpoint
     * 					__u8 bmAttributes			- bitmask with USB_ENDPOINT_XFERTYPE_MASK to know if of type: USB_ENDPOINT_XFER_ISOC, USB_ENDPOINT_XFER_BULK or USB_ENDPOINT_XFER_INT
     * 					__le16 wMaxPacketSize		- max size in byes endpoint can handle at once
     * 					__u8 bInterval				- if of type interrupt (USB_ENDPOINT_XFER_INT) value of time between interrupt requests in milliseconds
     */

    cam_dev->number_interfaces = intf->num_altsetting;
    cam_dev->usb_int_info = kmalloc(sizeof(USB_Interface_Info)*cam_dev->number_interfaces, GFP_KERNEL);

    if(!cam_dev->usb_int_info)
    	printk(KERN_WARNING "===usbcam_probe: Cannot allocate memory to USB_Interface_Info (%s,%s,%u)",__FILE__, __FUNCTION__, __LINE__);

    for (n = 0; n < intf->num_altsetting; n++) { // Cycle through the Interfaces

        interface = &intf->altsetting[n];
        cam_dev->usb_int_info[n].interface_number = interface->desc.bInterfaceNumber;
        cam_dev->usb_int_info[n].num_endpoints = interface->desc.bNumEndpoints;
        cam_dev->active_interface = interface->desc.bAlternateSetting;

        //altSetNum = interface->desc.bAlternateSetting;

        if(interface->desc.bInterfaceClass == CC_VIDEO) {

            if(interface->desc.bInterfaceSubClass == SC_VIDEOSTREAMING) {

            	// Save information about Class and SubClass
            	cam_dev->usb_int_info[n].interface_class = interface->desc.bInterfaceClass;
            	cam_dev->usb_int_info[n].interface_subclass = interface->desc.bInterfaceSubClass;

            	// Allocate memory for endpoint information
            	cam_dev->usb_int_info[n].usb_endpoint_info = kmalloc(sizeof(USB_Endpoint_Info)*cam_dev->usb_int_info[n].num_endpoints, GFP_KERNEL);

            	if(!cam_dev->usb_int_info->usb_endpoint_info)
            		printk(KERN_WARNING "usbcam_probe: Cannot allocate memory to USB_Endpoint_Info (%s,%s,%u)\n",__FILE__,__FUNCTION__,__LINE__);

                for(m = 0; m < interface->desc.bNumEndpoints; m++) { // Cycle through the Endpoints

                    endpoint = &interface->endpoint[m].desc;

                    // Get endpoint direction and save it
                    if(endpoint->bEndpointAddress & USB_DIR_IN)
                    	cam_dev->usb_int_info[n].usb_endpoint_info[m].endpoint_direction = endpoint->bEndpointAddress & USB_DIR_IN;
                    else
                    	cam_dev->usb_int_info[n].usb_endpoint_info[m].endpoint_direction = endpoint->bEndpointAddress & USB_DIR_OUT;

                    // Display endpoint type: ISOCHRONOUS, BULK or INTERRUPT
                    switch(endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) {

                    	case USB_ENDPOINT_XFER_ISOC:
                    		printk(KERN_WARNING "===usbcam_probe: endpoint type ISOCHRONOUS\n");
                    		break;
                    	case USB_ENDPOINT_XFER_BULK:
                    		printk(KERN_WARNING "===usbcam_probe: endpoint type BULK\n");
                    		break;
                    	case USB_ENDPOINT_XFER_INT:
                    		printk(KERN_WARNING "===usbcam_probe: endpoint type INTERRUPT\n");
                    		break;
                    	default:
                    		printk(KERN_WARNING "===usbcam_probe: Invalid endpoint type\n");
                    		break;
                    } // end switch

                    cam_dev->usb_int_info[n].usb_endpoint_info[m].endpoint_type = endpoint->bmAttributes;
                    cam_dev->usb_int_info[n].usb_endpoint_info[m].endpoint_max_packet_size = endpoint->wMaxPacketSize;
                    cam_dev->usb_int_info[n].usb_endpoint_info[m].endpoint_interval_time = endpoint->bLength;

                    // Basic interface data
                    printk(KERN_WARNING "===usbcam_probe: endpoint length: %u\n", endpoint->bLength);
                    printk(KERN_WARNING "===usbcam_probe: endpoint descriptor type: %u\n", endpoint->bDescriptorType);
                    printk(KERN_WARNING "===usbcam_probe: endpoint address: %u\n", endpoint->bEndpointAddress);
                    printk(KERN_WARNING "===usbcam_probe: endpoint attributes: %u\n", endpoint->bmAttributes);
                    printk(KERN_WARNING "===usbcam_probe: endpoint max packet size: %lu\n", endpoint->wMaxPacketSize);

                } //end for loop for endpoints

                //activeInterface = altSetNum;
                //break;
            }// end subclass check if
        }// end class check if
    }// end interface for loop.
    printk(KERN_WARNING "===usbcam_probe: Done detecting Interface(s)\n");

    if(cam_dev->active_interface != -1){
        usb_set_intfdata (intf, cam_dev);
        usb_register_dev (intf, &usbcam_class);
        usb_set_interface (dev, interface->desc.bInterfaceNumber, interface->desc.bAlternateSetting);
        printk(KERN_WARNING "===usbcam_probe: all good\n");
        return 0;
    }
    else{
        printk(KERN_WARNING "usbcam_probe: could not associate interface to device\n");
        return -1;
    }
}

void usbcam_disconnect(struct usb_interface *intf) {
    usb_set_intfdata(intf, NULL);
    usb_deregister_dev(intf, &usbcam_class);


}

int usbcam_open (struct inode *inode, struct file *filp) {

	printk(KERN_WARNING "===usbcam_open: Opening usbcam driver!\n");
    return 0;
}

int usbcam_release (struct inode *inode, struct file *filp) {
    return 0;
}

ssize_t usbcam_read (struct file *filp, char __user *ubuf, size_t count, loff_t *f_ops) {
    return 0;
}

ssize_t usbcam_write (struct file *filp, const char __user *ubuf, size_t count, loff_t *f_ops) {
    return 0;
}

long usbcam_ioctl (struct file *filp, unsigned int cmd, unsigned long arg) {
    return 0;
}


// *************************** //
// **** Private functions **** //
// *************************** //

int urbInit(struct urb *urb, struct usb_interface *intf) {
    int i, j, ret, nbPackets, myPacketSize, size, nbUrbs;

    struct usb_host_interface *cur_altsetting = intf->cur_altsetting;
    struct usb_endpoint_descriptor endpointDesc = cur_altsetting->endpoint[0].desc;
    // get interface data structure
    struct USBCam_Dev *cam_dev = usb_get_intfdata(intf);

    nbPackets = 40;  // The number of isochronous packets this urb should contain
    myPacketSize = le16_to_cpu(endpointDesc.wMaxPacketSize);
    size = myPacketSize * nbPackets;
    nbUrbs = 5;

    for (i = 0; i < nbUrbs; ++i) {
        usb_free_urb(cam_dev->myUrb[i]);
        cam_dev->myUrb[i] = usb_alloc_urb(nbPackets, GFP_KERNEL);
        if (myUrb[i] == NULL) {
            // TODO: printk(KERN_WARNING "");
            return -ENOMEM;
        }

        // usb_buffer_alloc renamed to usb_alloc_coherent see linux commit 073900a28d95c75a706bf40ebf092ea048c7b236
        cam_dev->myUrb[i]->transfer_buffer = usb_alloc_coherent(dev, size, GFP_KERNEL, &cam_dev->myUrb[i]->transfer_dma);

        if (myUrb[i]->transfer_buffer == NULL) {
            // printk(KERN_WARNING "");
            usb_free_urb(myUrb[i]);
            return -ENOMEM;
        }

        // initializing isochronous urb by hand
        printk(KERN_WARNING "===usbcam_urbInit: initializing isochronous urb\n");
        cam_dev->myUrb[i]->dev = dev;
        cam_dev->myUrb[i]->context = dev; // *dev* ??
        cam_dev->myUrb[i]->pipe = usb_rcvisocpipe(dev, endpointDesc.bEndpointAddress);
        cam_dev->myUrb[i]->transfer_flags = URB_ISO_ASAP | URB_NO_TRANSFER_DMA_MAP;
        cam_dev->myUrb[i]->interval = endpointDesc.bInterval;
        cam_dev->myUrb[i]->complete = urbCompletionCallback;
        cam_dev->myUrb[i]->number_of_packets = nbPackets;
        cam_dev->myUrb[i]->transfer_buffer_length = size;

        for (j = 0; j < nbPackets; ++j) {
            myUrb[i]->iso_frame_desc[j].offset = j * myPacketSize;
            myUrb[i]->iso_frame_desc[j].length = myPacketSize;
        }
    }

    for(i = 0; i < nbUrbs; i++){
        // TODO: if ((ret = usb_submit_urb(...)) < 0) {
            // TODO: printk(KERN_WARNING "");
            return ret;
        }
    }
    return 0;
}


static void urbCompletionCallback(struct urb *urb) {
    int ret;
    int i;
    unsigned char * data;
    unsigned int len;
    unsigned int maxlen;
    unsigned int nbytes;
    void * mem;

    if(urb->status == 0){

        for (i = 0; i < urb->number_of_packets; ++i) {
            if(myStatus == 1){
                continue;
            }
            if (urb->iso_frame_desc[i].status < 0) {
                continue;
            }

            data = urb->transfer_buffer + urb->iso_frame_desc[i].offset;
            if(data[1] & (1 << 6)){
                continue;
            }
            len = urb->iso_frame_desc[i].actual_length;
            if (len < 2 || data[0] < 2 || data[0] > len){
                continue;
            }

            len -= data[0];
            maxlen = myLength - myLengthUsed ;
            mem = myData + myLengthUsed;
            nbytes = min(len, maxlen);
            memcpy(mem, data + data[0], nbytes);
            myLengthUsed += nbytes;

            if (len > maxlen) {
                myStatus = 1; // DONE
            }

            // Mark the buffer as done if the EOF marker is set.
            if ((data[1] & (1 << 1)) && (myLengthUsed != 0)) {
                myStatus = 1; // DONE
            }
        }

        if (!(myStatus == 1)){
            if ((ret = usb_submit_urb(urb, GFP_ATOMIC)) < 0) {
                // TODO: printk(KERN_WARNING "");
            }
        } else {
            ///////////////////////////////////////////////////////////////////////
            //  Synchronisation
            ///////////////////////////////////////////////////////////////////////
            //TODO
        }
    } else {
        // TODO: printk(KERN_WARNING "");
    }
}