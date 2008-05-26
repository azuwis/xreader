#!/bin/sh

DEST=/media/disk/psp/game/xReader
SRC=./src

while [ ! -x $DEST/ ]
do 
	sleep 1
done

echo "OK, PSP Connected"

#for file in bg.png fonts/fonts.zip GBK.TTF ASC.TTF
#do
#	[ ! -x $DEST/`basename "$file"` ] && 'cp' -f "$file" "$DEST"
#done

touch $DEST/
('cp' -f $SRC/EBOOT.* $SRC/*.prx $DEST/ 2>&1 > /dev/null && sudo umount /media/disk) || ('cp' -f ./EBOOT.* *.prx $DEST/ && sudo umount /media/disk)

#echo "Have fun with xReader"
