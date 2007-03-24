
# $Id: Makefile,v 1.8 2007/03/12 01:55:28 sfjro Exp $

all: aufs.5 aulchown
	test -x ./mount.aufs || chmod a+x ./mount.aufs

aufs.5: aufs.in.5 ./c2tmac
	${RM} $@
	./c2tmac > $@
	cat aufs.in.5 >> $@
	chmod a-w $@

CFLAGS += -O2 -Wall ${EXTRA_CFLAGS}

.INTERMEDIATE: c2tmac.o c2tmac
c2tmac.o: c2tmac.c ../include/linux/aufs_type.h

clean:
	${RM} c2tmac aulchown aufs.5 *~ .*~
