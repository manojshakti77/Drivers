::obj-m := ex01.o
::obj-m := mybt.o
::obj-m := bt.o
::obj-m := btskel_sleep.o
obj-m := btskel.o

all: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean : 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
