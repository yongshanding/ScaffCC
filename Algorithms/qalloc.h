#ifndef QALLOC_H
#define QALLOC_H

#include <stdlib.h>
#include <stdio.h>

void acquire(int n, qbit **addr, int n_inter, qbit **inter) {
	qbit *new = (qbit *)malloc(sizeof(qbit)*n);
	*addr = new;
	//qbit *new2 = (qbit *)malloc(sizeof(qbit)*n_inter);
	//*inter = new2;
	if (n_inter == 0 || inter == NULL) {
		printf("No interaction bits specified.\n");
	}
}

void release(qbit **out, int n1, qbit **anc, int n2, qbit **cpy);

void declare_free(qbit **anc, int nanc) {
	if (nanc <= 0 || anc == NULL || anc[0] == NULL) {
		printf("Invalid call to declare_free.\n");
	}
}

void promote_free(qbit **anc, int nanc) {
	if (nanc <= 0 || anc == NULL || anc[0] == NULL) {
		printf("Invalid call to promote_free.\n");
	}
}

#endif

