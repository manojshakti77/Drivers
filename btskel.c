/* http://opensourceforu.com/2016/02/waiting-and-blocking-in-a-linux-driver/ */

#include<linux/init.h>
#include<linux/module.h>
#include<linux/fs.h>//For struct file_operations
#include<linux/slab.h>
#include<linux/kernel.h>
#include<linux/usb.h>

#define MAJOR_NUMBER 30
#define BULK_EP_OUT 0x01
#define BULK_EP_IN 0x82

//#define USB_TYPE_VENDOR 0x02 << 5
//#define USB_DIR_OUT 0


MODULE_LICENSE("GPL");

#define JIFFY

#ifdef JIFFY 
#include<linux/jiffies.h>
#endif

struct usb_device *device;
struct usb_class_driver class;

static struct usb_driver bt_driver;


struct usb_device_id bt_table[] =
{
//        {USB_DEVICE(0x0E0F,0x0008)},
//        {USB_DEVICE(0x0008,0x0E0F)},
//        {USB_DEVICE(0x07DC,0x8087)},
        {USB_DEVICE(0x8087,0x07DC)},
//        {USB_DEVICE(0x2717,0xff40)},
        {}
};

MODULE_DEVICE_TABLE(usb,bt_table);//----------------------->>>>>

MODULE_LICENSE("GPL");

#define MIN(a,b) ((a < b)?a:b)

/* Structure to hold all of our device specific stuff */
struct usb_skel {
    struct usb_device   *udev;          /* the usb device for this device */
    struct usb_interface    *interface;     /* the interface for this device */
    struct semaphore    limit_sem;      /* limiting the number of writes in progress */
    struct usb_anchor   submitted;      /* in case we need to retract our submissions */
    unsigned char           *bulk_in_buffer;    /* the buffer to receive data */
    size_t          bulk_in_size;       /* the size of the receive buffer */
    __u8            bulk_in_endpointAddr;   /* the address of the bulk in endpoint */
    __u8            bulk_out_endpointAddr;  /* the address of the bulk out endpoint */
    int         errors;         /* the last request tanked */
    int         open_count;     /* count the number of openers */
    spinlock_t      err_lock;       /* lock for errors */
    struct kref     kref;
    struct mutex        io_mutex;       /* synchronize I/O with disconnect */
};
#define r_size 16



