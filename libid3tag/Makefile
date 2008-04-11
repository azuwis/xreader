PSPSDK=$(shell psp-config --pspsdk-path)
PSPDIR = $(shell psp-config --psp-prefix)

PSP_FW_VERSION=390

TARGET_LIB = ../lib/libid3tag.a
OBJS = \
compat.o  field.o  frametype.o  parse.o   ucs4.o   util.o \
crc.o     file.o   genre.o      render.o  utf16.o  version.o \
debug.o   frame.o  latin1.o     tag.o     utf8.o

ifeq ($(DEBUG), 1)
CFLAGS = -O0 -G0 -g -Wall
else
CFLAGS = -O3 -G0 -fomit-frame-pointer -ffast-math -frename-registers -Wall
endif		

CFLAGS += -DHAVE_CONFIG_H -DHAVE_SYS_TYPES_H -DHAVE_ERRNO_H -DHAVE_UNISTD_H \
	-DHAVE_FCNTL_H -I$(PSPSDK)/include/sys/  -I../include -I . 
include $(PSPSDK)/lib/build.mak
