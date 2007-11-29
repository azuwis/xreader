# $Id: Makefile.PSP 79 2007-11-07 17:13:49Z soarchin $

# Copyright 2007 aeolusc

# This file is part of eReader2.

# eReader2 is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# eReader2 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

TARGET_LIB = eReader2lib.a
OBJS=AA3.o CharsetsCpp.o Compat.o Filesys.o IORead.o MP3Cpp.o Music.o Thread.o Utils.o musicwrapper.o

INCDIR = $(PSPSDK)/../include ../

PSP_FW_VERSION=371
CFLAGS = -O0 -g -Wall #-DDISPLAY_16BIT_DEPTH
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

PSPSDK=$(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak
