CC = @PSP_CC@
CCAS = $(CC)
AR = @PSP_AR@
RANLIB = @PSP_RANLIB@

EXEEXT =
OBJEXT = o

xrdir = $(top_srcdir)/src

AM_CPPFLAGS = \
-I$(srcdir) -I$(xrdir) -I$(xrdir)/include -I$(xrdir)/include/freetype2 \
-I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_srcdir)/xrPrx -I$(top_srcdir)/xr_rdriver \
-I$(PSPSDK)/include

if OPTIMIZE
AM_CFLAGS = -Os -G0 -Wall -fno-builtin-printf
else
AM_CFLAGS = -O0 -G0 -g -Wall
endif
CCASFLAGS = $(CFLAGS) $(INCLUDES)

LDFLAGS = -mno-crt0 -nostartfiles
	
noinst_PROGRAMS= testprx.elf

testprx_elf_SOURCES = $(xrdir)/strsafe.c $(xrdir)/strsafe.h main.c testprx.h exports.exp

TARGET = testprx

PSP_FW_VERSION=401
BUILD_PRX=1

PRX_EXPORTS = exports.exp

PSPSDK=$(shell psp-config --pspsdk-path)
LIBS=-lpspkernel
libdir = $(top_srcdir)/lib
LIBS += $(libdir)/pmpmodavc/libavformat.a \
	$(libdir)/pmpmodavc/libavcodec.a $(libdir)/pmpmodavc/libavutil.a -lm

include $(PSPSDK)/lib/build.mak

all: testprx.S
testprx.S: exports.exp
	psp-build-exports -s $^

clean-local:
	$(RM) testprx.prx testprx.S
