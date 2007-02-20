#!/bin/sh -

# aufs sample -- diskless system

# Copyright (C) 2006 Junjiro Okajima
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

# $Id: local.sh,v 1.1 2006/12/22 06:08:56 sfjro Exp $

set -e
tmp=/tmp/$$
trap "ret=\$?; test \$ret -ne 0 && echo $0 abend \$ret" 0

. `dirname $0`/func.sh

test $# -eq 3
test -d $1 -a -d $2 -a -d $3
#echo "${InstPkg}"
#echo "${UninstPkg}"
#exit

tgtroot=$1
pkgdir=$2
pkginfodir=$3

cd ${tgtroot}

cat << EOF > /dev/null
- do 'preseed' if you want it.
- lets start by 'Base install',
  which is numbered as 60 in
  debian-installer/doc/devel/menu-item-numbers.txt
- fake postinst script of base-installer pkg.
- fake install_base_system function in it,
  which executes initrd/usr/lib/base-installer.d/* and run-debootstrap
- run-debootstrap executes debootstrap
- fake debootstrap(read sarge script, download, extract, install)
- fake the menu-items after 'Base install'
EOF

########################################
# base-installer pkg postinst.

# check_target
#nothing
# get_mirror_info
#nothing
# install_base_system
# mkdir -p etc
# echo "# UNCONFIGURED FSTAB FOR BASE SYSTEM" >> etc/fstab
# usr/lib/base-installer.d/40netcfg
# d=etc/network
# test ! -d $d && sudo mkdir -p $d
# interfaces networks hostname resolv.conf hosts

########################################
# fake first_stage_install function of debootstrap
> $tmp.req
while read i
do
	f=${pkgdir}/var/cache/apt/archives/${i}_*.deb
	basename $f >> $tmp.req
	ar -p $f data.tar.gz | zcat | sudo tar -xf -
done < ${pkginfodir}/pkg.0req
#exit

# var/lib/dpkg/
# status available # diversions statoverride

# d=etc
# resolv.conf hostname fstab

SudoHere mount -t proc proc /proc
sudo tar -xpzf ${pkginfodir}/devices.tar.gz
f=dev/ttyS0
test -e $f || sudo mknod -m 666 $f c 4 64
f=dev/lktr
test -e $f || sudo mknod -m 666 $f c 10 63

SudoHere ldconfig
sudo ln ${pkgdir}/var/cache/apt/archives/*.deb .

d=var/lib/dpkg/info
test ! -d $d && SudoHere mkdir -p $d
# var/lib/dpkg/status $d/dpkg.list

########################################
# fake second_stage_install function of debootstrap
x_core_install() # deb
{
#	yes '' | SudoHereV dpkg --force-depends --install $@ || :
	SudoHereV dpkg --force-depends --install $@ || :
}

export TERM=vt100
export DEBIAN_FRONTEND=noninteractive
SudoHere ln -s ./mawk usr/bin/awk
{
	x_core_install base-files_*.deb base-passwd_*.deb
	x_core_install dpkg_*.deb
	SudoHere ln -sf /usr/share/zoneinfo/Japan etc/localtime
	x_core_install libc6_*.deb
	x_core_install perl-base_*.deb
	SudoHere rm usr/bin/awk
	x_core_install mawk_*.deb
	sh $OLDPWD/preseed.sh debconf | SudoHereV debconf-set-selections
	x_core_install debconf_*.deb
} > /dev/null 2>&1
#exit

n=1
while [ $n -gt 0 ]
do
	echo unpack req $n. wait for a while... > /dev/tty
	SudoHere dpkg --force-depends --unpack `cat $tmp.req` || :
	n=$(($n-1))
done > /dev/null 2>&1
#exit

# var/lib/dpkg/cmethopt
SudoHereV dpkg \
	--configure --pending --force-configure-any --force-depends \
	> /dev/null 2>&1 #|| :
#exit

cat ${pkginfodir}/pkg.0base |
while read i
do basename ${i}_*.deb
done > $tmp.base
n=1
while [ $n -gt 0 ]
do
	echo unpack base $n. wait for a while... > /dev/tty
	#yes '' |
	SudoHere dpkg \
		--force-auto-select --force-overwrite --force-confold \
		--skip-same-version --unpack `cat $tmp.base` #|| :
	n=$(($n-1))
done > /dev/null 2>&1
#exit

sh $OLDPWD/preseed.sh base | SudoHereV debconf-set-selections
d=sbin
f=start-stop-daemon
SudoHere mv $d/$f $d/$f.REAL
cat <<- EOF > $tmp
#!/bin/sh
echo
echo dummy \$0. does nothing.
EOF
chmod a+x $tmp
sudo cp $tmp $d/$f
d=usr/sbin
f=sendmail
if [ -e $d/$f ]
then
SudoHere mv $d/$f $d/$f.REAL
sudo mv $tmp $d/$f
fi

# fdutils pkg asks through tty, bypassing debconf
cmd=dpkg
cmd="env DEBIAN_FRONTEND=noninteractive $cmd"
n=5
while [ $n -gt 0 ]
do
	echo configure base $n. wait for a while... > /dev/tty
	SudoHere $cmd --force-confold --skip-same-version --configure -a #|| :
	n=$(($n-1))
done #> /dev/null 2>&1
#exit

########################################
# base-installer pkg postinst, again.

# after debootstrap in install_base_system
# fstab

# configure_apt
#nothing
# apt_update
# d=etc/apt
# ${d}/sources.list
# #SudoHere apt-get -q update
d=var/cache/apt/archives
test ! -d $d && SudoHere mkdir -p $d
sudo mv *.deb $d
# d=var/cache/apt
# sudo cp -p $pkgdir/$d/*.bin $d
d=var/lib/apt/lists
find $d | grep -q Packages ||
sudo cp -p $pkgdir/$d/*_Packages $pkgdir/$d/*_Release $d/
# f=var/lib/dpkg/available
# sudo cp -p $pkgdir/$f $f
f=etc/apt/sources.list
test -f $f || sudo cp -p $pkgdir/$f $f
SudoHereV aptitude update

# create_devices
#nothing
# pick_kernel
#nothing
# install_kernel
#nothing
# install_pcmcia_modules
#nothing
# install_extra
#nothing
# cleanup
#nothing

########################################
# fake prebaseconfig
# etc/network/options
# root/dbootstrap_settings
# f=etc/inittab
# SudoHere mv $f $f.REAL
# RootCopy ${pkgdir}/inittab etc
# SudoHere chown --reference=$f.REAL $f
echo /dev/cdrom \?

# fake cdebconf-priority
#nothing
# fake cdrom-checker
#nothing

########################################
# fake baseconfig-udeb
#set -x
# unset DEBIAN_HAS_FRONTEND # Set by debconf
# unset DEBCONF_REDIR       # Set by debconf
# unset DEBIAN_FRONTEND     # Just to be sure
# unset DEBCONF_FRONTEND    # Probably not used any more [pere 2003-01-18]
# unset LANG

#exit
sh $OLDPWD/preseed.sh base-config | SudoHereV debconf-set-selections
f=usr/bin/aptitude
SudoHere mv -i $f $f.REAL
SudoHere ln bin/true $f
tty=/dev/tty

d=usr/lib/base-config/menu
grep '^Order:' $d/*.mnu |
sort +1n |
cut -f1 -d: |
sed -e '
/finish\.mnu/,$d
s/\.mnu$//
' |
while read i
do
	SudoHereV env KEEP_DEBS=yes KEEP_BASE_DEBS=yes $i new < $tty > $tty
done 2>&1 |
SudoHere tee var/log/fake-base-config.log
SudoHere rm $f
SudoHere mv $f.REAL $f
#exit

d=var/cache/apt/archives
find $d -links 1 -name '*.deb' #|
#xargs -ri ln {} ${pkgdir}
#exit

########################################
# base-installer pkg postinst, again.

# fake di-utils-shell
#nothing

########################################
sh $OLDPWD/preseed.sh custom | SudoHereV debconf-set-selections
pkgs=
test "${InstPkg}" &&
for i in ${InstPkg}
do pkgs="${pkgs} ${i}+"
done
test "${UninstPkg}" &&
for i in ${UninstPkg}
do pkgs="${pkgs} ${i}-"
done
echo $pkgs
SudoHereV aptitude -y install ${pkgs} #> /dev/null

> $tmp
test -x usr/bin/deborphan && SudoHere deborphan > $tmp
while [ -s $tmp ]
do
	cat $tmp |
	SudoHereV xargs -r aptitude remove -y #> /dev/null
	SudoHere deborphan > $tmp
done

sh $OLDPWD/custom.sh

########################################

for i in sbin/start-stop-daemon usr/sbin/sendmail
do
	test -e $i.REAL || continue
	SudoHere rm $i
	SudoHere mv $i.REAL $i
done
SudoHere umount -n /proc
rm -fr $tmp $tmp.*
