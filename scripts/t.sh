#!/bin/sh

DEST=/media/disk/psp/game/xReader

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
'cp' -f EBOOT.* $DEST/ && sudo umount /media/disk

#echo "Have fun with xReader"
