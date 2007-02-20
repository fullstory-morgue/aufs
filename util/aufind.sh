#!/bin/sh

mntpnt=${1}
shift

br=`
grep -w ${mntpnt}.\*xino /proc/mounts |
cut -f4 -d' ' |
tr , '\n' |
egrep '^(dirs|br)=' |
cut -f2- -d'=' |
sed -e 's/=[^:]*:*/ /g'`

find ${br} $@
