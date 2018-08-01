#include <cstdlib>    /* malloc    */
#include <cstdio>     /* printf    */
#include <iostream>
//#include <stddef.h>    /* offsetof  */
//#include <string.h>    /* strcpy    */
//#include <stdbool.h>   /* bool      */
//#include <stdint.h>    /* int64_t   */
//#include "uthash.h"    /* HASH_ADD  */
//#include <math.h>      /* floorf    */
#include <map>

#define _MAX_FUNCTION_NAME 90
#define _MAX_INT_PARAMS 4
#define _MAX_DOUBLE_PARAMS 4
#define _MAX_CALL_DEPTH 16
#define _MAX_NUM_QUBITS 100000
#define _GLOBAL 0
#define _GLOBAL_MAX_SIZE 100000
#define _HIERAR 1
#define _EAGER 0
#define _LAZY 1
#define _OPT 2
#define _NOFREE 3

#define _X 0
#define _Y 1
#define _Z 2
#define _H 3
#define _T 4
#define _Tdag 5
#define _S 6
#define _Sdag 7
#define _CNOT 8
#define _PrepZ 9
#define _MeasZ 10
#define _PrepX 11
#define _MeasX 12
#define _Fredkin 13
#define _Toffoli 14
#define _Rx 15
#define _Ry 16
#define _Rz 17
#define _TOTAL_GATES 18

//using namespace std;

// Policy switch
int allocPolicy = _GLOBAL;
int freePolicy = _NOFREE; 

// DEBUG switch
bool trackGates = true;
bool debugRevMemHybrid = true;

typedef int16_t qbit_t;

// qubit struct:
typedef struct qbit_struct {
	qbit_t *addr;
	int idx;
} qbitElement_t;

typedef struct all_qbits_struct {
	int N;
	qbitElement_t Qubits[_MAX_NUM_QUBITS];
}	all_qbits_t;

//typedef struct {
//	qbit_t *addr;
//	int idx;
//	UT_hash_handle hh;
//} q_entry_t;

all_qbits_t *AllQubits = NULL;
//q_entry_t *AllQubitsHash = NULL;
std::map<qbit_t *, int> AllQubitsHash;

void qubitsInit() {
	AllQubits = (all_qbits_t *)malloc(sizeof(all_qbits_t));
	AllQubits->N = 0; 
}

/* return the index of addr, or AllQubits->N if not found */
int qubitsFind(qbit_t *newAddr) {
	//HASH_FIND(AllQubitsHash, &newAddr, s);
	//HASH_FIND_PTR(AllQubitsHash, &newAddr, s);
	std::map<qbit_t *, int>::iterator it = AllQubitsHash.find(newAddr);
	if (it == AllQubitsHash.end()) {
		//Not found
		printf("(Warning: qubit ");
		printf("(%p)", newAddr);
		printf(" not found)");
		return AllQubits->N;
	} else {
		return it->second;
	}
}

void qubitsAdd(qbit_t *newAddr) {
	//if (qubitsFind(newAddr) == AllQubits->N) {
		int newIdx = AllQubits->N;
		(AllQubits->Qubits[AllQubits->N]).addr = newAddr;
		(AllQubits->Qubits[AllQubits->N]).idx = AllQubits->N;
		AllQubits->N++;
		//q_entry_t *s = malloc(sizeof(q_entry_t));
		//s->addr = newAddr; // set the pointer key
		//s->idx = newIdx;
  	// HASH_ADD (hh_name, head, keyfield_name, key_len, item_ptr)
		//HASH_ADD(hh, AllQubitsHash, addr, sizeof(qbit_t *), s);
		//HASH_ADD_PTR(AllQubitsHash, addr, s);
		AllQubitsHash[newAddr] = newIdx;
	//}
}


