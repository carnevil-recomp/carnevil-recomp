//
// randper.c
//
// $Revision: 4 $
//
#include	<stdlib.h>

char	goose_randper_c_version[] = {"$Revision: 3 $"};

int randrng(int val)
{
	/* CarnEvil's revision 3 has no zero guard */
	return((random() % val));
}
