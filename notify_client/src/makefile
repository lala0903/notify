#CROSS_COMPILE ?= arm-linux-gnueabihf-
CROSS_COMPILE =
TARGET ?= libnotiyclient.so

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
INCLUDE := -I ../inc

SOURCE	:= threadpool.c \
			notify_client_send.c \
			notify_client_recv.c \
			notify_client_init.c \
			notify_client_ack_list.c

CFLAGS := -shared -fPIC -g -O2
LDFLAGS := -lthpread

CFILES := $(wildcard *.c src/*.c)
# CFILENDIR := $(notdir  $(CFILES))
# COBJS := $(patsubst %, obj/%, $(CFILENDIR:.c=.o))

.PHONY: all

all:
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDE) $(SOURCE) -o $(TARGET)
