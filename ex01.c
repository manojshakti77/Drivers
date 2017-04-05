#include<linux/init.h>
#include<linux/module.h>
#include<linux/fs.h>//For struct file_operations
#include<linux/slab.h>
#include<linux/kernel.h>
#include<linux/usb.h>
#include<linux/spinlock.h>

#define MAJOR_NUMBER 30
#define BULK_EP_OUT 0x01
#define BULK_EP_IN 0x82

//#define USB_TYPE_VENDOR 0x02 << 5
//#define USB_DIR_OUT 0


MODULE_LICENSE("GPL");

char *ptr;
struct btusb_data {
    struct hci_dev       *hdev;
    struct usb_device    *udev;
    struct usb_interface *intf;
    struct usb_interface *isoc;
    struct usb_interface *diag;

    unsigned long flags;

    struct work_struct work;
    struct work_struct waker;

    struct usb_anchor deferred;
    struct usb_anchor tx_anchor;
    int tx_in_flight;
    spinlock_t txlock;

    struct usb_anchor intr_anchor;
    struct usb_anchor bulk_anchor;
    struct usb_anchor isoc_anchor;
    struct usb_anchor diag_anchor;
    spinlock_t rxlock;

    struct sk_buff *evt_skb;
    struct sk_buff *acl_skb;
    struct sk_buff *sco_skb;

    struct usb_endpoint_descriptor *intr_ep;
    struct usb_endpoint_descriptor *bulk_tx_ep;
    struct usb_endpoint_descriptor *bulk_rx_ep;
    struct usb_endpoint_descriptor *isoc_tx_ep;
    struct usb_endpoint_descriptor *isoc_rx_ep;
    struct usb_endpoint_descriptor *diag_tx_ep;
    struct usb_endpoint_descriptor *diag_rx_ep;

    __u8 cmdreq_type;
    __u8 cmdreq;

    unsigned int sco_num;
    int isoc_altsetting;
    int suspend_count;
    int (*recv_event)(struct hci_dev *hdev, struct sk_buff *skb);
    int (*recv_bulk)(struct btusb_data *data, void *buffer, int count);

    int (*setup_on_usb)(struct hci_dev *hdev);
};

struct usb_device *device;
struct usb_class_driver class;
static unsigned char bulk_buf[1024];
static unsigned char bulk_buf_read[1024];

static struct usb_driver bt_driver;

#define MIN(a,b) (((a) <= (b)) ? (a) : (b)) 

struct usb_device_id bt_table[] =
{
        {USB_DEVICE(0x0E0F,0x0008)},
//        {USB_DEVICE(0x0008,0x0E0F)},
//        {USB_DEVICE(0x07DC,0x8087)},
//        {USB_DEVICE(0x8087,0x07DC)},
//        {USB_DEVICE(0x2717,0xff40)},
//		{USB_DEVICE_INFO(0xe0, 0x01, 0x01)},
        {}
};

unsigned char intr;

MODULE_DEVICE_TABLE(usb,bt_table);//----------------------->>>>>

#if 1
ssize_t read_dev(struct file * read_file, char * buf, size_t size, loff_t * offset) 
{
	int retval;
	int read_cnt;
	struct usb_skel *dev;
	dev = read_file->private_data;

	printk(KERN_ALERT "read :: interface1\n");
	/* no concurrent readers */
    mutex_lock(&dev->io_mutex);
    if (!dev->interface)
	{
		printk(KERN_ALERT "Device disconnected\n");
        return retval;
	}
//	retval = usb_bulk_msg(device,usb_rcvbulkpipe(device,BULK_EP_IN),bulk_buf_read,1024,&read_cnt,5000);
	retval = usb_bulk_msg(device,usb_rcvbulkpipe(device,0x81),bulk_buf_read,1024,&read_cnt,5000);
//	retval = usb_interrupt_msg(device,usb_rcvintpipe(device,0x81),bulk_buf_read,16,&read_cnt,5000);
	if(retval < 0)
	{
		printk(KERN_ALERT "READ ERROR......%d\n",retval);
		return retval;
	}
	printk(KERN_ALERT "read :: interface2\n");
	mutex_unlock(&dev->io_mutex);
	printk(KERN_ALERT "read :: interface3\n");
//	read_unlock(&read_slock);
//	printk(KERN_ALERT "INSIDE KERNEL:");
//	for(i=0;i<read_cnt;i++)
//		printk(KERN_ALERT "0x%02X ",bulk_buf_read[i]);
//	printk(KERN_ALERT "\n");
	printk(KERN_ALERT "read :: interface4\n");
	if(copy_to_user(buf,bulk_buf_read,MIN(size,read_cnt)))
	{	
		printk(KERN_ALERT "READ::Error copy_to_user\n");
		return -EFAULT;
	}
	printk(KERN_ALERT "READ_DEV......%d\n",read_cnt);
//	for(i=0;i<read_cnt;i++)
//		printk(KERN_ALERT "0x%02X ",buf[i]);
//	printk(KERN_ALERT "\n");

	return MIN(size,read_cnt);
}