/* infinite looping!
void print_qubit_table() {
  printf("<<<---------------------------------------\n");  
  printf("current qubit table:\n");
  int i;
  q_entry_t *qubit;
  for (qubit=AllQubitsHash; qubit != NULL; qubit=AllQubitsHash->hh.next) {
    printf("%p -- ", qubit->addr);
    printf("%u -- ", qubit->idx);
    //for (i=0; i<_MAX_INT_PARAMS; i++)
    // printf("%d ", memo->int_params[i]); 
    //printf("-- ");
    //for (i=0; i<_MAX_DOUBLE_PARAMS; i++)
    // printf("%f ", memo->double_params[i]);   
    //printf("-- ");       
    //for (i=0; i<3; i++)
    // printf("%llu ", memo->resources[i]);   
    printf("\n"); 
  }
  printf("--------------------------------------->>>\n");  
}
*/

size_t *AllGates = NULL;

char *gate_str[_TOTAL_GATES] = {"X", "Y", "Z", "H", "T", "Tdag", "S", "Sdag", "CNOT", "PrepZ", "MeasZ", "PrepX", "MeasX", "Fredkin", "Toffoli", "Rx", "Ry", "Rz"};

void gatesInit() {
	AllGates = (size_t *)malloc(_TOTAL_GATES * sizeof(size_t));
	for (size_t i = 0; i < _TOTAL_GATES; i++) {
		AllGates[i] = 0;
	}
}

void printGateCounts() {
	printf("Total number of gates by type: \n");
	for (size_t i = 0; i < _TOTAL_GATES / 2; i++) {
		printf("%-7s\t", gate_str[i]);
	}
	printf("\n");
	for (size_t i = 0; i < _TOTAL_GATES / 2; i++) {
		printf("%-7zu\t", AllGates[i]);
	}
	printf("\n");
	for (size_t i = _TOTAL_GATES / 2; i < _TOTAL_GATES; i++) {
		printf("%-7s\t", gate_str[i]);
	}
	printf("\n");
	for (size_t i = _TOTAL_GATES / 2; i < _TOTAL_GATES; i++) {
		printf("%-7zu\t", AllGates[i]);
	}
	printf("\n");
}

/*****************
* Stack Definition  
******************/
// The stack is in fact a call stack, but keeps frequencies of all previous parents
// so that a childs frequency would be multiplied by that of all before it

// elements on the stack are of type:
typedef int stackElement_t;

// defining a structure to act as stack for pointer values to resources that must be updated                    
typedef struct {
  stackElement_t *contents;
  int top;
  int maxSize;
} resourcesStack_t;

// declare global "resources" array address stack
resourcesStack_t *resourcesStack = NULL;

void stackInit (int maxSize) {
  resourcesStack = (resourcesStack_t*)malloc(sizeof(resourcesStack_t));
  if (resourcesStack == NULL) {
    fprintf(stderr, "Insufficient memory to initialize stack.\n");
    exit(1);
  }
  stackElement_t *newContents;
  newContents = (stackElement_t*)malloc( sizeof(stackElement_t)*maxSize );
  if (newContents == NULL) {
    fprintf(stderr, "Insufficient memory to initialize stack.\n");
    exit(1);
  }
  resourcesStack->contents = newContents;
  resourcesStack->maxSize = maxSize;
  resourcesStack->top = -1; /* i.e. empty */ 
}

void stackDestroy() {
  free(resourcesStack->contents);
  resourcesStack->contents = NULL;
  resourcesStack->maxSize = 0;
  resourcesStack->top = -1;
}

void stackPush (stackElement_t stackElement) {
  if (resourcesStack->top >= resourcesStack->maxSize - 1) {
    fprintf (stderr, "Can't push element on stack: Stack is full.\n");
    exit(1);
  }
  // insert element and update "top"
  resourcesStack->contents[++resourcesStack->top] = stackElement;
}

void stackPop () {
  if (debugRevMemHybrid)
    printf("Popping from stack\n");  
  if (resourcesStack->top < 0) {
    fprintf (stderr, "Can't pop element from stack: Stack is empty.\n");
    exit(1);    
  }

  //update "top"
  resourcesStack->top--;

}

/**********************
* Hash Table Definition
***********************/

