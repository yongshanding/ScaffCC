#ifndef _UNCOMPUTE_H
#define _UNCOMPUTE_H

#include <setjmp.h>

#ifdef __cplusplus
extern "C"
{
#endif

int _free_option(qbit **out, int nout, qbit **anc, int nanc, int ngate);

#define Free(out,nout,anc,nanc,ngate) \
	{ \
		if (_free_option(out, nout, anc, nanc, ngate)) { \
			printf("Free %d, %d \n", nout, nanc); 


#define Nofree \
		} else { \
			printf("Nofree\n"); \
		} \
	} \
	printf("Out\n");

#ifdef __cplusplus
}   // extern "C"
#endif


#endif // _UNCOMPUTE_H
