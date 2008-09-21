# $Id: Revision.sh 74 2007-10-20 10:46:39Z soarchin $

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

#!/bin/sh

test "$1" && srcdir="$1/"

svn_revision=`LC_ALL=C svn info 2> /dev/null | grep Revision | cut -d' ' -f2`
test $svn_revision || svn_revision=`grep revision ${srcdir}.svn/entries 2>/dev/null | cut -d '"' -f2`
test $svn_revision || svn_revision=`sed -n -e '/^dir$/{n;p;q;}' ${srcdir}.svn/entries 2>/dev/null`
test $svn_revision && revision=SVN-r$revision

# check for git short hash
if ! test $svn_revision; then
    revision=`cd "${srcdir}" && git log -1 --pretty=format:%h`
    test $revision && revision=git-$revision
fi


NEW_REVISION="#define REVISION \"${revision}\""
OLD_REVISION=`cat ./Revision.h 2> /dev/null`

# Update version.h only on revision changes to avoid spurious rebuilds
if test "$NEW_REVISION" != "$OLD_REVISION"; then
    echo "$NEW_REVISION" > ./Revision.h
fi