// defining a structure that can be hashed using "uthash.h"
//typedef struct {
//  
//  char function_name[_MAX_FUNCTION_NAME];             /* these three fields */
//  int int_params[_MAX_INT_PARAMS];                    /* comprise */
//  double double_params[_MAX_DOUBLE_PARAMS];           /* the key */
//
//  // resources[0] ---> Invocation count (frequency) of module 
//  // resources[1] ---> Number of integer arguments
//  // resources[2] ---> Number of double arguments
//  int long long resources[3];                    /* hash table value field */
//
//  UT_hash_handle hh;                                  /* make this structure hashable  */
//
//} hash_entry_t;
//
//// defining multi-field key for hash table
//typedef struct {
//  char function_name[_MAX_FUNCTION_NAME];      
//  int int_params[_MAX_INT_PARAMS];             
//  double double_params[_MAX_DOUBLE_PARAMS];    
//} lookup_key_t;
//
//// how many bytes long is the key?
//const size_t keylen = _MAX_FUNCTION_NAME * sizeof(char)
//                    + _MAX_INT_PARAMS * sizeof(int)
//                    + _MAX_DOUBLE_PARAMS * sizeof(double);
//
//// declare global memoization hash table
//hash_entry_t *memos = NULL;
//
//void print_hash_table() {
//  printf("<<<---------------------------------------\n");  
//  printf("current hash table:\n");
//  int i;
//  hash_entry_t *memo;
//  for (memo=memos; memo != NULL; memo=memo->hh.next) {
//    printf("%s -- ", memo->function_name);
//    for (i=0; i<_MAX_INT_PARAMS; i++)
//     printf("%d ", memo->int_params[i]); 
//    printf("-- ");
//    for (i=0; i<_MAX_DOUBLE_PARAMS; i++)
//     printf("%f ", memo->double_params[i]);   
//    printf("-- ");       
//    for (i=0; i<3; i++)
//     printf("%llu ", memo->resources[i]);   
//    printf("\n"); 
//  }
//  printf("--------------------------------------->>>\n");  
//}
//
///* add_memo: add entry in hash table */
//void add_memo ( char *function_name,                          /* function name string */
//                int *int_params, int num_ints,           /* integer parameters */
//                double *double_params, int num_doubles,  /* double parameters */
//                int long long *resources                 /* resources for that function version */
//                ) {             
//
//  // allocate space for "memo" entry and
//  // set "memo" space to zero to avoid random byte inconsistency in lookup 
//  hash_entry_t *memo = calloc( 1, sizeof(hash_entry_t) );
//  if (memo == NULL) {
//    fprintf(stderr, "Insufficient memory to add memo.\n");
//    exit(1);
//  }  
//
//  // copy into field of "memo"
//  strcpy (memo->function_name, function_name);
//  memcpy (memo->int_params, int_params, num_ints * sizeof(int));
//  memcpy (memo->double_params, double_params, num_doubles * sizeof(double));
//  memcpy (memo->resources, resources, 3 * sizeof(int long long));
// 
//  // HASH_ADD (hh_name, head, keyfield_name, key_len, item_ptr)
//  HASH_ADD(hh, memos, function_name, keylen, memo);
//}
//
///* find_memo: find an entry in hash table */
//hash_entry_t *find_memo ( char *function_name, 
//                          int *int_params, int num_ints,
//                          double *double_params, int num_doubles
//                              ) {
//
//  // returned entry -- NULL if not found
//  hash_entry_t *memo = NULL;
//
//  // build appropriate lookup_key object
//  lookup_key_t *lookup_key = calloc(1, sizeof(lookup_key_t) );
//  if (lookup_key == NULL) {
//    fprintf(stderr, "Insufficient memory to create lookup key.\n");
//    exit(1);
//  }  
//  memset (lookup_key, 0, sizeof(lookup_key_t));
//  strcpy (lookup_key->function_name, function_name);
//  memcpy (lookup_key->int_params, int_params, num_ints * sizeof(int));
//  memcpy (lookup_key->double_params, double_params, num_doubles * sizeof(double));
//
//  HASH_FIND (hh, memos, lookup_key, keylen, memo);
//    
//
//  //free(lookup_key->double_params);  
//  //free(lookup_key->int_params);
//  //free(lookup_key->function_name);
//  free(lookup_key);
//  lookup_key = NULL;
//  return memo;
//}
//
///* delete_memo: delete a hash table entry */
//void delete_memo (hash_entry_t *memos, hash_entry_t *memo) {
//  HASH_DEL (memos, memo);
//  free(memo);
//}
//
///*void delete_all_memos() {
//  hash_entry_t *memo, *tmp;
//
//  // deletion-safe iteration
//  HASH_ITER(hh, memos, memo, tmp) {
//    HASH_DEL(memos, memo);  
//    free(memo);            
//  }
//}*/


