
dirs = cpup open #fsaccess
kdir = /lib/modules/$(shell uname -r)/build
#kdir = /lib/modules/2.6.20jrousD/build
#kdir = /lib/modules/2.6.19jrousD/build
#kdir = /lib/modules/2.6.18jrousD/build
#kdir = /lib/modules/2.6.17jrousND/build
#kdir = /lib/modules/2.6.16jrousND/build

all: link_probe
	for i in ${dirs}; \
	do \
		cd $${i}; \
		${MAKE} -C ${kdir} M=${CURDIR}/$${i} modules; \
		cd ..; \
		ln -f $${i}/p$${i}.ko .; \
	done

link_probe:
	for i in ${dirs}; \
	do for j in c h; \
	do test -e $${i}/probe.$${j} || ln ${CURDIR}/probe.$${j} $${i}/; \
	done; \
	done

clean:
	for i in ${dirs}; \
	do \
		cd $${i}; \
		${MAKE} -C ${kdir} M=${CURDIR}/$${i} $@; \
		cd ..; \
		${RM} p$${i}.ko; \
	done