ssize_t write_dev(struct file * write_file, const char *buf, size_t size, loff_t *offset )
{
//	int i;
	//int wrote_cnt = MIN(size,1024);
	int wrote_cnt = 8;
	struct usb_skel *dev;
    int retval = 0;

	printk(KERN_ALERT "Write :: interface1\n");
	if(size == 0)
	{
		printk(KERN_ALERT "Write :: interface7\n");
		return 0;
	}
	if(copy_from_user(bulk_buf,buf,MIN(size,1024)))
	{	
		printk(KERN_ALERT "Write::Error copy_to_user\n");
		return -EFAULT;
	}
//	if(write_lock(&write_slock) <= 0);
//    {
//        printk(KERN_INFO "Write:spin_trylock failed\n");
//		return -1;
//    }
//	write_lock(&write_slock);

    dev = write_file->private_data;

	if(!dev)
	{
		printk(KERN_ALERT "dev error\n");
		return 0;
	}
#if 1	
	spin_lock_irq(&dev->err_lock);
    retval = dev->errors;
    if (retval < 0) {
        /* any error is reported once */
        dev->errors = 0;
		printk(KERN_ALERT "dev------>errors\n");
        /* to preserve notifications about reset */
        retval = (retval == -EPIPE) ? retval : -EIO;
    }
    spin_unlock_irq(&dev->err_lock);
    if (retval < 0)
	{
		printk(KERN_ALERT "Write::spin_unlock_irq\n");
		return retval;
	}
#endif
	/* this lock makes sure we don't submit URBs to gone devices */
	printk(KERN_ALERT "Write :: interface2\n");
    mutex_lock(&dev->io_mutex);
//f(!dev->io_mutex)
//
//		printk(KERN_ALERT "dev error\n");
//		return 0;
//	}
	printk(KERN_ALERT "Write :: interface3\n");
    if (!dev->interface) {      /* disconnect() was called */
        mutex_unlock(&dev->io_mutex);
        retval = -ENODEV;
        printk(KERN_ALERT "WRITE::Device disconnected\n");
		return 0;
    }
	printk(KERN_ALERT "Write :: interface4\n");

//	retval = usb_bulk_msg(device,usb_sndbulkpipe(device,BULK_EP_OUT),bulk_buf,1024,&wrote_cnt,5000);
	retval = usb_control_msg(device,usb_sndctrlpipe(device,0),0x00,0x21,0,0,bulk_buf,wrote_cnt,5000);
	if(retval < 0)
	{
		printk(KERN_ALERT "WRITE_DEV.....FAILED....%d\n",wrote_cnt);
//		printk(KERN_ALERT "Wrote message Size....%d....:",wrote_cnt);
		return retval;
	}
	printk(KERN_ALERT "Write :: interface5\n");
	mutex_unlock(&dev->io_mutex);
//	write_lock(&write_slock);
	printk(KERN_ALERT "WRITE_DEV.....SUCCESS....%d\n",wrote_cnt);
//	for(i=0;i<wrote_cnt;i++)
//		printk(KERN_ALERT "0x%02X ",buf[i]);
//	printk(KERN_ALERT "\n");
	return wrote_cnt;
}
#endif

#if 0
ssize_t write_dev(struct file * write_file, const char *buf, size_t size, loff_t *offset )
{
	printk(KERN_ALERT "WRITE_DEV\n");
	return 0;
}
ssize_t read_dev(struct file * read_file, char * buf, size_t size, loff_t * offset) 
{
	printk(KERN_ALERT "READ_DEV\n");
	return 0;
}
#endif
int open_dev(struct inode *inode, struct file *open_file) 
{
	struct btusb_data *dev;
	struct usb_interface *interface;
	int subminor;
	
	subminor = iminor(inode);
	printk(KERN_ALERT "inode\n");
	
	interface = usb_find_interface(&bt_driver,subminor);
	printk(KERN_ALERT "interface\n");
	if(!interface)
	{
		printk(KERN_ALERT "Interface not found\n");
		return 0;
	}
	printk(KERN_ALERT "interface1\n");
	
	dev = usb_get_intfdata(interface);
    if (!dev) 
	{
		printk(KERN_ALERT "Interface\n");
        return 0;
    }
	
	printk(KERN_ALERT "interface2\n");
	open_file->private_data = dev;
	
	
	return 0;
}
int release_dev(struct inode *inode, struct file *release_file)
{
	printk(KERN_ALERT "RELEASE_DEV\n");
	return 0;
} 

