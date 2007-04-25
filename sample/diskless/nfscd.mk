
all: $(addprefix ${NfsCdDir}/, cdrom kmod ${NfsCdKo})

CdImage = $(notdir ${NfsCdUrl})
${NfsCdDir}/${CdImage}:
	cd ${NfsCdDir} && wget ${NfsCdUrl}
${NfsCdDir}/cdrom: ${NfsCdDir}/${CdImage}
	ln -f $< $@

${NfsCdDir}/kmod: ${DLModTar}
	mkdir -p $@
	cd $@ && tar -xpjf $<

${NfsCdKo}:
	tar -xpjf ${DLModTar} ${NfsCdKoFull}
	mv -i ${NfsCdKoFull} .
	find lib -depth -type d | xargs rmdir
${NfsCdDir}/%: %
	cp -pu $< $@
