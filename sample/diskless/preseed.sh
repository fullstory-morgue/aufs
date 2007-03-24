#!/bin/sh -

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

# $Id: preseed.sh,v 1.2 2007/03/12 01:54:56 sfjro Exp $

set -e
trap "ret=\$?; test \$ret -ne 0 && echo $0 abend \$ret" 0

case $1 in
debconf)
	cat <<- EOF
	debconf	debconf/frontend	select	Dialog
	debconf	debconf/priority	select	medium
	EOF
	;;
base)
	grep -v '#' <<- EOF || :
	adduser		adduser/homedir-permission	boolean	true
	tcpd		tcpd/paranoid-mode		boolean	false

	exim4-config	exim4/use_split_config		boolean	false
	exim4-config	exim4/dc_eximconfig_configtype	select	no configuration at this time
	exim4-config	exim4/no_config			boolean	true
# 	exim4-config	exim4/dc_eximconfig_configtype	select	local
# 	exim4-config	exim4/mailname			string	diskless
# 	exim4-config	exim4/dc_local_interfaces	string	127.0.0.1
# 	exim4-config	exim4/dc_other_hostnames	string
	exim4-config	exim4/dc_postmaster		string	${USER}

	man-db		man-db/install-setuid		boolean	false
	console-common	console-data/keymap/full	select	jp106
	console-common	console-data/keymap/policy	select	Select keymap from full list
# 	console-data	console-data/keymap/policy	select	Select keymap from full list
	EOF
	;;
base-config)
	grep -v '#' <<- EOF || :
	base-config	base-config/intro		note
	base-config	tzconfig/gmt			boolean	false
	base-config	tzconfig/geographic_area	select	Asia
# cannot preseed
	base-config	tzconfig/select_zone		select	Tokyo
# 	base-config	tzconfig/choose_country_zone_single	boolean true
# 	tzconfig/select_zone	tzconfig/select_zone	select Tokyo
# 	tzconfig/select_zone/Asia	tzconfig/select_zone	select Tokyo
# 	debian-installer/country	debian-installer/country	string Japan

	passwd	passwd/root-password		string	a
	passwd	passwd/root-password-again	string	a
	passwd	passwd/make-user		boolean	true
	passwd	passwd/user-fullname		string	${USER}
	passwd	passwd/username			string	${USER}
	passwd	passwd/user-password		string	a
	passwd	passwd/user-password-again	string	a

#	base-config	base-config/get-hostname	string	diskless
	base-config	base-config/get-hostname	string	${HOSTNAME}
	base-config	base-config/use-ppp		boolean	false
	base-config	apt-setup/uri_type		select	http
	base-config	apt-setup/country		select	Japan
	base-config	apt-setup/mirror		select	ftp2.jp.debian.org
	base-config	mirror/http/proxy		string
	base-config	mirror/suite			select	stable
	base-config	apt-setup/another		boolean	false
	base-config	apt-setup/security-updates	boolean	true

	tasksel		tasksel/first			multiselect manual package selection

# usb disabled kernel
	libusb-0.1-4	libusb-0.1-4/nousb		select

# cannot preseed
# 	dictionaries-common dictionaries-common/default-ispell	 select american (American English)
# 	dictionaries-common/default-ispell dictionaries-common/default-ispell	 select american (American English)
# 	dictionaries-common dictionaries-common/default-wordlist select american (American English)
# 	dictionaries-common/default-wordlist dictionaries-common/default-wordlist select american (American English)

	locales locales/locales_to_be_generated multiselect en_US ISO-8859-1, ja_JP.EUC-JP EUC-JP, ja_JP.UTF-8 UTF-8
	locales	locales/default_environment_locale	select	None

	ssh	ssh/protocol2_only	boolean	true
	ssh	ssh/SUID_client		boolean	true
	ssh	ssh/run_sshd		boolean	true
	EOF
	;;
custom)
	grep -v '#' <<- EOF || :
	cvs	cvs/repositories	string
#	cvs	cvs/badrepositories	select	ignore
#	cvs	cvs/pserver		boolean	false
	EOF
	;;
*)
	echo unkown $1
	false
esac
