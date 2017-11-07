#obj-m := ex01.o
#obj-m := mybt.o
#obj-m := bt.o
#obj-m := btskel_sleep.o
#obj-m := btskel.o
#obj-m := btskel_urb.o
obj-m := mybt_real.o
#obj-m := sysfs.o

all: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean : 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
