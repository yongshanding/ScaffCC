#ifndef QALLOC_H
#define QALLOC_H

#include <stdlib.h>

void acquire(int n, qbit **addr) {
	qbit *new = (qbit *)malloc(sizeof(qbit)*n);
	*addr = new;
}

void release(qbit **out, int n1, qbit **anc, int n2);

#endif

