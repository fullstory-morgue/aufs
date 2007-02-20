
#include <stdio.h>
#include "../include/linux/aufs_type.h"

int
main(int argc, char *argv[])
{
#define p(m, v, fmt)	printf(".ds %s " fmt "\n", m, v)
#define pstr(m)		p(#m, m, "%s")
#define pint(m)		p(#m, m, "%d")
	pstr(AUFS_VERSION);
	pstr(AUFS_XINO_FNAME);
	pstr(AUFS_XINO_DEFPATH);
	pint(AUFS_DIRWH_DEF);
	pstr(AUFS_WH_PFX);
	pstr(AUFS_WKQ_NAME);
	pint(AUFS_NWKQ_DEF);
	pstr(AUFS_WH_DIROPQ);
	pstr(AUFS_WH_BASENAME);
	pint(AUFS_BRANCH_MAX);
	return 0;
}
