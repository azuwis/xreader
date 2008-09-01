#!/bin/sh

cd $1

for file in *.html
do
	sed 's/<meta http-equiv="Content-Type" content="text\/html;charset=iso-8859-1">/<meta http-equiv="Content-Type" content="text\/html;charset=gbk">/g' "$file" > .t
	mv -f .t "$file"
done