#if 1
ssize_t write_dev(struct file *file, const char *buf, size_t size, loff_t *offset )
{
	int retval;
    int wrote_cnt = MIN(size,1024);
    struct usb_skel *dev;
    char *w_buf;

    if(size == 0)
        return 0;

    dev = file->private_data;
    w_buf = kmalloc(sizeof(char )*size,GFP_KERNEL);
	printk(KERN_ALERT "WRITE_DEV\n");

    if(copy_from_user(w_buf,buf,MIN(size,1024)))
    {
        printk(KERN_ALERT "Write::Error copy_to_user\n");
        return -EFAULT;
    }

//  mutex_lock(&dev->io_mutex);
    if(!dev->interface)
    {
        printk(KERN_ALERT "Device got disconnected\n");
        mutex_unlock(&dev->io_mutex);
        return 0;
    }

#if 0
	if(flag == 1)
	{
		retval = wake_up_process(sleeping_task);
		if(retval == 1)
        	printk(KERN_ALERT "Woked up the process .....\n");
		else
        	printk(KERN_ALERT "Process already running.....\n");
		flag = 0;
	}
#endif

//  retval = usb_bulk_msg(device,usb_sndbulkpipe(device,BULK_EP_OUT),bulk_buf,1024,&wrote_cnt,5000);
    retval = usb_control_msg(dev->udev,usb_sndctrlpipe(dev->udev,0),0x00,0x20,0,0,w_buf,wrote_cnt,5000);
//  mutex_unlock(&dev->io_mutex);
    if(retval < 0)
    {
        printk(KERN_ALERT "WRITE_DEV.....FAILED....%d\n",wrote_cnt);
        return retval;
    }
    printk(KERN_ALERT "WRITE_DEV.....SUCCESS....%d\n",wrote_cnt);
    return wrote_cnt;
//	return 0;
}
ssize_t read_dev(struct file *file, char * buf, size_t size, loff_t * offset) 
{
	    struct usb_skel *dev;
//  bool on_going;
    int retval;
    int read_cnt;
    char *r_buf;

#ifdef JIFFY
	unsigned long a;
	unsigned long b;
	unsigned long diff;
#endif
	
	printk(KERN_ALERT "READ_DEV\n");

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
#if 0
	if(flag == 0) 
	{
	printk(KERN_ALERT "read sleeping_task is NULL\n");
//	sleeping_task = current;
//	if(sleeping_task == NULL)
//		printk(KERN_ALERT "sleeping_task is NULL\n");
//	set_current_state(TASK_INTERRUPTIBLE);
	//schedule_work();
	flag = 1;
	}
#endif
//  retval = usb_bulk_msg(device,usb_rcvbulkpipe(device,BULK_EP_IN),bulk_buf_read,1024,&read_cnt,5000);
//  retval = usb_bulk_msg(device,usb_rcvbulkpipe(device,BULK_EP_IN),bulk_buf_read,1024,&read_cnt,5000);
#ifdef JIFFY
	a = jiffies;
#endif

    retval = usb_interrupt_msg(dev->udev,usb_rcvintpipe(dev->udev,0x81),r_buf,r_size,&read_cnt,5000);
    if(retval < 0)
    {
        printk(KERN_ALERT "READ ERROR......%d\n",retval);
        return retval;
    }
#ifdef JIFFY
	b = jiffies;
	if(time_before(a,b))
	{
		diff = ((unsigned long)b - (unsigned long)a);
		diff = (diff * 1000)/HZ;
		printk(KERN_ALERT "JIFFY = %lu\n",diff);
	}
#endif
	if(copy_to_user(buf,r_buf,MIN(size,read_cnt)))
    {
        printk(KERN_ALERT "READ::Error copy_to_user\n");
        return -EFAULT;
    }
    printk(KERN_ALERT "READ_DEV......%d\n",read_cnt);
    return MIN(size,read_cnt);
}
#endif
int open_dev(struct inode *inode, struct file *file) 
{
	struct usb_skel *dev;
    struct usb_interface *interface;
    int subminor = 0;
//    int retval;

    subminor = iminor(inode);
	printk(KERN_ALERT "subminor...%d\n",subminor);
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
	
    /*Register a interrupt handler*/
    int request_irq(unsigned int irq,&handler,SA_SHIRQ, "manoj_bt_int",NULL);
        
    return 0;
}
DECLARE_TASKLET(interupt_tasklet, func, data);
irqreturn_t handler(int, void *, struct pt_regs *)
{
    /*Interrupt handling for the data present at the USB interrupt*/
    
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

		dev = kmalloc(sizeof(struct usb_skel ),GFP_KERNEL);
		if(!dev)	
		{
			printk(KERN_ALERT "Error kmalloc....\n");
			return -1;
		}
		dev->udev = usb_get_dev(interface_to_usbdev(interface));
        dev->interface = interface;
        usb_set_intfdata(interface,dev);
        
		//device = interface_to_usbdev(interface);

        class.name = "usb/bluetooth%d";
        class.fops = &fops;
                
        if((retval = usb_register_dev(interface,&class)) < 0)
        {
                printk(KERN_ALERT "USB_REGISTER_DEV\n");
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
	}
	return 0;
}

void ex01_module_exit(void)
{
	printk(KERN_ALERT "EXIT MODULE\n");
	usb_deregister(&bt_driver); 
}

module_init(ex01_module_init);
module_exit(ex01_module_exit);
