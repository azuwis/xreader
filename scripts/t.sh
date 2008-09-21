#!/bin/sh

AUTHOR=hrimfaxi
EMAIL=outmatch@gmail.com
DIRS="msg fonts"
BUILDDIR=./src
SRCDIR=$HOME/xreader/src
DESTDIR=/media/disk/PSP/game/xReader
DESTDRIVE=/media/disk
DEBUG=y

echo "xReader transfer script"
echo "Author: $AUTHOR($EMAIL)"

echo "Check the PSP is plugged in"

while [ ! -e $DESTDRIVE ]
do
	sleep 1
done

echo "OK, now we create dirs"

for dir in $DIRS
do
	echo "\tCreate dir: $DESTDIR/$dir"
	mkdir -p "$DESTDIR/$dir"
done

echo "OK, now we copy files"

echo "\tCopy files"
echo "\t\tEBOOT.PBP"
cp -u "$BUILDDIR/EBOOT.PBP" "$DESTDIR/"
if [ x"$DEBUG" = xy ]; then
	echo "\t\txReader.prx"
	cp -u "$BUILDDIR/xReader.prx" "$DESTDIR/"
fi
echo "\t\txrPrx.prx"
cp -u "$BUILDDIR/../xrPrx/xrPrx.prx" "$DESTDIR/"
#echo "\t\txr_rdriver.prx"
#cp -u "$BUILDDIR/../xr_rdriver/xr_rdriver.prx" "$DESTDIR/"
echo "\t\tfonts.zip"
cp -u "$SRCDIR/../fonts/fonts.zip" "$DESTDIR/"
echo "\t\tReadme.txt"
cp -u "$SRCDIR/../Readme.txt" "$DESTDIR/"
echo "\t\tChangelog.txt"
cp -u "$SRCDIR/../Changelog.txt" "$DESTDIR/"
echo "\t\tbg.png"
cp -u "$SRCDIR/../resource/bg.png" "$DESTDIR/"
echo "\t\tmsg/zh_CN.so"
cp -u "$SRCDIR/../msg/zh_CN.so" "$DESTDIR/msg"
echo "\t\tmsg/zh_TW.so"
cp -u "$SRCDIR/../msg/zh_TW.so" "$DESTDIR/msg"
echo "\t\tmsg/en_US.so"
cp -u "$SRCDIR/../msg/en_US.so" "$DESTDIR/msg"

touch "$DESTDIR"
sudo umount $DESTDRIVE
while test ! $? -eq 0; do
	sudo umount $DESTDRIVE
done
