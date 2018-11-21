#ifndef _UNCOMPUTE_H
#define _UNCOMPUTE_H

#include <setjmp.h>

//#ifdef __cplusplus
//extern "C"
//{
//#endif

void _computeModule();
void _exitModule();
int _free_option(qbit **out, int nout, qbit **anc, int nanc, int ngate);

//#define Free(out,nout,anc,nanc,ngate) \
//	{ \
//		if (_free_option(out, nout, anc, nanc, ngate)) { \
//			printf("Free %d, %d \n", nout, nanc); 
//
//
//#define Nofree \
//		} else { \
//			printf("Nofree\n"); \
//		} \
//	} \
//	printf("Out\n");
//
//#ifdef __cplusplus
//}   // extern "C"
//#endif

#define Compute \
	_computeModule();
	

#define Store 
	

#define Uncompute(out,nout,anc,nanc,ngate) \
	{ \
		if (_free_option(out, nout, anc, nanc, ngate)) { \
			printf("Free %d, %d \n", nout, nanc); 

#define Free(anc,nanc) \
			declare_free(anc,nanc); \
			_exitModule();\
		}	else { \
			printf("Nofree\n"); \
			_exitModule();\
		} \
	}

#endif // _UNCOMPUTE_H
