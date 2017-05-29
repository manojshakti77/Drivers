/* http://opensourceforu.com/2016/02/waiting-and-blocking-in-a-linux-driver/ */

#include<linux/init.h>
#include<linux/module.h>
#include<linux/fs.h>//For struct file_operations
#include<linux/slab.h>
#include<linux/kernel.h>
#include<linux/usb.h>
#include<linux/interrupt.h>
#include<linux/irq.h>



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
	struct urb *int_urb;
	char *int_buf;
    struct usb_endpoint_descriptor *intr_ep;
    struct usb_endpoint_descriptor *bulk_rx_ep;
    struct usb_endpoint_descriptor *bulk_tx_ep;	
	wait_queue_head_t   bulk_in_wait;       /* to wait for an ongoing read */
	bool ongoing_read;
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
#if 0
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
#endif

static void btusb_intr_complete(struct urb* urb)
{
	struct usb_skel *dev = urb->context;
//	int i;

	if(urb->status == 0)
	{
	printk(KERN_ALERT "btusb_intr_complete received\n");
	}
//	for(i=0;i<urb->actual_length;i++)
//		printk(KERN_ALERT "0x%02X\n",dev->int_buf[i]);
	dev->ongoing_read = 0;
#if 0
//	if(copy_to_user(temp,dev->int_buf,MIN(r_size,urb->actual_length)))
	retval = copy_to_user(temp,dev->int_buf,MIN(r_size,urb->actual_length));
	if(retval)
    {
        printk(KERN_ALERT "READ::Error copy_to_user.....%lu\n",retval);
    }
#endif
	wake_up_interruptible(&dev->bulk_in_wait);
	
	return;
}

ssize_t read_dev(struct file *file, char * buf, size_t size, loff_t * offset) 
{
	struct usb_skel *dev;
//	char *r_buf;
	int status;
	int i;
	
	dev = file->private_data;
	printk(KERN_ALERT "READ\n");
	printk(KERN_ALERT "Endpoint.....0x%02X\n",dev->intr_ep->bEndpointAddress);
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
	dev->int_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!dev->int_urb)
	{
		printk(KERN_ALERT "FAILED to usb_alloc_urb");
        return -ENOMEM;
	}
#if 0
//    if(copy_from_user(temp,buf,sizeof(char *)))
    retval = copy_from_user(temp,buf,sizeof(temp));
	if(retval)
	{
        printk(KERN_ALERT "READ::Error copy_from_user......%lu",retval);
	}
#endif
    dev->int_buf = kmalloc(sizeof(char) * r_size,GFP_KERNEL);	
    usb_fill_int_urb(dev->int_urb, dev->udev, usb_rcvintpipe(dev->udev,dev->intr_ep->bEndpointAddress), dev->int_buf,r_size,btusb_intr_complete,dev,dev->intr_ep->bInterval);
//    usb_fill_int_urb(dev->int_urb, dev->udev, usb_rcvintpipe(dev->udev,0x81), dev->int_buf,r_size,btusb_intr_complete,dev,5000);
	dev->int_urb->transfer_flags |= URB_FREE_BUFFER;
	status = usb_urb_dir_in(dev->int_urb);
	printk(KERN_ALERT "urb in interrupt direction....%d\n",status);
	status = usb_urb_dir_out(dev->int_urb);
	printk(KERN_ALERT "urb out interrupt direction....%d\n",status);
	dev->ongoing_read = 1;
	status = usb_submit_urb(dev->int_urb,GFP_KERNEL);
	if(status < 0)	
	{
		printk(KERN_ALERT "Failed to submit usb....:%d\n",status);
		return status;
	}
	status = wait_event_interruptible(dev->bulk_in_wait, (!dev->ongoing_read));
    printk(KERN_ALERT "wait_event_interruptible....");
    if (status < 0)
       	printk(KERN_ALERT "Failed for wait_event_interruptible....%d\n",status);
	dev->ongoing_read = 1;
	for(i=0;i<dev->int_urb->actual_length;i++)
		printk(KERN_ALERT "0x%02X\n",dev->int_buf[i]);
	if(copy_to_user(buf,dev->int_buf,MIN(r_size,dev->int_urb->actual_length)))
    {
        printk(KERN_ALERT "READ::Error copy_to_user\n");
        return -EFAULT;
    }
    printk(KERN_ALERT "READ_DEV......%d\n",dev->int_urb->actual_length);
    return MIN(r_size,dev->int_urb->actual_length);
}


//DECLARE_TASKLET(interupt_tasklet, func, data);

//irqreturn_t handler(int intr, void *dev_id, struct pt_regs *regs)
irqreturn_t handler(int intr, void *dev_id)
{
    /*Interrupt handling for the data present at the USB interrupt*/
	printk(KERN_ALERT "Interrupt....%d\n",intr);
	return 0;
}

int open_dev(struct inode *inode, struct file *file) 
{
	struct usb_skel *dev;
    struct usb_interface *interface;
    int subminor = 0;
    //int retval;

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
	
#if 0
    /*Register a interrupt handler*/
    retval =  request_irq(18,handler,IRQF_SHARED, "manoj_bt_int",NULL);
	printk(KERN_ALERT "request_irq retval....%d\n",retval);
	enable_irq(18);
#endif
    
	return 0;
}


int release_dev(struct inode *inode, struct file *release_file)
{
	printk(KERN_ALERT "RELEASE_DEV\n");
//	free_irq(18,NULL);
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
	    struct usb_endpoint_descriptor *ep_desc;
		int i;

//		dev = kmalloc(sizeof(struct usb_skel ),GFP_KERNEL);
		dev = devm_kzalloc(&interface->dev, sizeof(*dev), GFP_KERNEL);
		if(!dev)	
		{
			printk(KERN_ALERT "Error kmalloc....\n");
			return -1;
		}
		printk(KERN_ALERT "Number=%d\n",interface->cur_altsetting->desc.bNumEndpoints);

	    for (i = 0; i < interface->cur_altsetting->desc.bNumEndpoints; i++) {
        ep_desc = &interface->cur_altsetting->endpoint[i].desc;

        if (!dev->intr_ep && usb_endpoint_is_int_in(ep_desc)) {
            dev->intr_ep = ep_desc;
			printk(KERN_ALERT "0x%02X\n",dev->intr_ep->bEndpointAddress);
            continue;
        }

        if (!dev->bulk_tx_ep && usb_endpoint_is_bulk_out(ep_desc)) {
            dev->bulk_tx_ep = ep_desc;
			printk(KERN_ALERT "0x%02X\n",dev->bulk_tx_ep->bEndpointAddress);
            continue;
        }

        if (!dev->bulk_rx_ep && usb_endpoint_is_bulk_in(ep_desc)) {
            dev->bulk_rx_ep = ep_desc;
			printk(KERN_ALERT "0x%02X\n",dev->bulk_rx_ep->bEndpointAddress);
            continue;
        }
    	}	

    	if (!dev->intr_ep || !dev->bulk_tx_ep || !dev->bulk_rx_ep)
		{
			printk(KERN_ALERT "End points not found\n");
        	return -ENODEV;
		}
		init_waitqueue_head(&dev->bulk_in_wait);
//		printk(KERN_ALERT "Endpoints.....0x%02X,0x%02X,0x%02X\n",dev->intr_ep->bEndpointAddress,dev->bulk_tx_ep->bEndpointAddress,dev->bulk_rx_ep->bEndpointAddress);

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
