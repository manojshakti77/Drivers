#include<linux/init.h>
#include<linux/module.h>

MODULE_LICENSE("GPL");

int ex01_module_init(void)
{
	printk(KERN_ALERT "INIT MODULE\n");
	return 0;
}

void ex01_module_exit(void)
{
	printk(KERN_ALERT "EXIT MODULE\n");
}

module_init(ex01_module_init);
module_exit(ex01_module_exit);