/***********************
* Memory Heap Definition  
***********************/

// defining a structure to act as heap for pointer values to resources that must be updated                    
typedef struct memHeap_str {
  size_t maxSize; // capacity of this heap
  size_t numQubits; // number of free qubits in this heap
  qbitElement_t **contents;
	size_t numPredecessors;
	struct memHeap_str **predecessors;
	size_t numSuccessors;
	struct memHeap_str **successors;
} memHeap_t;

// declare global "resources" array address stack
memHeap_t *memoryHeap = NULL;


/* memHeapInit: initialize an empty heap for main */
memHeap_t *memHeapNew(size_t maxSize) {
  memHeap_t *newHeap = (memHeap_t*)malloc(sizeof(memHeap_t));
  if (newHeap == NULL) {
    fprintf(stderr, "Insufficient memory to initialize qubit memory heap.\n");
    exit(1);
  }
  qbitElement_t **newContents;
  newContents = (qbitElement_t**)malloc( sizeof(qbitElement_t*) * maxSize );
  if (newContents == NULL) {
    fprintf(stderr, "Insufficient memory to initialize qubit memmory heap.\n");
    exit(1);
  }
  newHeap->maxSize = maxSize;
  newHeap->numQubits = 0;
	newHeap->contents = newContents;
  newHeap->numPredecessors = 0; /* for main */ 
	newHeap->predecessors= NULL;
  newHeap->numSuccessors = 0; 
	newHeap->successors = NULL;
	return newHeap;
}

/* memHeapFind: find the index of M in array; return size of array ss if not found*/
size_t memHeapFind(memHeap_t *M, memHeap_t **array, size_t ss) {
	if (M == NULL || array == NULL) {
		return ss;
	}
	for (size_t i = 0; i < ss; i++) {
		if (M == array[i]) return i;
	}
	return ss;
}

void memHeapRemoveFromArray(size_t idx, memHeap_t **array, size_t *ss) {
	if (idx >= *ss) {
		fprintf(stderr, "Cannot remove from index that is out of bound.\n");
		exit(1);
	}
	if (array == NULL) {
		fprintf(stderr, "Cannot remove from NULL array.\n");
		exit(1);
	}
	for (size_t i = idx+1; i < *ss; i++) {
		array[i-1] = array[i];
	}
	*ss -= 1;
	return;
}

void removePredecessorFrom(memHeap_t *M, memHeap_t *from) {
	if (M == NULL || from == NULL || from->numPredecessors == 0) {
		return;
	}
	size_t np = from->numPredecessors;
	size_t idx = memHeapFind(M, from->predecessors, np);
	if (idx < np) memHeapRemoveFromArray(idx, from->predecessors, &(from->numPredecessors));
	
}

void memHeapDelete(memHeap_t *M) {
	if (M == NULL) {
		fprintf(stderr, "Cannot delete a NULL memory heap.\n");
		exit(1);
	}
	if (M->contents != NULL) {
		for (size_t i = 0; i < M->numQubits; i++) {
			if (M->contents[i] != NULL) free(M->contents[i]);
		}
  	free(M->contents);
	}
	if (M->predecessors != NULL) {
		size_t sp = M->numPredecessors;
		for (size_t i = 0; i < sp; i++) {
			size_t idx = memHeapFind(M->predecessors[i], M->predecessors, sp);
			memHeapRemoveFromArray(idx, M->predecessors, &(M->numPredecessors));
		}
		free(M->predecessors);
	}
	if (M->successors != NULL) {
		for (size_t i = 0; i < M->numSuccessors; i++) {
			if (M->successors[i] != NULL) {
				memHeapDelete(M->successors[i]);
			}
		}
		free(M->successors);
	}
  free(M);
}

