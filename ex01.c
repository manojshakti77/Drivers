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

char *ptr;

struct usb_device *device;
struct usb_class_driver class;
static unsigned char bulk_buf[1024];
static unsigned char bulk_buf_read[1024];

#define MIN(a,b) (((a) <= (b)) ? (a) : (b)) 

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

#if 1
ssize_t read_dev(struct file * read_file, char * buf, size_t size, loff_t * offset) 
{
	int retval;
	int read_cnt;
	int i;
	
//	retval = usb_bulk_msg(device,usb_rcvbulkpipe(device,BULK_EP_IN),bulk_buf_read,1024,&read_cnt,5000);
//	retval = usb_bulk_msg(device,usb_rcvbulkpipe(device,BULK_EP_IN),bulk_buf_read,1024,&read_cnt,5000);
	retval = usb_interrupt_msg(device,usb_rcvintpipe(device,0x81),bulk_buf_read,1024,&read_cnt,5000);
	if(retval < 0)
	{
		printk(KERN_ALERT "READ ERROR......%d\n",retval);
		return retval;
	}
	printk(KERN_ALERT "INSIDE KERNEL:");
	for(i=0;i<read_cnt;i++)
		printk(KERN_ALERT "0x%02X ",bulk_buf_read[i]);
	printk(KERN_ALERT "\n");
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
	int retval;
//	int i;
	int wrote_cnt = MIN(size,1024);
	
	if(copy_from_user(bulk_buf,buf,MIN(size,1024)))
	{	
		printk(KERN_ALERT "Write::Error copy_to_user\n");
		return -EFAULT;
	}
//	retval = usb_bulk_msg(device,usb_sndbulkpipe(device,BULK_EP_OUT),bulk_buf,1024,&wrote_cnt,5000);
	retval = usb_control_msg(device,usb_sndctrlpipe(device,0),0x00,0x20,0,0,bulk_buf,wrote_cnt,5000);
	if(retval < 0)
	{
		printk(KERN_ALERT "WRITE_DEV.....FAILED....%d\n",wrote_cnt);
//		printk(KERN_ALERT "Wrote message Size....%d....:",wrote_cnt);
		return retval;
	}
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
        device = interface_to_usbdev(interface);

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

struct usb_driver bt_driver =
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
