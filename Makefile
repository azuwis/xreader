TARGET = eReader
SRCS = avc.c bg.c bookmark.c charsets.c conf.c copy.c ctrl.c display.c fat.c \
	fs.c html.c image.c location.c lyric.c main.c mp3.c mp3info.c power.c msgresource.c\
	scene.c scene_image.c scene_music.c scene_text.c text.c ttfont.c usb.c win.c \
	./common/log.c ./common/qsort.c ./common/utils.c ./common/psp_utils.c
OBJS = $(SRCS:.c=.o)

INCDIR = $(PSPSDK)/../include ./include ./include/freetype2

CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-rtti

PSP_FW_VERSION=371
BUILD_PRX=1
PSP_LARGE_MEMORY=1

LIBDIR = ./lib
LIBS = ./lib/unrar.a ./lib/unzip.a ./lib/libchm.a ./lib/libpng.a \
	./lib/libgif.a ./lib/libjpeg.a ./lib/libbmp.a ./lib/libtga.a \
	./lib/libmad.a ./lib/libz.a ./lib/libfreetype.a ./lib/libwmadec.a \
	./lib/pmpmodavc/pmpmodavclib.a ./lib/pmpmodavc/libavformat.a ./lib/pmpmodavc/libavcodec.a ./lib/pmpmodavc/libavutil.a ./lib/pmpmodavc/libpspmpeg.a ./lib/pmpmodavc/pmpmodavclib.a \
	-lm -lpspaudio -lpspaudiolib -lpspgu -lpspgum -lpsphprm -lpsppower -lpsprtc \
	-lpspusb -lpspusbstor -lstdc++ -lpspkubridge

EXTRA_TARGETS = EBOOT.PBP
EXTRA_CLEAN = -r __SCE__eReader %__SCE__eReader eReader eReader% eReader.prx
PSP_EBOOT_TITLE = xReader
PSP_EBOOT_ICON = ICON0.png

PSPSDK=$(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak

#DEP