// Remember to malloc newElement before calling push
void memHeapPush (qbitElement_t *newElement, memHeap_t *M) {
  if (debugRevMemHybrid)
    printf("Pushing on to heap\n");  
  if (M->numQubits >= M->maxSize) {
    fprintf (stderr, "Can't push element on heap: heap is full.\n");
    exit(1);
  }
  // insert element at the END and update numQubits
  M->contents[M->numQubits] = newElement;
	M->numQubits += 1;
}

// Remember to free result after use
qbitElement_t *memHeapPop (memHeap_t *M) {
  if (debugRevMemHybrid)
    printf("Popping from heap\n");  
  if (M->numQubits <= 0) {
    fprintf (stderr, "Can't pop element from heap: heap is empty.\n");
    exit(1);    
  }
  //update numQubits
  M->numQubits--;
	return M->contents[M->numQubits];
}

int memHeapGetQubits(int num_qbits, memHeap_t *M, qbitElement_t *res) {
	if (M == NULL) {
		fprintf(stderr, "Requesting qubits from invalid memory heap.\n");
		exit(1);
	}
	if (res == NULL) {
		fprintf(stderr, "Result array not properly initialized.\n.");
		exit(1);
	}
	size_t available = M->numQubits;
	if (num_qbits <= available) {
  	if (debugRevMemHybrid)
    	printf("Obtaining %u qubits from pool of %zu...\n", num_qbits, available);  
		for (size_t i = 0; i < num_qbits; i++) {
			qbitElement_t *qq = memHeapPop(M);
			res[i] = *qq; // copy over the value of the struct
			free(qq); // since popped off heap, need to free
		}
		return num_qbits;
	} else {
  	if (debugRevMemHybrid)
    	printf("Obtaining %u qubits from pool of %zu...\n", num_qbits, available);  
		for (size_t i = 0; i < available; i++) {
			qbitElement_t *qq = memHeapPop(M);
			res[i] = *qq; // copy over the value of the struct
			free(qq); // since popped off heap, need to free
		}
		return available;
	}
}

int memHeapNewQubits(int num_qbits, qbitElement_t *res) {
	if (res == NULL) {
		fprintf(stderr, "Result array not properly initialized.\n.");
		exit(1);
	}
	if (AllQubits->N + num_qbits > _MAX_NUM_QUBITS) {
		fprintf(stderr, "Cannot allocate more than %d total qubits.\n", _MAX_NUM_QUBITS);
		exit(1);
	}
  if (debugRevMemHybrid)
  	printf("Obtaining %u new qubits.\n", num_qbits);  
	// malloc new qubits!
	qbit_t *newt = (qbit_t *)malloc(sizeof(qbit_t)*num_qbits);
	if (newt == NULL) {
    fprintf(stderr, "Insufficient memory to initialize qubit memory.\n");
    exit(1);
  }
	// Track the new qubits in AllQubits
	for (size_t i = 0; i < num_qbits; i++) {
		res[i].addr = &newt[i];
		res[i].idx = AllQubits->N;
		qubitsAdd(&newt[i]);
	}
	return num_qbits;
}

/*****************************
* Functions to be instrumented
******************************/
//TODO!!
// 1. memory heap with mirroring shape: node contains heap pointer
// 2. optimize for uncompute choices

void recordGate(int gateID, qbit_t **operands, int numOp) {
	AllGates[gateID]++;
	if (trackGates) {
		printf("%s ", gate_str[gateID]);
		for (size_t i = 0; i < numOp; i++) {
			//printf("q%u (%p)", qubitsFind(operands[i]), operands[i]);
			printf("q%u ", qubitsFind(operands[i]));
		}
		printf("\n");
		//printf("heap size: %zu\n", memoryHeap->numQubits);
	}
}

