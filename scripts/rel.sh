rm -rf xReader*.rar
rm -rf PSP
mkdir -p ./PSP/GAME/xReader
cp EBOOT.PBP ./PSP/GAME/xReader
cp ../resource/bg.png ./PSP/GAME/xReader
cp ../fonts/fonts.zip ./PSP/GAME/xReader
cp ../xrPrx/xrPrx.prx ./PSP/GAME/xReader
mkdir -p ./PSP/GAME/xReader/msg
cp ../msg/*.so ./PSP/GAME/xReader/msg
cp ../Changelog.txt ./PSP/GAME/xReader
cp ../Readme.txt ./PSP/GAME/xReader
rar a -v1500k xReader.rar PSP #GenIndex/GenIndex.exe
