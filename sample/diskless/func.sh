#!/bin/sh

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

# $Id: func.sh,v 1.1 2006/12/22 06:08:56 sfjro Exp $

Sudo() # cmd...
{
	echo "$@" > /dev/tty
	sudo $@
}

SudoHere() # cmd [arg...]
{
	sudo chroot . $@
}

SudoHereV() # cmd [arg...]
{
	Sudo chroot . $@
}
