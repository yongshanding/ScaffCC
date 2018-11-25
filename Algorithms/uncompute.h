#ifndef _UNCOMPUTE_H
#define _UNCOMPUTE_H

#include <setjmp.h>

//#ifdef __cplusplus
//extern "C"
//{
//#endif

void _computeModule();
void _exitModule();
int _free_option(qbit **out, int nout, qbit **anc, int nanc, int ngate1, int ngate0);

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
	

#define Uncompute(out,nout,anc,nanc,ngate1,ngate0) \
	{ \
		if (_free_option(out, nout, anc, nanc, ngate1, ngate0)) { \
			printf("Free %d, %d \n", nout, nanc); 

#define Free(anc,nanc) \
			declare_free(anc,nanc); \
			_exitModule();\
		}	else { \
			printf("Nofree\n"); \
			promote_free(anc,nanc); \
			_exitModule();\
		} \
	}

#endif // _UNCOMPUTE_H
