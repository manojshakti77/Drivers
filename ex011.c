#include<linux/init.h>
#include<linux/module.h>
#include<linux/fs.h>//For struct file_operations
#include<linux/slab.h>

#define MAJOR_NUMBER 30

MODULE_LICENSE("GPL");

char *ptr;

ssize_t read_dev(struct file * read_file, char * buf, size_t size, loff_t * offset) 
{
	printk(KERN_ALERT "WRITE_DEV\n");
	return 0;
}
ssize_t write_dev(struct file * write_file, const char *buf, size_t size, loff_t *offset )
{
	printk(KERN_ALERT "WRITE_DEV\n");
	return 0;
}
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


int ex01_module_init(void)
{
	printk(KERN_ALERT "INIT MODULE\n");
	if (register_chrdev(MAJOR_NUMBER, "My_driver", &fops))
  		printk(KERN_ALERT "Can not register driver as Major Device 30\n" ); 
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
	unregister_chrdev(MAJOR_NUMBER,"my_driver"); 
	kfree(ptr);
}

module_init(ex01_module_init);
module_exit(ex01_module_exit);
