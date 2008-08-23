CC = @PSP_CC@
CCAS = $(CC)
AR = @PSP_AR@
RANLIB = @PSP_RANLIB@

EXEEXT =
AM_CPPFLAGS = \
-I$(srcdir) -I$(srcdir)/include -I$(srcdir)/include/freetype2 \
-I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_srcdir)/xrPrx -I$(top_srcdir)/xr_rdriver \
-I$(PSPSDK)/include

if OPTIMIZE
AM_CFLAGS = -O3 -G0 -fomit-frame-pointer -ffast-math -frename-registers -Wall
AM_CFLAGS += \
-funsigned-char -ffast-math -fforce-addr -fmerge-all-constants \
-floop-optimize -funsafe-loop-optimizations -ftree-loop-linear
else
AM_CFLAGS = -O0 -G0 -g -Wall
endif
CCASFLAGS = $(CFLAGS) $(INCLUDES)

noinst_PROGRAMS = xReader.elf

xReader_elf_SOURCES = \
array.c array.h avc.c avc.h bg.c bg.h bookmark.c bookmark.h \
buffer.c buffer.h charsets.c charsets.h conf.c conf.h copy.c \
copy.h ctrl.c ctrl.h dbg.c dbg.h dictionary.c dictionary.h \
display.c display.h fat.c fat.h fs.c fs.h html.c html.h image.c \
image.h iniparser.c iniparser.h location.c location.h lyric.c \
lyric.h main.c musicdrv.c musicdrv.h musicmgr.c musicmgr.h power.c \
power.h pspscreen.h scene.c scene.h scene_image.c scene_impl.h \
scene_music.c scene_text.c strsafe.c strsafe.h text.c text.h \
ttfont.c ttfont.h usb.c usb.h version.h win.c win.h common/datatype.h \
common/psp_utils.c common/psp_utils.h common/qsort.c common/qsort.h \
common/utils.c common/utils.h apetaglib/APETag.c apetaglib/APETag.h \
apetaglib/TagError.c apetaglib/TagHeader.h apetaglib/TagItem.c \
apetaglib/TagItem.h simple_gettext.h osk.c \
osk.h exception.c exception.h m33boot.c m33boot.h passwdmgr.c passwdmgr.h \
archive.c archive.h resample.c resample.h xrPrx.S xr_RDLib.S

xrPrx.o: $(top_builddir)/xrPrx/xrPrx.S
	$(CC) -I$(PSPSDK)/include -c -o $@ $^

xr_RDLib.o: $(top_builddir)/xr_rdriver/xr_RDLib.S
	$(CC) -I$(PSPSDK)/include -c -o $@ $^

xReader_elf_SOURCES += \
madmp3drv.c madmp3drv.h mpcplayer.c mpcplayer.h xmp3audiolib.c \
clock.c mp3info.c

if NLS
xReader_elf_SOURCES += simple_gettext.c sofile.c sofile.h hash.c hash.h
endif

clean-local:
	$(RM) -rf __SCE__xReader %__SCE__xReader xReader xReader% xReader.prx \
		PARAM.SFO EBOOT.PBP

PSP_FW_VERSION = 401
BUILD_PRX = 1
PSP_LARGE_MEMORY = 1

libdir = $(top_srcdir)/lib
LIBS = $(libdir)/libFLAC.a $(libdir)/libmpcdec.a $(libdir)/libvorbisidec.a
LIBS += $(libdir)/libid3tag.a 
LIBS += \
-L$(libdir) $(libdir)/unrar.a $(libdir)/unzip.a $(libdir)/libchm.a \
$(libdir)/libpng.a $(libdir)/libgif.a $(libdir)/libjpeg.a $(libdir)/libbmp.a \
$(libdir)/libtga.a $(libdir)/libid3tag.a $(libdir)/libmad.a $(libdir)/libz.a \
$(libdir)/libfreetype.a $(libdir)/libwmadec.a $(libdir)/pmpmodavc/pmpmodavclib.a \
$(libdir)/pmpmodavc/libavformat.a $(libdir)/pmpmodavc/libavcodec.a \
$(libdir)/pmpmodavc/libavutil.a $(libdir)/pmpmodavc/libpspmpeg.a \
$(libdir)/pmpmodavc/pmpmodavclib.a  -lexif -lm -lpspkubridge -lpspsystemctrl_user \
-lpspaudio -lpspaudiolib -lpspgu -lpspgum -lpsphprm -lpsppower -lpsprtc -lpspusb \
-lpspusbstor -lstdc++ -lpspaudio -lpspaudiocodec -lpspaudio -lpspmpeg -lpsputility 

TARGET = xReader
EXTRA_TARGETS = EBOOT.PBP
EXTRA_CLEAN = -r __SCE__xReader %__SCE__xReader xReader xReader% xReader.prx
PSP_EBOOT_TITLE = xReader
PSP_EBOOT_ICON = $(top_srcdir)/resource/ICON0.png

include $(PSPSDK)/lib/build.mak
