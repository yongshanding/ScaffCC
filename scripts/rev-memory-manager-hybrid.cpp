#include <cstdlib>    /* malloc    */
#include <cstdio>     /* printf    */
#include <iostream>
#include <fstream>
#include <iomanip>
#include <limits.h>
//#include <stddef.h>    /* offsetof  */
//#include <string.h>    /* strcpy    */
#include <string>
//#include <stdbool.h>   /* bool      */
//#include <stdint.h>    /* int64_t   */
//#include "uthash.h"    /* HASH_ADD  */
//#include <math.h>      /* floorf    */
#include <map>
#include <cmath>
#include <vector>
#include <queue>
#include <algorithm>
#include "../llvm/include/rapidjson/document.h"
#include <stack>

#define _INT_MAX 1000000
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
#define _SWAP 18
#define _FREE 19
#define _TOTAL_GATES 20

using namespace std;

// Policy switch
int allocPolicy = _GLOBAL;
int freePolicy = _NOFREE; 
int systemSize = 6600;

// DEBUG switch
bool trackGates = true;
bool debugRevMemHybrid = false;

typedef int16_t qbit_t;

// qubit struct:
typedef struct qbit_struct {
	qbit_t *addr;
	int idx;
} qbitElement_t;

typedef struct swap_gate {
	qbitElement_t op1;
	qbitElement_t op2;
	int gateIndex;
} swap;

typedef struct all_qbits_struct {
	int N;
	qbitElement_t Qubits[_MAX_NUM_QUBITS];
}	all_qbits_t;

typedef struct gate_t_str {
	string gate_name;
	int gate_id;
	vector<qbit_t*> operands;
	int num_operands;
} gate_t;

typedef struct {
	int idx;
	int nq; // num of qubits
	vector<qbit_t*> temp_addrs;
} acquire_str;

//typedef struct {
//	qbit_t *addr;
//	int idx;
//	UT_hash_handle hh;
//} q_entry_t;

all_qbits_t *AllQubits = NULL; //permanent
//q_entry_t *AllQubitsHash = NULL;
std::map<qbit_t *, int> AllQubitsHash; // permanent
all_qbits_t *TempQubits = NULL; //temporary
std::map<qbit_t *, int> TempQubitsHash; // temporary

std::map<qbit_t*, int> qubitUsage; // latest usage of qubits
std::map<qbit_t*, int> tempQubitUsage; // latest usage of temporary qubits

std::map<qbit_t*, bool> waitlist; // whether a qubit is being held to stall

std::vector<gate_t*> pendingGates;
std::vector<acquire_str*> pendingAcquires;

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

bool isWaiting(vector<qbit_t*>operands, int numOp) {
	for (int i = 0; i < numOp; i++) {
		if (waitlist[operands[i]]) {
			return true;
		}
	}
	return false;
}

void markAsWait(vector<qbit_t*>operands, int numOp) {
	// update waitlist
	for (int i = 0; i < numOp; i++) {
		waitlist[operands[i]] = true;
	}
	return;
}

void markAsReady(vector<qbit_t*>operands, int numOp) {
	// update waitlist
	for (int i = 0; i < numOp; i++) {
		waitlist[operands[i]] = false;
	}
	return;
}

bool doStall(int num_qbits, int heap_idx) {
	std::cout << "heap size: " << memoryHeap->numQubits << " total: " << AllQubits->N << "\n";
	if (num_qbits <= 1) {
		return false;
	} else if (num_qbits + AllQubits->N - memoryHeap->numQubits <= systemSize) {
		return false;
	} else {
		std::cout << "stalling " << num_qbits << " qubits.\n";
		return true;
	}
}

void updateWaitlist() {
	for (vector<acquire_str*>::iterator it = pendingAcquires.begin(); it != pendingAcquires.end(); ) {
		if (!doStall((*it)->nq, 0)) {
			markAsReady((*it)->temp_addrs, (*it)->nq);
			acquire_str *ptr = *it;
			it = pendingAcquires.erase(it);
			free(ptr);
		}
		else {
			++it;
		}
	}
}

void clearWaitlist() {
	for (vector<acquire_str*>::iterator it = pendingAcquires.begin(); it != pendingAcquires.end(); ) {
		markAsReady((*it)->temp_addrs, (*it)->nq);
		acquire_str *ptr = *it;
		it = pendingAcquires.erase(it);
		free(ptr);
	}
}