struct file_operations fops =
{
    open : open_dev,
    release : release_dev,
    read : read_dev,
    write : write_dev
};

int bt_probe(struct usb_interface *interface,const struct usb_device_id *id)
{
        int retval;
		int i;


		struct usb_host_interface *my_ptr;
		struct usb_host_interface *my_ptr_alt;
		struct usb_endpoint_descriptor *endpoint;
		struct btusb_data *dev;
		
		/* allocate memory for our device state and initialize it */
    	dev = devm_kzalloc(&interface->dev,sizeof(*dev), GFP_KERNEL);
    	if (!dev)
        {
			printk(KERN_ALERT "kzalloc error\n");
			return -1;
		}
		
		INIT_WORK(&data->work, btusb_work);
	    INIT_WORK(&data->waker, btusb_waker);
    	init_usb_anchor(&data->deferred);
    	init_usb_anchor(&data->tx_anchor);
    	spin_lock_init(&data->txlock);

    	init_usb_anchor(&data->intr_anchor);
    	init_usb_anchor(&data->bulk_anchor);
    	init_usb_anchor(&data->isoc_anchor);
    	init_usb_anchor(&data->diag_anchor);
    	spin_lock_init(&data->rxlock);

#if 0
    	kref_init(&dev->kref);
    	sema_init(&dev->limit_sem, 4);
    	mutex_init(&dev->io_mutex);
    	spin_lock_init(&dev->err_lock);
    	init_usb_anchor(&dev->submitted);
    	init_waitqueue_head(&dev->bulk_in_wait);
        device = interface_to_usbdev(interface);
		my_ptr = interface->cur_altsetting;
		my_ptr_alt = interface->altsetting;
		endpoint = &my_ptr->endpoint[1].desc;
		intr = endpoint->bEndpointAddress;

#endif
        class.name = "usb/bluetooth%d";
        class.fops = &fops;
		class.minor_base =   192;
	//	usb_set_intfdata(interface,dev);
             
//		intr = ptr;    
        if((retval = usb_register_dev(interface,&class)) < 0)
        {
                printk(KERN_ALERT "USB_REGISTER_DEV\n");
        }
        else
        {
                printk(KERN_ALERT "Minor registered......%d\n",interface->minor);
        }
		for (i = 0; i < my_ptr->desc.bNumEndpoints; i++)
		{
			endpoint = &my_ptr->endpoint[i].desc;

			printk(KERN_INFO "ED[%d]->bEndpointAddress: 0x%02X\n",i, endpoint->bEndpointAddress);
//			printk(KERN_INFO "ED[%d]->bmAttributes: 0x%02X\n",i,endpoint->bmAttributes);
//			printk(KERN_INFO "ED[%d]->wMaxPacketSize: 0x%04X (%d)\n",i, endpoint->wMaxPacketSize,endpoint->wMaxPacketSize);
		}
	
			printk(KERN_INFO "Agiilg\n");
		for (i = 0; i < my_ptr->desc.bNumEndpoints; i++)
        {
            endpoint = &my_ptr_alt->endpoint[i].desc;

            printk(KERN_INFO "ED[%d]->bEndpointAddress: 0x%02X\n",i, endpoint->bEndpointAddress);
//          printk(KERN_INFO "ED[%d]->bmAttributes: 0x%02X\n",i,endpoint->bmAttributes);
//          printk(KERN_INFO "ED[%d]->wMaxPacketSize: 0x%04X (%d)\n",i, endpoint->wMaxPacketSize,endpoint->wMaxPacketSize);
        }
//		rwlock_init(&read_slock);
//		rwlock_init(&write_slock);
		    /* let the user know what node this device is now attached to */
    	dev_info(&interface->dev,
         "USB Skeleton device now attached to USBSkel-%d",
         interface->minor);
        return retval;
}

void bt_disconnect(struct usb_interface *interface)
{
	usb_deregister_dev(interface,&class);
}

static struct usb_driver bt_driver =
{
    .name = "bt_driver",
    .probe = bt_probe,
    .disconnect = bt_disconnect,
    .id_table = bt_table,
};

int ex01_module_init(void)
{
	int result;
	printk(KERN_ALERT "INIT MODULE\n");
	if ((result = usb_register(&bt_driver)))
	{
  		printk(KERN_ALERT "Can not register driver as Major Device 30\n" ); 
	}
 	else 
	{
  		printk(KERN_ALERT "Device dirver registered successfully.\n" ); 
		ptr = kmalloc(100,GFP_KERNEL);
	}
	return 0;
}

void ex01_module_exit(void)
{
	printk(KERN_ALERT "EXIT MODULE\n");
	usb_deregister(&bt_driver); 
	kfree(ptr);
}

module_init(ex01_module_init);
module_exit(ex01_module_exit);
