CC = @PSP_CC@
CXX = @PSP_CXX@
CCAS = $(CC)
AR = @PSP_AR@
RANLIB = @PSP_RANLIB@
CFLAGS = 
CXXFLAGS = 

EXEEXT =
OBJEXT = o

AM_CPPFLAGS = \
-I$(srcdir) -I$(srcdir)/include -I$(srcdir)/include/freetype2 \
-I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_srcdir)/xrPrx -I$(top_srcdir)/xr_rdriver \
-I$(PSPSDK)/include

if APE
AM_CPPFLAGS += -I $(srcdir)/include/mac
endif

if OPTIMIZE
AM_CFLAGS = -O3 -G0 -fomit-frame-pointer -ffast-math -frename-registers -Wall
AM_CFLAGS += \
-funsigned-char -ffast-math -fforce-addr -fmerge-all-constants \
-floop-optimize -funsafe-loop-optimizations -ftree-loop-linear
else
AM_CFLAGS = -O0 -G0 -g -Wall
endif

AM_CXXFLAGS = $(AM_CFLAGS) -fno-exceptions -fno-rtti
CCASFLAGS = $(CFLAGS) $(INCLUDES)

noinst_PROGRAMS = xReader.elf

xReader_elf_SOURCES = \
array.c array.h bg.c bg.h bookmark.c bookmark.h \
buffer.c buffer.h charsets.c charsets.h conf.c conf.h copy.c \
copy.h ctrl.c ctrl.h dbg.c dbg.h dictionary.c dictionary.h \
display.c display.h fat.c fat.h fs.c fs.h html.c html.h image.c \
image.h iniparser.c iniparser.h location.c location.h \
pspscreen.h scene.c scene.h scene_image.c scene_impl.h \
scene_music.c scene_text.c strsafe.c strsafe.h text.c text.h \
ttfont.c ttfont.h usb.c usb.h version.h win.c win.h common/datatype.h \
common/psp_utils.c common/psp_utils.h common/qsort.c common/qsort.h \
common/utils.c common/utils.h apetaglib/APETag.c apetaglib/APETag.h \
simple_gettext.h osk.c \
osk.h exception.c exception.h m33boot.c m33boot.h passwdmgr.c passwdmgr.h \
archive.c archive.h xrPrx.S xr_RDLib.S \
depdb.c depdb.h unumd.c unumd.h ssv.c ssv.h crc16.c crc16.h \
main.c power.c power.h freq_lock.c freq_lock.h xrhal.h image_queue.h image_queue_server.c fontconfig.c fontconfig.h thread_lock.c

xrPrx.o: $(top_builddir)/xrPrx/xrPrx.S
	$(CC) -I$(PSPSDK)/include -c -o $@ $^

xr_RDLib.o: $(top_builddir)/xr_rdriver/xr_RDLib.S
	$(CC) -I$(PSPSDK)/include -c -o $@ $^

if MUSIC
xReader_elf_SOURCES += \
musicdrv.c musicdrv.h musicmgr.c musicmgr.h xmp3audiolib.c xmp3audiolib.h \
genericplayer.c genericplayer.h buffered_reader.c buffered_reader.h mp3info.h mp3info.c mp3info_buffered.c lyric.c lyric.h clock.c \
musicinfo.c musicinfo.h mediaengine.c medaiengine.h

if MP3
xReader_elf_SOURCES += \
mp3player.c mp3player.h
endif

if MPC
xReader_elf_SOURCES += \
mpcplayer.c mpcplayer.h
endif

if OGG
xReader_elf_SOURCES += \
oggplayer.c oggplayer.h
endif

if WMA
xReader_elf_SOURCES += \
wmaplayer.c wmaplayer.h sceAsfParser.S $(top_builddir)/cooleyesBridge/cooleyesBridge.S
endif

if WAV
xReader_elf_SOURCES += \
wavplayer.c wavplayer.h
endif

if FLAC
xReader_elf_SOURCES += \
flacplayer.c flacplayer.h
endif

if TTA
xReader_elf_SOURCES += \
ttaplayer.c ttaplayer.h \
tta/filter.h tta/ttadec.h tta/ttadec.c tta/ttalib.h
endif

if WAVPACK
xReader_elf_SOURCES += \
wvplayer.c wvplayer.h
endif

if APE
xReader_elf_SOURCES += \
apeplayer.cpp apeplayer.h
endif

if AT3 
xReader_elf_SOURCES += \
at3player.c at3player.h
endif

if AA3 
xReader_elf_SOURCES += \
aa3player.c aa3player.h
endif

if M4A
xReader_elf_SOURCES += \
m4aplayer.c m4aplayer.h
endif

if AAC
xReader_elf_SOURCES += \
aacplayer.c aacplayer.h
endif

endif

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
if MUSIC

if MPC
LIBS += $(libdir)/libmpcdec.a
endif

if MP3
LIBS += $(libdir)/libmad.a
endif

if OGG
LIBS += $(libdir)/libvorbisidec.a
endif

if WMA
endif

if APE
LIBS += -lmac
endif

if FLAC
LIBS += $(libdir)/libFLAC.a
endif

if WAVPACK
LIBS += $(libdir)/libwavpack.a
endif

if M4A
LIBS += $(libdir)/libmp4v2.a $(libdir)/libfaad2.a
endif

endif

LIBS += \
-L$(libdir) $(libdir)/unrar.a $(libdir)/unzip.a $(libdir)/libchm.a \
$(libdir)/libpng.a $(libdir)/libgif.a $(libdir)/libjpeg.a $(libdir)/libbmp.a \
$(libdir)/libtga.a $(libdir)/libz.a \
$(libdir)/libfreetype.a -lexif -lm -lpspkubridge -lpspsystemctrl_user \
-lpspaudio -lpspaudiolib -lpspgu -lpspgum -lpsphprm -lpsppower -lpsprtc -lpspusb \
-lpspusbstor -lstdc++ -lpspaudio -lpspaudiocodec -lpspaudio -lpsputility 

if DMALLOC
LIBS += -ldmallocth
endif

TARGET = xReader
EXTRA_TARGETS = EBOOT.PBP
EXTRA_CLEAN = -r __SCE__xReader %__SCE__xReader xReader xReader% xReader.prx
PSP_EBOOT_TITLE = xReader $(xReader_version)
PSP_EBOOT_ICON = $(top_srcdir)/resource/ICON0.png

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