std::map<qbit_t *, int> logicalPhysicalMap;
std::map<int, qbit_t *> physicalLogicalMap;
std::vector<std::vector<int> > neighborSets;
std::vector<std::vector<int> > distanceMatrix;
std::vector<int> qubitOrdering;

void qubitsInit() {
	AllQubits = (all_qbits_t *)malloc(sizeof(all_qbits_t));
	AllQubits->N = 0; 
	TempQubits = (all_qbits_t *)malloc(sizeof(all_qbits_t));
	TempQubits->N = 0; 
}

int getPhysicalID(qbit_t *addr){
	std::map<qbit_t *, int>::iterator it = logicalPhysicalMap.find(addr);
	if (it != logicalPhysicalMap.end()) return it->second;
	else return -1;
}

qbit_t * getLogicalAddr(int qb){
	std::map<int, qbit_t *>::iterator it = physicalLogicalMap.find(qb);
	if (it != physicalLogicalMap.end()) return it->second;
	else return NULL;
}

int getDistance(qbit_t *q1, qbit_t *q2){
	return distanceMatrix[getPhysicalID(q1)][getPhysicalID(q2)];
}

/* return the index of addr, or AllQubits->N if not found */
int qubitsFind(qbit_t *newAddr) {
	//HASH_FIND(AllQubitsHash, &newAddr, s);
	//HASH_FIND_PTR(AllQubitsHash, &newAddr, s);
	std::map<qbit_t *, int>::iterator it = AllQubitsHash.find(newAddr);
	if (it == AllQubitsHash.end()) {
		//Not found
		//printf("(Warning: qubit ");
		//printf("(%p)", newAddr);
		//printf(" not found)");
		std::cout << " (Warning: qubit [" << newAddr << "] not found) ";
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
		qubitUsage.insert(std::make_pair(newAddr, 0)); // fresh qubit
	//}
}

/* return the index of addr, or AllQubits->N if not found */
int tempQubitsFind(qbit_t *newAddr) {
	//HASH_FIND(AllQubitsHash, &newAddr, s);
	//HASH_FIND_PTR(AllQubitsHash, &newAddr, s);
	std::map<qbit_t *, int>::iterator it = TempQubitsHash.find(newAddr);
	if (it == TempQubitsHash.end()) {
		//Not found
		//printf("(Warning: qubit ");
		//printf("(%p)", newAddr);
		//printf(" not found)");
		std::cout << " (Warning: qubit [" << newAddr << "] not found) ";
		return TempQubits->N;
	} else {
		return it->second;
	}
}

void tempQubitsAdd(qbit_t *newAddr) {
	//if (qubitsFind(newAddr) == TempQubits->N) {
		int newIdx = TempQubits->N;
		(TempQubits->Qubits[TempQubits->N]).addr = newAddr;
		(TempQubits->Qubits[TempQubits->N]).idx = TempQubits->N;
		TempQubits->N++;

		TempQubitsHash[newAddr] = newIdx;
		tempQubitUsage.insert(std::make_pair(newAddr, 0)); // fresh temp qubit
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

std::string gate_str[_TOTAL_GATES] = {"X", "Y", "Z", "H", "T", "Tdag", "S", "Sdag", "CNOT", "PrepZ", "MeasZ", "PrepX", "MeasX", "Fredkin", "Toffoli", "Rx", "Ry", "Rz", "swap", "free"};

void gatesInit() {
	AllGates = (size_t *)malloc(_TOTAL_GATES * sizeof(size_t));
	for (size_t i = 0; i < _TOTAL_GATES; i++) {
		AllGates[i] = 0;
	}
}

void printGateCounts() {
	std::cout << "Total number of gates by type: \n";
	for (size_t i = 0; i < _TOTAL_GATES / 2; i++) {
		std::cout << std::left << setw(8) << gate_str[i];
	}
	std::cout << "\n";
	for (size_t i = 0; i < _TOTAL_GATES / 2; i++) {
		std::cout << std::left << setw(8) <<AllGates[i];
	}
	std::cout << "\n";
	for (size_t i = _TOTAL_GATES / 2; i < _TOTAL_GATES; i++) {
		std::cout << std::left << setw(8) <<gate_str[i];
	}
	std::cout << "\n";
	for (size_t i = _TOTAL_GATES / 2; i < _TOTAL_GATES; i++) {
		std::cout << std::left << setw(8) <<AllGates[i];
	}
	std::cout << "\n";
}


void printSchedLength() {
	int maxT = 0;
	for (std::map<qbit_t*, int>::iterator it= qubitUsage.begin(); it != qubitUsage.end(); ++it) {
		if (it->second > maxT) {
			maxT = it->second;
		}
	}
	printf("==================================\n");
	printf("Total number of time steps: %d. \n", maxT);
	
}


/*****************************
* Physical Connectivity Graph 
******************************/

void initializeConnections(int num){
  for (int i = 0; i < num; i++){
    neighborSets.push_back(std::vector<int>());
		qubitOrdering.push_back(i);
  }
}

void printConnectivityGraph(){
	std::cout << "Connectivity Graph: \n";
  for (int i = 0; i < neighborSets.size(); i++){
		std::cout << "Qubit: " << i << " | ";
    for (int j = 0; j < neighborSets[i].size(); j++){
			if (j == neighborSets[i].size()-1) std::cout << neighborSets[i][j];
			else std::cout << neighborSets[i][j] << ","; 
    }
		std::cout << "\n";
 	}
}

void generateSquareGrid(int num){
	int gridLength = std::ceil(std::sqrt(num));
	int length = gridLength;
	initializeConnections(length*length);
  for (int i = 0; i < length*length; i++){
      if (((i+1)%length) > 0)
      neighborSets[i].push_back(i+1);
	if ((i)%length > 0)
      neighborSets[i].push_back(i-1);
	if (i+length < length*length)
      neighborSets[i].push_back(i+length);
	if (i-length >= 0)
      neighborSets[i].push_back(i-length);
  }
}

void readDeviceDescription(std::string deviceName){
	if (systemSize != -1){
		generateSquareGrid(systemSize);
		return;
	}
	string filename = deviceName;
	std::ifstream file(filename.c_str());
	if( !(file) ){
		std::cout << "Cannot find device description, exit...\n";
		exit(1);
	}
	rapidjson::Document doc;
	string document( (std::istreambuf_iterator<char>(file)),
				   (std::istreambuf_iterator<char>())	);
	doc.Parse(document.c_str());
	const rapidjson::Value& layout = doc["QubitLayout"];
	const rapidjson::Value& topology = doc["QubitConnectivity"];
	if(layout.IsArray()){
		for(rapidjson::SizeType i=0; i < layout.Size(); i++){
			const rapidjson::Value& entry = layout[i];
            qubitOrdering.push_back(entry.GetInt());
		}
		systemSize = qubitOrdering.size();
	}
	else{
		if(debugRevMemHybrid) 
			std::cout << "No layout specified\n";
		exit(1);
	}
	if(debugRevMemHybrid) 
			std::cout << "Completed Layout Description Import\n";
	int numQubits = topology["NumQbits"].GetInt();
	initializeConnections(numQubits);
	rapidjson::Value::ConstMemberIterator itr = topology.MemberBegin();
	++itr;
	for( ; itr!=topology.MemberEnd(); ++itr){
		int index = std::atoi(itr->name.GetString()); 
		const rapidjson::Value& list = itr->value;
		if (list.IsArray()){
			for(rapidjson::Value::ConstValueIterator it2 = list.Begin(); 
				   it2 != list.End(); ++it2){
				int neighbor = it2->GetInt();
				if (neighbor >= numQubits){
					std::cout << "Out of bounds qubit specification provided\n";
				}
				neighborSets[index].push_back(neighbor);
			}	
		}
		else if(list.IsInt()){
			int neighbor = list.GetInt();
			if (neighbor >= numQubits){
				std::cout << "Out of bounds qubit specification provided\n";
			}
			neighborSets[index].push_back(neighbor);
		}
	}
	if(debugRevMemHybrid) std::cout << "Completed Topology Description Import\n";
}

void initializeDistances(){
	for (int i = 0; i < neighborSets.size(); i++){
		distanceMatrix.push_back(std::vector<int>());
		for (int j = 0; j < neighborSets.size(); j++){
			distanceMatrix[i].push_back(0);
		}
	}
}

void bfs(int src) {
	int V = neighborSets.size();
	bool *visited = new bool[V];
	for(int i = 0; i < V; i++)
		visited[i] = false;
	
	queue<pair<int,int> > queue;
	
	visited[src] = true;
	distanceMatrix[src][src] = 0;
	queue.push(make_pair(src, 0));
	
	int i;
	
	while(!queue.empty())
	{
		pair<int, int> p = queue.front();
		int s = p.first;
		int d = p.second;
		cout << s << " ";
		queue.pop();
		
		for (i = 0; i < neighborSets[s].size(); i++)
		{
			int j = neighborSets[s][i];
			if (!visited[j])
			{
				visited[j] = true;
				distanceMatrix[src][j] = d+1;
				distanceMatrix[j][src] = d+1;
				queue.push(make_pair(j, d+1));
			}
		}
	}
}


void calculateDistances(){
	// Floyd-Warshall
	//for (int i = 0; i < neighborSets.size(); i++){
	//	for (int j = 0; j < neighborSets.size(); j++){
	//		if (i == j) distanceMatrix[i][j] = 0;
	//		if (std::find(neighborSets[i].begin(), neighborSets[i].end(), j)
	//				!= neighborSets[i].end()) distanceMatrix[i][j] = 1;
	//		else distanceMatrix[i][j] = 100000;
	//	}
	//}
	//for (int i = 0; i < neighborSets.size(); i++){
	//	for (int j = i; j < neighborSets.size(); j++){
	//		for (int k = 0; k < neighborSets.size(); k++){
	//			std::cerr << "k i j: " << k << " " << i << " " << j << "\n";
	//			int d = distanceMatrix[i][k] + distanceMatrix[k][j];
	//			if (distanceMatrix[i][j] > d){
	//				distanceMatrix[i][j] = d;
	//				distanceMatrix[j][i] = d;
	//			}
	//		}
	//	}
	//}
	// All BFS
	for (int i = 0; i < neighborSets.size(); i++) {
		bfs(i);
	}
	
}


vector<int> recoverPath(vector<int> prev, int dest){
	int u = dest;
	vector<int> path;
	while (prev[u] != -1){
		path.push_back(prev[u]);
		u = prev[u];
	}
	return path;
}

vector<pair<int,int> >buildSwaps(vector<int> path){
	//vector<pair<qbit_t*,qbit_t*> > swaps;
	vector<pair<int,int> > swaps;
	for (int i = 0; i < path.size()-1; i++){
//		pair<qbit_t*, qbit_t*> new_swap = make_pair(getLogicalAddr(path[i]),getLogicalAddr(path[i+1]));
		pair<int,int> new_swap = make_pair(path[i],path[i+1]);
		swaps.push_back(new_swap);
	}
	return swaps;
}

vector<pair<int,int> > dijkstraSearch(qbit_t *src, qbit_t *dst){
//vector<pair<qbit_t *, qbit_t *> > dijkstraSearch(qbit_t *src, qbit_t *dst){
	int source = getPhysicalID(src);
	int dest = getPhysicalID(dst);
	std::priority_queue< std::pair<int,int>, vector<std::pair<int,int> >,greater<std::pair<int,int> > > Q;
	vector<int> unvisited;
	vector<int> distances(neighborSets.size(), _INT_MAX);
	vector<int> previous(neighborSets.size(),-1);
	//for (int i = 0; i < neighborSets.size(); i++){
	//	unvisited.push_back(i);
	//}
	Q.push(make_pair(0,source));
	distances[source] = 0;
	while(!Q.empty()){
		int u = Q.top().second; 
		Q.pop();
		for (int i = 0; i < neighborSets[u].size(); i++){
			int v = neighborSets[u][i];
			if (distances[v] > distances[u] + 1){
				distances[v] = distances[u] + 1;
				previous[v] = u;
				Q.push(make_pair(distances[v], v));
			}
		}
	}
	vector<int> path_reversed = recoverPath(previous, dest);
	vector<int> path (path_reversed.size(),-1);
	int j = 0;
	for(int i = path_reversed.size()-1; i >= 0; i--){
		path[j++] = path_reversed[i];
	}
	vector<pair<int,int> > swap_chain = buildSwaps(path);
	return swap_chain;
}

/*******************************************************************
 * Resolve Interactions
 * Input: operand list and number of operands
 * Output: a list of pairs of qubits along a path between operands
 * Note: controls SWAP to target by convention
 * *****************************************************************/
//vector<pair<qbit_t *, qbit_t *> > resolveInteraction(qbit_t **operands, int num_ops){
vector<pair<int,int> > resolveInteraction(qbit_t **operands, int num_ops){
	vector<pair<int,int> > swaps;
	//vector<pair<qbit_t *, qbit_t *> > swaps;
	if (num_ops == 2){
		swaps = dijkstraSearch(operands[0],operands[1]);
	}
	return swaps;
}

void updateMaps(vector<pair<int,int> > swaps){
	for (int i = 0; i < swaps.size(); i++){
		//int phys1 = getPhysicalID(swaps[i].first);
		//int phys1 = getPhysicalID(swaps[i].first);
		qbit_t *	log1 = getLogicalAddr(swaps[i].first);
		qbit_t *	log2 = getLogicalAddr(swaps[i].second);
		logicalPhysicalMap[log1] = swaps[i].second;
		logicalPhysicalMap[log2] = swaps[i].first;
		physicalLogicalMap[swaps[i].first] = log2;
		physicalLogicalMap[swaps[i].second] = log1;
	}
}

//void printSwapChain(vector<pair<qbit_t*,qbit_t*> > swaps){
void printSwapChain(vector<pair<int,int> > swaps){
	for (int i = 0; i < swaps.size(); i++){
		//std::cout << "SWAP q" << getPhysicalID(swaps[i].first) << ", q" << getPhysicalID(swaps[i].second) << "\n";
		std::cout << "SWAP q" << swaps[i].first << ", q" << swaps[i].second << "\n";
	}
}

void printDistances(){
	std::cerr << "Distance Matrix: \n";
	for (int i = 0; i < neighborSets.size(); i++){
		for (int j = 0; j < neighborSets.size(); j++){
			std::cerr << std::left << setw(6) << distanceMatrix[i][j] << " ";
		}
		std::cerr << "\n";
	}
}

vector<qbit_t*> findClosestFree(qbit_t **targets, qbitElement_t **free, int num, int free_size, int targets_size){
	std::vector<std::pair<int,qbit_t*> > sortedFree;
	for (int i = 0; i < free_size; i++){
		int dist = 0;
		for (int j = 0; j < targets_size; j++){
			dist += getDistance(free[i]->addr, targets[j]);
		}
		sortedFree.push_back(make_pair(dist,free[i]->addr));	
	}	
	std::sort(sortedFree.begin(),sortedFree.end());
	std::vector<qbit_t *> allocated;
	for (int i = 0; i < num; i++){
		allocated.push_back(sortedFree[i].second);
	}
	return allocated;
}	


/***********************
* Memory Heap Definition  
***********************/

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

int memHeapFindQubit (memHeap_t *M, qbit_t *addr) {
	for (int i = 0; i < M->numQubits; i++){
		if (M->contents[i]->addr == addr){
			return i;
		}
	}
	return -1;
}

qbitElement_t *memHeapRemoveQubit (memHeap_t *M, qbit_t *addr) {
	int idx = memHeapFindQubit(M, addr);
	if (idx != -1){
		qbitElement_t *qubit = M->contents[idx];
		for (size_t i = idx+1; i < M->numQubits; i++) {
			M->contents[i-1] = M->contents[i];
		}
		M->numQubits--;
		return qubit;
	}
	exit(1);
}

int memHeapGetQubits(int num_qbits, memHeap_t *M, qbitElement_t *res, qbit_t **inter, int targets_size) {
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
  	if (debugRevMemHybrid) {
    	printf("Obtaining %u qubits from pool of %zu...\n", num_qbits, available);  
		}
		//vector<qbit_t *> closestSet = findClosestFree(inter, M->contents, num_qbits, available, targets_size);
		for (size_t i = 0; i < num_qbits; i++) {
			qbitElement_t *qq = memHeapPop(M);
			//qbitElement_t *qq = memHeapRemoveQubit(M, closestSet[i]);
			res[i] = *qq; // copy over the value of the struct
			free(qq); // since popped off heap, need to free
		}
		return num_qbits;
	} else {
  	if (debugRevMemHybrid) {
    	printf("Obtaining %u qubits from pool of %zu...\n", num_qbits, available);  
		}
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
	// Track the new qubits in AllQubits and the mapping
	for (size_t i = 0; i < num_qbits; i++) {
		res[i].addr = &newt[i];
		res[i].idx = AllQubits->N;
		qubitsAdd(&newt[i]);
		logicalPhysicalMap.insert(make_pair(res[i].addr,res[i].idx));
		physicalLogicalMap.insert(make_pair(res[i].idx,res[i].addr));
		waitlist.insert(make_pair(res[i].addr, false));
	}
	return num_qbits;
}

int memHeapNewTempQubits(int num_qbits, qbitElement_t *res) {
	if (res == NULL) {
		fprintf(stderr, "Result array not properly initialized.\n.");
		exit(1);
	}
	if (TempQubits->N + num_qbits > _MAX_NUM_QUBITS) {
		fprintf(stderr, "Cannot allocate more than %d total qubits.\n", _MAX_NUM_QUBITS);
		exit(1);
	}
  if (debugRevMemHybrid)
  	printf("Obtaining %u new temporary qubits.\n", num_qbits);  
	// malloc new qubits!
	qbit_t *newt = (qbit_t *)malloc(sizeof(qbit_t)*num_qbits);
	if (newt == NULL) {
    fprintf(stderr, "Insufficient memory to initialize qubit memory.\n");
    exit(1);
  }
	// Track the new qubits in AllQubits
	for (size_t i = 0; i < num_qbits; i++) {
		res[i].addr = &newt[i];
		res[i].idx = TempQubits->N;
		tempQubitsAdd(&newt[i]);
		waitlist.insert(make_pair(res[i].addr, true));
	}
	return num_qbits;
}









/*****************************
* Functions to be instrumented
******************************/
// 1. memory heap with mirroring shape: node contains heap pointer
// 2. optimize for uncompute choices

void recordGate(int gateID, qbit_t **operands, int numOp, int t) {
	AllGates[gateID]++;
	if (trackGates) {
		std::cout << t << ": " << gate_str[gateID] << " ";
		for (size_t i = 0; i < numOp; i++) {
			//printf("q%u (%p)", qubitsFind(operands[i]), operands[i]);
			//std::cout << "q" << qubitsFind(operands[i]) << " ";
			std::cout << "q" << getPhysicalID(operands[i]) << " ";
		}
		std::cout << "\n";
		//printf("heap size: %zu\n", memoryHeap->numQubits);
	}
}

/* memHeapAlloc: memory allocation when qubits are requested */
/* every function should have a heap index? for now always a global root heap*/
int  memHeapAlloc(int num_qbits, int heap_idx, qbit_t **result, qbit_t **inter, int ninter) {
	// decide if we want to stall the allocation to control parallelism/memory sharing
	if (inter == NULL) {
		std::cerr << "interaction bits are null.\n";
	} else {
		std::cerr << inter[0] << "\n";
	}
	if (doStall(num_qbits, heap_idx)) {
		qbitElement_t temp_res[num_qbits];
		int temp_new = memHeapNewTempQubits(num_qbits, &temp_res[0]);// marked as waiting
		if (temp_new != num_qbits) {
			fprintf(stderr, "Unable to initialize %u temporary qubits.\n", num_qbits);
			exit(1);
		}
		std::cerr << "here!\n";
		acquire_str *temp_acq = (acquire_str*)malloc(sizeof(acquire_str));
		std::cerr << "here!\n";
		//temp_acq->idx = pendingAcquires.size();
		//temp_acq->nq = num_qbits;
		// Store the addresses into result
		for (size_t i = 0; i < num_qbits; i++) {
			result[i] = temp_res[i].addr;
			//temp_acq->temp_addrs.push_back(temp_res[i].addr);
		}
		//pendingAcquires.push_back(temp_acq);
		return num_qbits;
	} else {
		if (heap_idx == 0) {
			// find num_qbits of qubits in the global memoryheap
			qbitElement_t res[num_qbits];
			// check if there are available in the heap
			int num = memHeapGetQubits(num_qbits, memoryHeap, &res[0], inter, ninter);
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
				logicalPhysicalMap.insert(make_pair(res[i].addr, res[i].idx));
				physicalLogicalMap.insert(make_pair(res[i].idx, res[i].addr));
			}
			return num_qbits;

		} else {
			// hierarchical heap
			fprintf(stderr, "Not implemented yet!\n");
			exit(1);
		}
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
			qIdx = tempQubitsFind(ancilla[i]);
			if (qIdx == TempQubits->N) {
				fprintf(stderr, "Cannot free qubit %p that has not been recorded.\n", ancilla[i]);
				exit(1);
			} else {
				// found temp qubits
				gate_t *free_gate = new gate_t();
				free_gate->gate_name = "free";
				free_gate->gate_id = _FREE;
				vector<qbit_t*> ops;
				for (int j = 0; j < num_qbits; j++) {
					ops.push_back(ancilla[j]);
				}
				free_gate->operands = ops;
				free_gate->num_operands = num_qbits;
				pendingGates.push_back(free_gate);
			}
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


void schedule(gate_t *new_gate) {
	int numOp = new_gate->num_operands; 
	vector<qbit_t*> operands = new_gate->operands; 
	int gateID = new_gate->gate_id;

	string gate_name = new_gate->gate_name;


	if (gate_name == "free") {
		memHeapFree(numOp, 0, &operands[0]);;
	}

	int Tmax = 0;
	for (int i = 0; i < numOp; i++) {
		int T = qubitUsage[operands[i]];
		if (T > Tmax) {
			Tmax = T;
		}
	}

	if (gate_name == "swap_chain") {
		for (int i = 0; i < numOp; i++) {
			qubitUsage[operands[i]] = Tmax+numOp-1; //modify usage in place
		}
		vector<pair<int,int> > swaps;
		for (int i = 0; i < numOp-1; i++) {
			qbit_t *swapOp[2];
			swapOp[0] = operands[i];
			swapOp[1] = operands[i+1];
			swaps.push_back(make_pair(getPhysicalID(swapOp[0]), getPhysicalID(swapOp[1])));
			recordGate(_SWAP, &swapOp[0], 2, Tmax+i+1);
		}
		updateMaps(swaps);
	}
	else {
		for (int i = 0; i < numOp; i++) {
			qubitUsage[operands[i]] = Tmax+1; //modify usage in place
		}
		qbit_t *op_arrs[numOp];
		for (int i = 0; i < numOp; i++){
			op_arrs[i] = operands[i];
		}
		recordGate(gateID, &op_arrs[0], numOp, Tmax+1);
		//if (gate_name == "swap" && numOp == 2) {
		//	int pq0 = getPhysicalID(operands[0]); //logicalPhysicalMap[operands[0]];
		//	int pq1 = getPhysicalID(operands[1]); //logicalPhysicalMap[operands[1]];
		//	vector<pair<int, int> > op_pair;
		//	op_pair.push_back(make_pair(pq0,pq1));
		//	updateMaps(op_pair);
		//}
	}

	return;	
}

void tryPendingGates() {
	if (!pendingGates.empty()) {
		//std::vector<gate_t*> to_schedule;
		std::map<qbit_t*, qbit_t*> temp2perm;
		std::map<qbit_t*, bool> perm_ready;
		for (vector<gate_t*>::iterator it = pendingGates.begin(); it != pendingGates.end(); ++it) {
			int numOp = (*it)->num_operands;
			vector<qbit_t*> ops = (*it)->operands;
			// check if ready (temp qubits no longer wait means other perm ready too)
			bool allClear = true;
			for (int i = 0; i < numOp; i++) {
				int idx = qubitsFind(ops[i]);
				if (idx != AllQubits->N) {
					// is a perm qubit
					if (perm_ready.find(ops[i]) != perm_ready.end()) {
						if (perm_ready[ops[i]] == false) {
							allClear = false;
							break;
						}
					}
				}
				idx = tempQubitsFind(ops[i]);
				if (idx != TempQubits->N) {
					// is a temp qubit
					if (temp2perm.find(ops[i]) != temp2perm.end()) {
						ops[i] = temp2perm[ops[i]]; // replace with perm
						if (perm_ready[ops[i]] == false) {
							allClear = false;
							break;
						}
					} else if (waitlist[ops[i]] == true){
						allClear = false;
						break;
					}
				}
			}
			if (allClear) {
				for (int i = 0; i < numOp; i++) {
					int idx = qubitsFind(ops[i]);
					if (idx == AllQubits->N) {
						// not found in permanent pool
						idx = tempQubitsFind(ops[i]);
						if (idx == TempQubits->N) {
							// not found in temporary pool either
							std::cerr << "Error: Qubit address not found in permanent nor temporary pool.\n";
						} else {
							// change address to permanent address
							qbit_t *perm;
							memHeapAlloc(1, 0, &perm, NULL, 0);
							temp2perm.insert(make_pair(ops[i], perm));
							ops[i] = perm;
							perm_ready.insert(make_pair(ops[i], true));
						}
					}
					waitlist[ops[i]] = false;
				}
				// schedule
				schedule((*it));

			} else {
				for (int i = 0; i < numOp; i++) {
					waitlist[ops[i]] = true;
					perm_ready[ops[i]] = false;
					//perm_ready.insert(make_pair(ops[i], false));
				}

			}

		}
	}
	return;
}




/* check and schedule a gate instruction*/
void checkAndSched(int gateID, qbit_t **operands, int numOp) {
	updateWaitlist();
	tryPendingGates();
	// check if operand in waiting qubit list
	vector<qbit_t*> ops;
	for (int i = 0; i < numOp; i++) {
		ops.push_back(operands[i]);
	}
	gate_t *new_gate = new gate_t();
	new_gate->gate_name = gate_str[gateID];
	new_gate->gate_id = gateID;
	new_gate->operands = ops;
	new_gate->num_operands = numOp;
		
	//vector<pair<qbit_t*,qbit_t*> > swaps = resolveInteraction(operands, numOp);
	vector<pair<int,int> > swaps = resolveInteraction(operands, numOp);
	//updateMaps(swaps);
	//printSwapChain(swaps);
	
	vector<gate_t*> resolvedGates;
	if (!swaps.empty()) {
		gate_t *swap_gate = new gate_t();
		swap_gate->gate_name = "swap_chain";
		swap_gate->gate_id = _SWAP;
		swap_gate->num_operands = 0;
		vector<qbit_t*> swap_ops;
		vector<pair<int,int> >::iterator it = swaps.begin();
		qbit_t *q0 =  getLogicalAddr(it->first); 
		swap_ops.push_back(q0);	
		swap_gate->num_operands++;
		for (it = swaps.begin(); it != swaps.end(); it++) {
			qbit_t *q2 = getLogicalAddr(it->second); //physicalLogicalMap[it->second];
			swap_ops.push_back(q2);

			swap_gate->num_operands++;
		}
		swap_gate->operands = swap_ops;
		resolvedGates.push_back(swap_gate);
	}
	resolvedGates.push_back(new_gate);

	for (vector<gate_t*>::iterator it = resolvedGates.begin(); it != resolvedGates.end(); ++it) {
		gate_t *this_gate = *it;
		vector<qbit_t*> this_ops = this_gate->operands;
		int this_numOp= this_gate->num_operands;
		if (isWaiting(this_ops, this_numOp)) {
			// push qubit operands to the waitlist
			markAsWait(this_ops, this_numOp);
			// push gate to the pending queue
			pendingGates.push_back(this_gate);

		} else {
			// schedule the gate at the earliest
			schedule(this_gate);
		}
	}
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
	//if (fullStack.empty()) {
	//	std::cerr << "Error: stack is empty. Cannot pop from empty stack.\n";
	//}
  if (debugRevMemHybrid)
    std::cout << "exiting scope... " << "\n";//<< fullStack.top()->func_name << "\n";
  
  //stackPop();
  //fullStack.pop();
}

//void qasm_gate () {
//}

void qasm_initialize ()
{
  if (debugRevMemHybrid)
    printf("initializing stack....\n");

  // initialize with maximum possible levels of calling depth
  //stackInit(_MAX_CALL_DEPTH);
	//bool auto_gen_graph = true;
  readDeviceDescription("DeviceDescription.json");
	std::cerr << "Device reading complete.\n";
	initializeDistances();
	calculateDistances();
	std::cerr << "Graph distances calculated.\n";
	
	//printConnectivityGraph();
	//printDistances();
  //stackInit(_MAX_CALL_DEPTH);

  memoryHeap = memHeapNew(_GLOBAL_MAX_SIZE);
	qubitsInit();
	gatesInit();
	
	//qubitUsage = new map<qbit_t*, int>();
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

  //stackPush(1); 
  //stackElement_t *se = new stackElement_t();
	//se->func_name = "main";
	//se->nInput = 0;
	//se->nAncilla = 0;
  //fullStack.push(se); //push main on to stack: other fields populated in module pass
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

	if (!pendingGates.empty()) {
		std::cout << " Stall has reached the end of program, \n start scheduling all pending gates...\n";
		clearWaitlist();
		//for (vector<gate_t*>::iterator it = pendingGates.begin(); it != pendingGates.end(); ++it) {
		//	schedule((*it));
		//}
		tryPendingGates();
	}

	printf("==================================\n");
	printf("Total number of qubits used: %u. \n", AllQubits->N);

	printSchedLength();

	printf("==================================\n");

	printGateCounts();
	memHeapDelete(memoryHeap);
	// TODO clean AllQubits
	free(AllGates);
	//print_qubit_table();

  // free allocated memory for the "stack"
  //stackDestroy();

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
