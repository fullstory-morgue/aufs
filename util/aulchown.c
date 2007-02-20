
/*
 * While this tool should be atomic, I choose loose/rough way.
 * cf. auplink and aufs.5
 * $Id: aulchown.c,v 1.1 2007/01/08 02:00:18 sfjro Exp $
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	int err, nerr;
	struct stat st;

	nerr = 0;
	while (*++argv) {
		err = lstat(*argv, &st);
		if (!err && S_ISLNK(st.st_mode))
			err = lchown(*argv, st.st_uid, st.st_gid);
		if (!err)
			continue;
		perror(*argv);
		nerr++;
	}
	return nerr;
}
