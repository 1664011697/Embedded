CC = arm-fsl-linux-gnueabi-gcc
#./gcc-4.6.2-glibc-2.13-linaro-multilib-2011.12/fsl-linaro-toolchain/bin/arm-fsl-linux-gnueabi-gcc

SRC     = ./fp_process.c ./fp_FTDI.c ./fp_commond.c ./fp_main.c
CFLAGS = -O2 -DEMBEDDED_LINUX
LIBS = -L. -L./ -ldl -lrt -lft4222 
INCLUDE = -I./ -I./hardware

all:
	$(CC) $(SRC) $(LIBS) $(INCLUDE) $(CFLAGS) -o fingerprint
clean:
	$(RM) $(TARGET) $(OBJS)
