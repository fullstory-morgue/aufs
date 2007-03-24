
# $Id: local.mk,v 1.34 2007/03/19 04:29:16 sfjro Exp $

########################################
# default values, see ./fs/aufs/Kconfig after 'make kconfig'
CONFIG_AUFS = m
CONFIG_AUFS_FAKE_DM = y
CONFIG_AUFS_BRANCH_MAX_127 = y
CONFIG_AUFS_BRANCH_MAX_511 =
CONFIG_AUFS_BRANCH_MAX_1023 =
#CONFIG_AUFS_BRANCH_MAX_32767 =
#CONFIG_AUFS_SYSAUFS = y
CONFIG_AUFS_HINOTIFY =
#CONFIG_AUFS_EXPORT =
#CONFIG_AUFS_AS_BRANCH =
#CONFIG_AUFS_DLGT =
CONFIG_AUFS_LHASH_PATCH =
CONFIG_AUFS_KSIZE_PATCH =
CONFIG_AUFS_DEBUG = y
CONFIG_AUFS_DEBUG_RWSEM =
CONFIG_AUFS_COMPAT =

AUFS_DEF_CONFIG =
-include priv_def.mk

# Ubuntu Edgy (2.6.17-10-*)
# While the kernel in Ubuntu Edgy calls itself as version 2.6.17,
# super_operations.umount_begin() operation has the interface of
# linux-2.6.18 actually. It means that the code such like,
# 	#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
# will never work correctly.
# Aufs is trying supporting this version of ubuntu kernel. Try defining
# UbuntuEdgy17Umount18 in ./local.mk.
# Honestly speaking, I am not an ubuntu user, and didn't test it by myself.
#AUFS_DEF_CONFIG = -DUbuntuEdgy17Umount18

define conf
ifdef $(1)
AUFS_DEF_CONFIG += -D$(1)
endif
endef

$(foreach i, FAKE_DM BRANCH_MAX_127 BRANCH_MAX_511 BRANCH_MAX_1023 \
	BRANCH_MAX_32767 \
	SYSAUFS HINOTIFY EXPORT AS_BRANCH DLGT \
	LHASH_PATCH \
	DEBUG DEBUG_RWSEM COMPAT, \
	$(eval $(call conf,CONFIG_AUFS_$(i))))
ifeq (${CONFIG_AUFS}, m)
AUFS_DEF_CONFIG += -DCONFIG_AUFS_MODULE -UCONFIG_AUFS
#$(eval $(call conf,CONFIG_AUFS_KSIZE_PATCH))
ifdef CONFIG_AUFS_KSIZE_PATCH
AUFS_DEF_CONFIG += -DCONFIG_AUFS_KSIZE_PATCH
endif
endif

EXTRA_CFLAGS += -I ${CURDIR}/include
EXTRA_CFLAGS += ${AUFS_DEF_CONFIG}
EXTRA_CFLAGS += -DLKTRHidePrePath=\"${CURDIR}/fs/aufs\"
export

########################################
# fake top level make

KDIR = /lib/modules/$(shell uname -r)/build
Tgt = aufs.ko aufs.5 aufind.sh mount.aufs auplink aulchown umount.aufs
ifdef CONFIG_AUFS_COMPAT
Tgt += unionctl
endif

all: ${Tgt}
FORCE:

########################################

aufs.ko: fs/aufs/aufs.ko
	test ! -e $@ && ln -s $< $@ || :
fs/aufs/aufs.ko: FORCE
#	@echo ${AUFS_DEF_CONFIG}
	${MAKE} -C ${KDIR} M=${CURDIR}/fs/aufs modules

fs/aufs/Kconfig: Kconfig.in
	@cpp -undef -nostdinc -P -I${KDIR}/include $< | \
		sed -e 's/"#"//' -e 's/^[[:space:]]*$$//' | \
		uniq > $@
kconfig: fs/aufs/Kconfig
	@echo copy all ./fs and ./include to your linux kernel source tree.
	@echo add \'obj-\$$\(CONFIG_AUFS\) += aufs/\' to linux/fs/Makefile.
	@echo add \'source \"fs/aufs/Kconfig\"\' to linux/fs/Kconfig.
	@echo then, try \'make menuconfig\' and go to Filesystem menu.

########################################

clean:
	${MAKE} -C ${KDIR} M=${CURDIR}/fs/aufs $@
	${MAKE} -C util $@
	${RM} ${Tgt}
	find . -type f \( -name '*~' -o -name '.#*[0-9]' \) | xargs -r ${RM}

util/%:
	${MAKE} -C util
aufind.sh: util/aufind.sh
	ln -s $< $@
aufs.5: util/aufs.5
	ln -s $< $@
mount.aufs: util/mount.aufs
	test -x $< || chmod a+x $<
	ln -s $< $@
unionctl: util/unionctl
	test -x $< || chmod a+x $<
	ln -s $< $@
auplink: util/auplink
	-ln -s $< $@
aulchown: util/aulchown
	-ln -s $< $@
umount.aufs: util/umount.aufs
	-ln -s $< $@

-include priv.mk