/* memHeapAlloc: memory allocation when qubits are requested */
/* every function should have a heap index? for now always a global root heap*/
int  memHeapAlloc(int num_qbits, int heap_idx, qbit_t **result) {
	if (heap_idx == 0) {
		// find num_qbits of qubits in the global memoryheap
		qbitElement_t res[num_qbits];
		// check if there are available in the heap
		int num = memHeapGetQubits(num_qbits, memoryHeap, &res[0]);
		// malloc any extra qubits needed
		int num_new = 0;
		if (num < num_qbits) {
			num_new = memHeapNewQubits(num_qbits-num, &res[num]);
		}
		if (num + num_new != num_qbits) {
			fprintf(stderr, "Unable to initialize %u qubits.\n", num_qbits);
			exit(1);
		}
		// Store the addresses into result
		for (size_t i = 0; i < num_qbits; i++) {
			result[i] = res[i].addr;
		}
		return num_qbits;

	} else {
		// hierarchical heap
		fprintf(stderr, "Not implemented yet!\n");
		exit(1);
	}
}

/* getHeapIdx: return the index of current heap we should be looking at during runtime*/
int getHeapIdx() {
	//TODO: use the call stack to see what function are we in
	return 0;
}

int memHeapFree(int num_qbits, int heap_idx, qbit_t **ancilla) {
	// Find the qubit element corresponding to ancilla, and push them to heap
	if (ancilla == NULL) {
		fprintf(stderr, "Cannot free up NULL set of ancilla.\n");
		exit(1);
	}
	for (size_t i = 0; i < num_qbits; i++) {
		int qIdx = qubitsFind(ancilla[i]);
		if (qIdx == AllQubits->N) {
			fprintf(stderr, "Cannot free qubit %p that has not been recorded.\n", ancilla[i]);
			exit(1);
		}
		qbitElement_t *toFree = (qbitElement_t *)malloc(sizeof(qbitElement_t));
		qbitElement_t qe = AllQubits->Qubits[qIdx];
		toFree->idx = qe.idx;
		toFree->addr = qe.addr;
		if (heap_idx == 0) {
			// push on to global memoryHeap
			memHeapPush(toFree, memoryHeap);
		} else {
			// hierarchical heap
			fprintf(stderr, "Not implemented yet!\n");
			exit(1);
		}
	}
  if (debugRevMemHybrid)
  	printf("Freeing up %u qubits.\n", num_qbits);  
	return num_qbits;	
}

/* freeOnOff: return 1 if uncompute, 0 otherwise*/
int freeOnOff(int nOut, int nAnc, int nGate, int flag) {
	int weight_q = 1;
	int weight_g = 1;
	if (nOut > nAnc) {
		return 0;
	} else if (weight_q * (nAnc-nOut) < weight_g * nGate) {
		return 0;
	}
	return 1;
}

/* memoize: memoization function */
/* A call to this function ensures that the relevant entry */
/* in the hash table has been created 
int memoize ( char *function_name, 
               int *int_params, int num_ints,
               double *double_params, int num_doubles,
               int repeat
                              ) {
	// TODO: in here we add entry to function hashtable: func_idx -> {}

  static int long long total_call_count = 0;
  total_call_count++;
  //printf("Total Call Count = %d", total_call_count);

	//printf("%s \n", function_name);
  if (debugRevMemHybrid) {
    printf("memoize called on %s !\n", function_name);
    printf("repeat value = %d !\n", repeat);
  }

  hash_entry_t *memo;
  memo = find_memo(function_name, int_params, num_ints, double_params, num_doubles);

  if (memo == NULL) {
    if (debugRevMemHybrid)
      printf("NOT memoized before :(\n");
    // create entry with zero resources
    // the function call in LLVM IR will be called and will populate
    int long long *resources = calloc (3, sizeof(int long long));
    resources[0] = repeat;  // invocation count
    resources[1] = num_ints; // number of int args
    resources[2] = num_doubles; // number of double args

    // multiply by the frequency of all parents before this module, then add to memos table
    int it;
    int long long accumulative_mult = 1;
    for (it = resourcesStack->top; it > 0; it--)
      accumulative_mult *= resourcesStack->contents[it];

    resources[0] *= accumulative_mult;

    add_memo(function_name, int_params, num_ints, double_params, num_doubles, resources);

    // push this function versions resources to the top of stack
    // will use the frequency of it to multiply all children frequencies hereafter by that frequency
    if (debugRevMemHybrid)
      printf("pushing to stack: %s\n", function_name);
  }

  else {
    if (debugRevMemHybrid)
      printf("Memoized already! :)\n");
    // add to the frequency of execution (repeat times)
    int it;
    int long long accumulative_mult = 1;
    for (it = resourcesStack->top; it > 0; it--)
      accumulative_mult *= resourcesStack->contents[it];

    memo->resources[0] += repeat*accumulative_mult;
  }

  // put on the stack the individual frequency of this module
  stackPush(repeat); 
  
  // print updated hash table
  if (debugRevMemHybrid)
    print_hash_table();

  return 0;
}
*/

