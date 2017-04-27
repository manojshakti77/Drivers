::obj-m := ex01.o
obj-m := mybt.o
::obj-m := bt.o

all: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean : 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
