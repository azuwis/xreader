xReader是PSP专用读书和看图软件，支持任意大小点阵字体，支持html读取，编码转换，竖看，翻页保留行，书签及自动书签等，可以看zip和chm和rar内的文本文件，有自动降频节能(不播放mp3时)。
看图支持bmp, tga, jpg,png和gif，支持较大的图，采用两次插值和三次立方采样缩放，保证图像柔和度不失真(测试了起点的VIP图片，700*3xxx大小，缩放到适应屏幕宽，效果完美，由于缩放的算法复杂，显示点也多，所以没有降频)
完美解决文件名中文问题和长文件名问题
支持mp3播放
以上介绍纯属抄袭eReader 1.61介绍。
其实xReader是一个eReader的分枝版本。开发的本意是开发一个能在在PSP2000，4GMS卡上正常工作的eReader。
然而eReader已经停止开发。所以xReader从eReader中分裂出来成为了一个独立的软件。

xReader对eReader的改进：

1. 对eReader的兼容性：
xReader完全兼容eReader所用的字体，UI和操作方式等。 
2. 更好的稳定性：
xReader花大力气改进了eReader存在的缓冲溢出问题，修复了许多会导致eReader死机的BUG。
3. 更好的功能支持：
xReader支持自动翻页和自动滚屏功能，支持读取图像EXIF信息，支持解压缩档案文件，支持自动休眠
等等eReader不支持的功能。
4. 更好的硬件支持:
xReader支持8GMS卡，支持PSP2000（以及其64MB内存），支持最新M33版本。

未来展望：
将支持MediaEngine音乐播放，播放MP3 WMA时将不占用CPU资源。
将支持更多音频格式（OGG，MusePack，FLAC，APE，TTA等等）
将支持Graphics Engine绘图
将支持多线程自动预读功能，系统空闲自动缓存图像或文字。
将支持LRC歌词自动搜索下载
将支持TXT目录自动跳转，并提供电脑上的生成目录工具
将重构代码以便代码复用

xReader电子书目录生成工具: GenIndex
使用方法：
1. 在你的TXT电子书里加入<<<<符号标注每章开始等
如
<<<<第一章 XXX
....
<<<<第二章 YYY
2. 保存后，将TXT拖到GenIndex图标中，GenIndex就会为你的电子书生成
一个简便的目录：
===================
页数 目录
1    第一章 XXX
2    第二章 YYY
===================
3. 页数数字就是GI值，看书时在信息栏里有显示，如果你要移动到某章，翻页直到GI相等即可找到该章。GI是绝对页码，不会因为字体大小而改变。
注：GenIndex不能处理UTF8 TXT文件，请先转换为GBK编码

常见问题:

中文文件无法打开：
在PSP XMB菜单中设定中《主机设定》、《文字设定》
设置为Simpleified Chinese GBK(936)即可。

有关自动换行功能:
设置自动翻页模式为自动翻页后
自动翻页时延决定多快翻一次页,单位为秒.如果被关闭,将不会自动翻页

设置自动翻页模式为自动换行后:
自动换行步进(由于某种原因要重新开菜单才能看到)决定一次换行步进多少像素
自动换行时延决定多快换一次行,单位约为1/30秒.如果被关闭,还会自动换行
比如设置为自动翻页模式模式为自动换行,自动换行步进为1,自动换行时延为1时,是平滑滚动
设置为自动翻页模式模式为自动换行,自动换行步进为12,自动换行时延为30时,是约一秒换一行
由于PSP LCD硬件限制，请在自动滚动时把前景色和背景色设置的尽量相似，以消除抖动感。

有关自动休眠：
自动休眠在“系统选项”中“自动休眠”设置一个分钟数（一般可设置为30）。如果在这样长的时间
内不按任何键，PSP就会自动关机。对夜猫子们十分有用。

xReader 与 SCM MOD的冲突问题:
最近SCM MOD升级，解决了与xReader冲突问题，故xReader不再提供32MB版本下载。

有关编码切换:
xReader中在mp3信息条内按左+三角键切换歌词编码,原ID3编码切换改为右+三角键

解压压缩包里的文件:
在压缩包文件列表中用方块键选择欲解压的文件，按三角键，再按L键复制。
退回到文件目的目录里，按三角键，再按START键粘贴。
注意：目前无法创建中文文件名，文件名中所有的汉字会被截去，后面按上文件大小的数字。
如：“你好世界abc.txt”会被转换为“abc_12345.txt”。

有关重新编排文本：
自动换行的TXT给较小显示空间的PSP带来显示上的极大麻烦。
如果字体较大，一行显示不完。
开启了自动编排文本后，就能自动将换行和段落开始分开了。

部分ini配置解释：
global:brightness:
PSP屏幕亮度（0-100）

global:hide_flash
是否隐藏Flash0、Flash1

image:no_repeat
是否在最后一张图像显示完成时退回文件列表

text:cfont_antialias
text:efont_antialias
分别决定是否使用中文、英文TTF字体反锯齿

text:cfont_cleartype
text:efont_cleartype
分别决定是否使用中文、英文TTF字体ClearType效果

text:cfont_embolden
text:efont_embolden
分别决定是否使用中文、英文TTF字体加粗效果

text:infobar_use_ttf_mode
阅读文本时是否使用TTF字体显示信息栏

ui:usedyncolor
是否打开xReader的复活节彩蛋

music::apetagorder
APETagV2是否优先于ID3TagV2读取
