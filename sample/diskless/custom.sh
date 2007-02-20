#!/bin/sh

# aufs sample -- diskless system

# Copyright (C) 2006, 2007 Junjiro Okajima
#
# This program, aufs is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# $Id: custom.sh,v 1.2 2007/01/08 01:58:41 sfjro Exp $

set -e
tmp=/tmp/$$
. `dirname $0`/func.sh

Mv() # src dst
{
	chmod --reference=$2 $1
	sudo chown --reference=$2 $1
	sudo mv $1 $2
}

# f=etc/syslog.conf
# cp $f $tmp
# cat << EOF >> $tmp
# #authpriv,daemon,ftp,kern,lpr,mail,mark,news,syslog,user,uucp,local0,local1,local2,local3,local4,local5,local6,local7.* ${USER}
# #kern.debug ${USER}
# EOF
# Mv $tmp $f

f=etc/inittab
sed -e '
s/\(.*:\)respawn\(:.*getty.* tty[3-9].*\)/\1off\2/
s/shutdown -t1 -a -r now/shutdown -t1 -a -h -H now/
' $f > $tmp
echo 'S0:23:respawn:/sbin/getty -L ttyS0 115200 vt100' >> $tmp
Mv $tmp $f

f=etc/init.d/mountall.sh
awk '
/^mount -a/ {
	q="\x27"
	print "grep", q " / aufs " q, "/proc/mounts >> /etc/mtab";
}
{print}
' $f > $tmp
Mv $tmp $f

f=etc/init.d/umountfs
awk '
/^umount -tno/ {
	sub(/^umount -tno/, "umount -tnoaufs,no");
}
/^mount -n -o remount,ro \// {
	print "umount /initrd/branch";
	print "echo -n Flushing aufs pseudo-links...";
	print "auplink / flush";
	print "echo done.";
	sub(/^mount -n/, "mount -in");
}
{print}
' $f > $tmp
Mv $tmp $f

f=etc/init.d/halt
awk '
/^halt -d/ {
       print "set -x";
}
{print}
' $f > $tmp
Mv $tmp $f

# f=etc/dhclient.conf
# cp $f $tmp
# echo 'retry 60;' >> $tmp
# Mv $tmp $f

f=home/$USER/.bashrc
if [ -e $f ]
then
cp $f $tmp
cat << EOF >> $tmp
alias ls="ls -CF"
alias pu="pushd $1 > /dev/null"
alias po="popd $1 > /dev/null"
PATH=\${PATH}:/sbin:/usr/sbin
EOF
Mv $tmp $f
fi

# f=etc/exim4/update-exim4.conf.conf
# if [ -f $f ]
# then
# 	SudoHere cat $f > $tmp
# 	echo dc_localdelivery=\'maildir_home\' >> $tmp
# 	sudo chown --reference=$f $tmp
# 	sudo mv $tmp $f
# fi

# f=etc/exim4/conf.d/retry/30_exim4-config
# if [ -f $f ]
# then
# 	sed -e 's/,2h,15m;/,10m,1m;/' $f > $tmp
# 	sudo mv $tmp $f
# fi

f=etc/mailname
if [ -e $f ]
then
echo diskless > $tmp
Mv $tmp $f
fi

# f=var/run/utmp
# test -f $f && SudoHere rm $f

# f=hostname
# echo ${Client} > $tmp
# Update $tmp $f

# f=resolv.conf
# cat <<- EOF > $tmp
# nameserver 192.168.192.168
# EOF
# Update $tmp $f

# f=cmethopt
# echo apt apt > $tmp
# Update $tmp $f

# f=dbootstrap_settings
# cat <<- EOF > $tmp
# # inserted by prebaseconfig"
# SUITE="sarge"
# EOF
# Update $tmp $f

# f=etc/init.d/nfs-common
# test -f $f && SudoHere invoke-rc.d `basename $f` stop #> /dev/null
# # real one on this host
# f=/$f
# test -f $f && sudo invoke-rc.d `basename $f` stop #> /dev/null

f=etc/sudoers
if [ -f $f ]
then
	SudoHere cat $f > $tmp
	echo ${USER} 'ALL=(ALL) ALL' >> $tmp
	Mv $tmp $f
fi
