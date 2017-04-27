#include<linux/init.h>
#include<linux/module.h>
#include<linux/fs.h>//For struct file_operations
#include<linux/slab.h>
#include<linux/kernel.h>
#include<linux/usb.h>

MODULE_LICENSE("GPL");

#define MIN(a,b) ((a < b)?a:b)

/* Structure to hold all of our device specific stuff */
struct usb_skel {
	struct usb_device	*udev;			/* the usb device for this device */
	struct usb_interface	*interface;		/* the interface for this device */
	struct semaphore	limit_sem;		/* limiting the number of writes in progress */
	struct usb_anchor	submitted;		/* in case we need to retract our submissions */
	unsigned char           *bulk_in_buffer;	/* the buffer to receive data */
	size_t			bulk_in_size;		/* the size of the receive buffer */
	__u8			bulk_in_endpointAddr;	/* the address of the bulk in endpoint */
	__u8			bulk_out_endpointAddr;	/* the address of the bulk out endpoint */
	int			errors;			/* the last request tanked */
	int			open_count;		/* count the number of openers */
	spinlock_t		err_lock;		/* lock for errors */
	struct kref		kref;
	struct mutex		io_mutex;		/* synchronize I/O with disconnect */
};

struct usb_device_id bt_table[] =
{
      {USB_DEVICE(0x0E0F,0x0008)},
      {USB_DEVICE(0x0008,0x0E0F)},
      {USB_DEVICE(0x07DC,0x8087)},
		{USB_DEVICE(0x8087,0x07DC)},
      {USB_DEVICE(0x2717,0xff40)},
        {}
};

struct usb_class_driver class;

MODULE_DEVICE_TABLE(usb,bt_table);

static struct usb_driver bt_driver;

#define r_size 16
ssize_t read_dev(struct file * file, char * buf, size_t size, loff_t * offset) 
{
	struct usb_skel *dev;
//	bool on_going;
	int retval;
	int read_cnt;
	char *r_buf;
	
	dev = file->private_data;
	if(!size)
	{
		printk(KERN_ALERT "Zero buffer zie to read\n");
		return 0;
	}
	
	if(!dev->interface)
	{
		printk(KERN_ALERT "No device\n");
		return 0;
	}
	r_buf = kmalloc(sizeof(char) * r_size,GFP_KERNEL);
#if 0
	retval = mutex_lock(&dev->io_mutex);
	if(retval < 0)
	{
		printk(KERN_ALERT "read::mutex_lock_interruptible\n");
		return 0;
	}	
#endif
//	retval = usb_bulk_msg(device,usb_rcvbulkpipe(device,BULK_EP_IN),bulk_buf_read,1024,&read_cnt,5000);
//	retval = usb_bulk_msg(device,usb_rcvbulkpipe(device,BULK_EP_IN),bulk_buf_read,1024,&read_cnt,5000);
	retval = usb_interrupt_msg(dev->udev,usb_rcvintpipe(dev->udev,0x81),r_buf,r_size,&read_cnt,5000);
	if(retval < 0)
	{
		printk(KERN_ALERT "READ ERROR......%d\n",retval);
		return retval;
	}
	printk(KERN_ALERT "\n");
	if(copy_to_user(buf,r_buf,MIN(size,read_cnt)))
	{	
		printk(KERN_ALERT "READ::Error copy_to_user\n");
		return -EFAULT;
	}
	printk(KERN_ALERT "READ_DEV......%d\n",read_cnt);
	return MIN(size,read_cnt);
}

ssize_t write_dev(struct file * file, const char *buf, size_t size, loff_t *offset )
{
	int retval;
	int wrote_cnt = MIN(size,1024);
	struct usb_skel *dev;
	char *w_buf;
	
	if(size == 0)
		return 0;
		
	dev = file->private_data;
	w_buf = kmalloc(sizeof(char )*size,GFP_KERNEL);
	
	if(copy_from_user(w_buf,buf,MIN(size,1024)))
	{	
		printk(KERN_ALERT "Write::Error copy_to_user\n");
		return -EFAULT;
	}
	
//	mutex_lock(&dev->io_mutex);
	if(!dev->interface)
	{
		printk(KERN_ALERT "Device got disconnected\n");
		mutex_unlock(&dev->io_mutex);
		return 0;
	}
	
//	retval = usb_bulk_msg(device,usb_sndbulkpipe(device,BULK_EP_OUT),bulk_buf,1024,&wrote_cnt,5000);
	retval = usb_control_msg(dev->udev,usb_sndctrlpipe(dev->udev,0),0x00,0x20,0,0,w_buf,wrote_cnt,5000);
//	mutex_unlock(&dev->io_mutex);
	if(retval < 0)
	{
		printk(KERN_ALERT "WRITE_DEV.....FAILED....%d\n",wrote_cnt);
		return retval;
	}
	printk(KERN_ALERT "WRITE_DEV.....SUCCESS....%d\n",wrote_cnt);
	return wrote_cnt;
}

int open_dev(struct inode *inode, struct file *file) 
{
	struct usb_skel *dev;
	struct usb_interface *interface;
	int subminor = 0;
//	int retval;
	
	subminor = iminor(inode);
	interface = usb_find_interface(&bt_driver,subminor);
	if(!interface)
	{
		printk(KERN_ALERT "ERROR::usb_find_interface open_dev\n");
		return 0;
	}
	dev = usb_get_intfdata(interface);
	if(!dev)
	{
		printk(KERN_ALERT "ERROR::usb_get_intfdata open_dev\n");
		return 0;
	}
#if 0
	retval = usb_autopm_get_interface(interface);
	if (retval)
		return retval;
#endif	
	
	file->private_data = dev;
	printk(KERN_ALERT "OPEN_DEV\n");
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
		struct usb_skel *dev;
		
		dev = kmalloc(sizeof(*dev),GFP_KERNEL);
		if(!dev)
		{
			printk(KERN_ALERT "Unable to allocate kzalloc\n");
			return -1;
		}
		dev->udev = usb_get_dev(interface_to_usbdev(interface));
		dev->interface = interface;
		usb_set_intfdata(interface,dev);
       
		class.name = "/dev/blueooth%d";
       	class.fops = &fops;
               
        retval = usb_register_dev(interface,&class);
        if(retval < 0)
        {
			printk(KERN_ALERT "USB_REGISTER_DEV ERROR bt_probe\n");
			usb_set_intfdata(interface,NULL);
			return -1;
        }
        else
        {
                printk(KERN_ALERT "Minor registered......%d\n",interface->minor);
        }
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
//		ptr = kmalloc(100,GFP_KERNEL);
	}
	return 0;
}

void ex01_module_exit(void)
{
	printk(KERN_ALERT "EXIT MODULE\n");
	usb_deregister(&bt_driver); 
//	kfree(ptr);
}

module_init(ex01_module_init);
module_exit(ex01_module_exit);
