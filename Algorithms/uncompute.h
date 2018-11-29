#ifndef _UNCOMPUTE_H
#define _UNCOMPUTE_H

#include <setjmp.h>

//#ifdef __cplusplus
//extern "C"
//{
//#endif

void _computeModule(int nout, int nanc, int ngate1, int ngate0, int degree, int r1, int r2);
void _exitModule();
int _free_option(qbit **out, int nout, qbit **anc, int nanc, int ngate1, int ngate0);
void _incr_walk();
void _decr_walk();

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

#define Compute(nout, nanc, ngate1, ngate0, degree, r1, r2) \
	_computeModule(nout, nanc, ngate1, ngate0, degree, r1, r2);
	

#define Store 
	

#define Uncompute(out,nout,anc,nanc,ngate1,ngate0) \
	{ \
		if (_free_option(out, nout, anc, nanc, ngate1, ngate0)) { \
			printf("Free %d, %d \n", nout, nanc); \
			_incr_walk();

#define Free(anc,nanc) \
			declare_free(anc,nanc); \
			_decr_walk(); \
			_exitModule();\
		}	else { \
			printf("Nofree\n"); \
			promote_free(anc,nanc); \
			_exitModule();\
		} \
	}


#define Recompute(out,nout,anc,nanc,ngate1,ngate0) \
	{ \
		int c = _free_option(out, nout, anc, nanc, ngate1, ngate0); \
		if (c) { \
			printf("Free %d, %d \n", nout, nanc); \
			_incr_walk();


#define Restore \
			_decr_walk(); \
		}

#define Unrecompute 
		
#define Refree(anc,nanc) \
		if (c) { \
			declare_free(anc,nanc); \
			_exitModule();\
		}	else { \
			printf("Nofree\n"); \
			promote_free(anc,nanc); \
			_exitModule();\
		} \
	}



#endif // _UNCOMPUTE_H