/* exit_scope: resource transfer function*/
/* A call to this function occurs after the module has been entered into the hash table*/
/* It will pop the last frequency off of the stack so it won't multiply anymore */
void exit_scope () 
{
  if (debugRevMemHybrid)
    printf("exiting scope...\n");
  
  stackPop();
}

//void qasm_gate () {
//}

void qasm_initialize ()
{
  if (debugRevMemHybrid)
    printf("initializing stack....\n");

  // initialize with maximum possible levels of calling depth
  stackInit(_MAX_CALL_DEPTH);

  memoryHeap = memHeapNew(_GLOBAL_MAX_SIZE);
	qubitsInit();
	gatesInit();

  // put "main" in the first row of both the hash table and the stack
  //int main_int_params[_MAX_INT_PARAMS] = {0};
  //int *main_int_params = (int*)calloc (_MAX_INT_PARAMS, sizeof(int));
  //double main_double_params[_MAX_DOUBLE_PARAMS] = {0.0};
  //double *main_double_params = calloc (_MAX_DOUBLE_PARAMS, sizeof(double));
  //int long long main_resources[3] = {0};
  //int long long *main_resources = calloc (3, sizeof(int long long));
  // main is executed once
  //main_resources[0] = 1;

	//add_memo("main                           ", main_int_params, 0, main_double_params, 0, main_resources);

  stackPush(1); 
}

void qasm_resource_summary ()
{
  // Profiling info: Total Gates, Execution Frequency, Number of Int Params, Number of Double Params
 
  int i;
  //hash_entry_t *memo;
  //for (memo=memos; memo != NULL; memo=memo->hh.next) {
  //  printf("%s ", memo->function_name);
  //  for (i=0; i<_MAX_INT_PARAMS; i++) {
  //    printf("%2d ", memo->int_params[i]); 
  //  }
  //  for (i=0; i<_MAX_DOUBLE_PARAMS; i++) {
  //    printf("%12f ", floorf(memo->double_params[i] * 10000 + 0.5) / 10000);   
  //  }
  //  printf("%8llu %8llu %8llu \n", memo->resources[0], memo->resources[1], memo->resources[2]);   
  //   
  //}

	//printf("==================================\n");
	//printf("Total number of qubits used: %u. \n", AllQubits->N);

	//printGateCounts();
	std::cout << "Test\n";
	std::cout << "Test again\n";
	memHeapDelete(memoryHeap);
	// TODO clean AllQubits
	free(AllGates);
	//print_qubit_table();

  // free allocated memory for the "stack"
  	stackDestroy();

  // free allocated memory for the "memos" table
  //delete_all_memos();
  //HASH_CLEAR(hh,memos);
}

/*
int main () {
  
  qasm_initialize();


  // create an entry
  char *function_name = "random_func                    ";
  int int_params [3] = {1, 6, -2};
  double double_params [1] = {-3.14};
  int long long resources [3] = {1,3,1};


  printf("*** main *** : memos->function_name = %s \n", memos->function_name);
  
  // insert entry
  memoize (function_name, int_params, 3, double_params, 1, 1);

  printf("*** main *** : memos->function_name = %s \n", memos->function_name);


  // find entry
  hash_entry_t *memo;
  memo = find_memo (function_name, int_params, 3, double_params, 1);
  
  if (memo == NULL)
  { printf("*** Entry not found *** \n"); return -1; }

  printf("Resources for this function: \n");
  int i;
  for (i=0; i<3; i++)
    printf("%llu, ", memo->resources[i]);
  printf("\n");

  memoize(function_name, int_params, 3, double_params, 1, 4);
  qasm_resource_summary();

  return 0;
}
*/
