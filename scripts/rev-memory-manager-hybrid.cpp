//#include <cstdlib>    /* malloc    */
//#include <cstdio>     /* printf    */
#include <iostream>
#include <fstream>
#include <iomanip>
//#include <limits.h>

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
#define _EXT 4

#define _LIFO 0
#define _MINQ 1
#define _HALF 2
#define _CLOSEST_BLOCK 3
#define _CLOSEST_QUBIT 4

#define _SWAP_CHAIN 0
// _SWAP_CHAIN=0 means assuming manhattan path: works on linear or grid topology
// _SWAP_CHAIN=1 means assuming shortest path: works on any topology, but slow

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
int allocPolicy = _LIFO;
int freePolicy = _EXT; 
bool swapAlloc = false; // not this flag
int systemSize = 21609; // perfect square number

// DEBUG switch
bool trackGates = true;
bool debugRevMemHybrid = true;
bool swapflag = true;


int num_gate_scheduled = 0;

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

/* Data structures for making freeOnOff decisions */
int CURRENT_IDX = 0; // how many times I have called freeOnOff
int current_level = 0; // root is level 0
typedef struct callnode_t {
	int is_root; // indicate is main
	int on_off; // decision for this node: -1: no value, 0: no uncomp, 1: uncomp
	callnode_t *parent;
	// A doublly linked list of the children calls, ordered in program order
	callnode_t *children_start;  
	callnode_t *children_end; 
	callnode_t *child_current;
	callnode_t *next;
	callnode_t *prev;
} callnode_t;

callnode_t *current_node = NULL;

callnode_t *callGraph = NULL;

callnode_t *callGraphNew() {
  callnode_t *newGraph = (callnode_t*)malloc(sizeof(callnode_t));
  if (newGraph == NULL) {
    fprintf(stderr, "Insufficient memory to initialize call graph.\n");
    exit(1);
  }
	newGraph->is_root = 1;
	newGraph->on_off = -1;
	newGraph->child_current= NULL;
	newGraph->parent = NULL;
	newGraph->children_start = NULL;
	newGraph->children_end = NULL;
	newGraph->prev = NULL; // siblings
	newGraph->next = NULL; // siblings
	return newGraph;
}

void callGraphDelete(callnode_t *cg) {
	if (cg == NULL) {
		fprintf(stderr, "Cannot delete a NULL call graph.\n");
		exit(1);
	}
	//TODO: free internal lists
	free(cg);
	
}

void computeNode() {
	if (current_node == NULL) {
		fprintf(stderr, "Call graph has not been initialize yet.\n");
		exit(1);
	}
	if (current_node->on_off == -1) {
		//TODO: create node
		callnode_t *newNode = (callnode_t*)malloc(sizeof(callnode_t));
		newNode->parent = current_node;
		current_node = newNode; // change global current pointer
		newNode->is_root = 0;
		newNode->on_off = -1;
		newNode->child_current = NULL;
		newNode->children_start = NULL;
		newNode->children_end = NULL;
		callnode_t *cstart= newNode->parent->children_start;
		callnode_t *cend = newNode->parent->children_end;
		if (cend == NULL) {
			newNode->prev = NULL;
			newNode->next = NULL;
			newNode->parent->children_start = newNode;
			newNode->parent->children_end = newNode;
		} else {
			newNode->prev = cend;
			cend->next = newNode;
			newNode->next = NULL;
			newNode->parent->children_end = newNode;
		}
	} else {
		//TODO: walk to current child 
		if (current_node->child_current == NULL){
			current_node->child_current = current_node->children_end;
		}
		callnode_t *tmp = current_node->child_current;
		current_node->child_current = tmp->prev;
		current_node = tmp;
	}
}

void exitNote() {
	if (current_node == NULL || current_node->parent == NULL) {
		fprintf(stderr, "There is no parent to return to.\n");
		exit(1);
	}

	current_node = current_node->parent;
}

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
	if (debugRevMemHybrid) { 
		std::cout << "heap size: " << memoryHeap->numQubits << " total: " << AllQubits->N << "\n";
	}
	if (num_qbits <= 1) {
		return false;
	} else if (num_qbits + AllQubits->N - memoryHeap->numQubits <= systemSize) {
		return false;
	} else {
		if (debugRevMemHybrid) { 
			std::cout << "stalling " << num_qbits << " qubits.\n";
		}
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
std::vector<int> unusedQubits;
int CoM = 0; // physical id of center of mass of all qubits so far, init by calculateDistances

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
		unusedQubits.push_back(i);
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
			//unusedQubits.push_back(i);
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
            unusedQubits.push_back(entry.GetInt());
		}
		systemSize = unusedQubits.size();
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

int bfs(int src) {
	int acc = 0; // accumulative sum of distances to all other nodes
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
		//cout << s << " ";
		queue.pop();
		
		for (i = 0; i < neighborSets[s].size(); i++)
		{
			int j = neighborSets[s][i];
			if (!visited[j])
			{
				visited[j] = true;
				distanceMatrix[src][j] = d+1;
				distanceMatrix[j][src] = d+1;
				acc += d+1;
				queue.push(make_pair(j, d+1));
			}
		}
	}
	return acc;
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
	int min_dist = systemSize*systemSize;
	int min_i = 0;
	for (int i = 0; i < neighborSets.size(); i++) {
		int d = bfs(i);
		if (d < min_dist) {
			min_i = i;
			min_dist = d;
		}
	}
	CoM = min_i;
	
}

/* find physical index of node in subgraph that's closest to center of mass of targets*/
int findCoM(vector<int> targets, vector<int> subgraph) {
	int min_d = systemSize * systemSize;
	int min_i = 0;
	for (vector<int>::iterator sit = subgraph.begin(); sit != subgraph.end(); ++sit) {
		int sum_d = 0;
		for (vector<int>::iterator tit = targets.begin(); tit != targets.end(); ++tit) {
			sum_d += distanceMatrix[*sit][*tit];
		}
		if (sum_d <= min_d) {
			min_d = sum_d;
			min_i = *sit;
		}
	}
	return min_i;
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
	//std::cerr << "path1 " << path.size() << "\n";
	//if (path.size() == 0)
	for (int i = 0; i+1 < path.size(); i++){

		//std::cerr << "path2 " << path.size() << "\n";
//		pair<qbit_t*, qbit_t*> new_swap = make_pair(getLogicalAddr(path[i]),getLogicalAddr(path[i+1]));
		pair<int,int> new_swap = make_pair(path[i],path[i+1]);
		swaps.push_back(new_swap);
	}
	//std::cerr << "path3 " << path.size() << "\n";
	return swaps;
}

vector<pair<int,int> > dijkstraSearch(qbit_t *src, qbit_t *dst){
//vector<pair<qbit_t *, qbit_t *> > dijkstraSearch(qbit_t *src, qbit_t *dst){
	int source = getPhysicalID(src);
	int dest = getPhysicalID(dst);
	//std::cerr << "resolve src: " << source << " dst: " << dest << "\n";
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
			if (getLogicalAddr(v) != NULL) { // only used qubits
				if (distances[v] > distances[u] + 1){
					distances[v] = distances[u] + 1;
					previous[v] = u;
					Q.push(make_pair(distances[v], v));
				}
			}
		}
	}
	vector<int> path_reversed = recoverPath(previous, dest);
	//std::cerr << "size: " << path_reversed.size() << "\n";
	vector<int> path (path_reversed.size(),-1);
	int j = 0;
	for(int i = path_reversed.size()-1; i >= 0; i--){
		path[j++] = path_reversed[i];
	}
	vector<pair<int,int> > swap_chain = buildSwaps(path);
	return swap_chain;
}

vector<pair<int,int> > manhattan(qbit_t *src, qbit_t *dst){

	int source = getPhysicalID(src);
	int dest= getPhysicalID(dst);
	//std::cout << "source: " << source << " dest: " << dest << "\n";
	// Assume the 2-D grid is labelled as row major.
	int sideLength = std::ceil(std::sqrt(systemSize));
	int s_r = source / sideLength;
	int s_c = source % sideLength;
	int d_r = dest / sideLength;
	int d_c = dest % sideLength;
	// X-Y routing: priorize on y direction, so move up/down first, then left/right
	int delta_r = d_r - s_r; // +: down; -: up
	int delta_c = d_c - s_c; // +: right; -: left
	int abs_r = abs(delta_r);
	int abs_c = abs(delta_c);
	vector<int> path;
	if (abs_r + abs_c > 1) {
		//path.push_back(source);
		int sign_r = (d_r >= s_r)? 1: -1;
		int sign_c = (d_c >= s_c)? 1: -1;
		//if (abs_r != 0) {
		//	sign_r = delta_r/abs_r;
		//}
		//if (abs_c != 0) {
		//	sign_c = delta_c/abs_c;
		//}
		int new_r = s_r;
		int new_c = s_c;
		for (int r = 0; r <= abs_r; r++) {
			new_r = s_r + sign_r * r;
			if (!(new_r == d_r && new_c == d_c)) {
				path.push_back(new_r * sideLength + s_c);
			}
		}
		for (int c = 0; c <= abs_c; c++) {
			new_c = s_c + sign_c * c;
			if ((path.back() != new_r*sideLength+new_c) && (new_r != d_r || new_c != d_c)) {
				path.push_back(new_r * sideLength + new_c);
			}
		}
		if (new_c != d_c || new_r != d_r){
			std::cout << "Error: manhattan path construction error.\n";
		}
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
	if (num_ops == 2 && _SWAP_CHAIN == 1){
		swaps = dijkstraSearch(operands[0],operands[1]);
	}
	if (num_ops == 2 && _SWAP_CHAIN == 0){
		swaps = manhattan(operands[0],operands[1]);
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

vector<int> findClosestNew(int center, int num, int *sum_dist){
	std::vector<std::pair<int,int> > sortedFree;// dist,physicalID
	int unusedQubits_size = unusedQubits.size();
	//std::cerr << "findClosestNew(" << num << ")\n";
	for (int i = 0; i < unusedQubits_size; i++){
		//std::cerr << unusedQubits[i] << "\t";
		int dist = distanceMatrix[center][unusedQubits[i]];
		
		sortedFree.push_back(make_pair(dist,unusedQubits[i]));	
	}	
	std::sort(sortedFree.begin(),sortedFree.end());
	std::vector<int> allocated;
	//std::cerr << "\nallocated\n";
	for (int i = 0; i < num; i++){
		//std::cerr << sortedFree[i].second << "\t"; 
		allocated.push_back(sortedFree[i].second);
		*sum_dist += sortedFree[i].first;
	}
	//std::cerr << "\n";
	return allocated;
}	

vector<qbit_t*> findClosestFree(int center, qbitElement_t **free, int num, int free_size, int targets_size, int *sum_dist){
	std::vector<std::pair<int,qbit_t*> > sortedFree;
	for (int i = 0; i < free_size; i++){
		//int dist = 0;
		//for (int j = 0; j < targets_size; j++){
		//	dist += getDistance(free[i]->addr, targets[j]);
		//}
		int dist = distanceMatrix[center][getPhysicalID(free[i]->addr)];
		sortedFree.push_back(make_pair(dist,free[i]->addr));	
	}	
	std::sort(sortedFree.begin(),sortedFree.end());
	std::vector<qbit_t *> allocated;
	for (int i = 0; i < num; i++){
		allocated.push_back(sortedFree[i].second);
		*sum_dist += sortedFree[i].first;
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


/* closest qubits block (heap or new)*/
int memHeapClosestQubits(int num_qbits, memHeap_t *M, qbitElement_t *res, qbit_t **inter, int targets_size) {
	if (M == NULL) {
		fprintf(stderr, "Requesting qubits from invalid memory heap.\n");
		exit(1);
	}
	if (res == NULL) {
		fprintf(stderr, "Result array not properly initialized.\n.");
		exit(1);
	}
	if (num_qbits == 0) {
		return 0;
	}
	size_t available = M->numQubits;
	vector<qbit_t *> closestSet;

	int new_dist = 0;
	int subsize = M->numQubits + unusedQubits.size();
	vector<int> subgraph;
	for (int i = 0; i < subsize; i++) {
		int heapsize = M->numQubits;
		if (i < heapsize) {
			subgraph.push_back(getPhysicalID(M->contents[i]->addr));
		} else {
			subgraph.push_back(unusedQubits[i - heapsize]);
		}
	}
	vector<int> targets;
	for (int i = 0; i < targets_size; i++) {
		targets.push_back(getPhysicalID(inter[i]));
	}
	int CoM_inter = findCoM(targets, subgraph);
	vector<int> closestNewSet = findClosestNew(CoM_inter, num_qbits, &new_dist);

	if (num_qbits <= available) {
  	if (debugRevMemHybrid) {
    	printf("Obtaining %u qubits from pool of %zu...\n", num_qbits, available);  
		}
		int free_dist = 0;
		vector<qbit_t *> closestFreeSet = findClosestFree(CoM_inter, M->contents, num_qbits, available, targets_size, &free_dist);
		//std::cerr << "free dist: " << free_dist << " new dist: " << new_dist << "\n";
		if (free_dist <= new_dist) {
			//std::cerr << "choosing free\n";
			closestSet = closestFreeSet;
			for (size_t i = 0; i < num_qbits; i++) {
				qbitElement_t *qq;
				if (allocPolicy == _LIFO) {
					qq = memHeapPop(M);
				} else {
					qq = memHeapRemoveQubit(M, closestSet[i]);
				}
				res[i] = *qq; // copy over the value of the struct
				free(qq); // since popped off heap, need to free
			}
		} else {
			//std::cerr << "choosing new\n";
			vector<int> physicalIDs = closestNewSet;
		  if (debugRevMemHybrid)
		  	printf("Obtaining %u new qubits.\n", num_qbits);  
			// malloc new qubits!
			qbit_t *newt = (qbit_t *)malloc(sizeof(qbit_t)*num_qbits);
			if (newt == NULL) {
		    fprintf(stderr, "Insufficient memory to initialize qubit memory.\n");
		    exit(1);
		  }
			for (size_t i = 0; i < num_qbits; i++) {
				unusedQubits.erase(std::remove(unusedQubits.begin(), unusedQubits.end(), physicalIDs[i]), unusedQubits.end());
				res[i].addr = &newt[i];
				res[i].idx = AllQubits->N;
				qubitsAdd(&newt[i]);
				logicalPhysicalMap.insert(make_pair(res[i].addr,physicalIDs[i]));
				physicalLogicalMap.insert(make_pair(physicalIDs[i],res[i].addr));
				waitlist.insert(make_pair(res[i].addr, false));
			}
		}


	} else {
		vector<int> physicalIDs = closestNewSet;
	  if (debugRevMemHybrid)
	  	printf("Obtaining %u new qubits.\n", num_qbits);  
		// malloc new qubits!
		qbit_t *newt = (qbit_t *)malloc(sizeof(qbit_t)*num_qbits);
		if (newt == NULL) {
	    fprintf(stderr, "Insufficient memory to initialize qubit memory.\n");
	    exit(1);
	  }
		for (size_t i = 0; i < num_qbits; i++) {
			unusedQubits.erase(std::remove(unusedQubits.begin(), unusedQubits.end(), physicalIDs[i]), unusedQubits.end());
			res[i].addr = &newt[i];
			res[i].idx = AllQubits->N;
			qubitsAdd(&newt[i]);
			logicalPhysicalMap.insert(make_pair(res[i].addr,physicalIDs[i]));
			physicalLogicalMap.insert(make_pair(physicalIDs[i],res[i].addr));
			waitlist.insert(make_pair(res[i].addr, false));
		}
	}
	return num_qbits;
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
	if (num_qbits == 0) {
		return 0;
	}
	size_t available = M->numQubits;
	vector<qbit_t *> closestSet;
	if (num_qbits <= available) {
  	if (debugRevMemHybrid) {
    	printf("Obtaining %u qubits from pool of %zu...\n", num_qbits, available);  
		}
		if (allocPolicy != _LIFO) {
			int free_dist = 0;
			int subsize = M->numQubits;
			vector<int> subgraph;
			for (int i = 0; i < subsize; i++) {
				int heapsize = M->numQubits;
				subgraph.push_back(getPhysicalID(M->contents[i]->addr));

			}
			vector<int> targets;
			for (int i = 0; i < targets_size; i++) {
				targets.push_back(getPhysicalID(inter[i]));
			}
			int inter_center = findCoM(targets, subgraph);
			closestSet = findClosestFree(inter_center, M->contents, num_qbits, available, targets_size, &free_dist);
		}
		for (size_t i = 0; i < num_qbits; i++) {
			qbitElement_t *qq;
			if (allocPolicy == _LIFO) {
				qq = memHeapPop(M);
			} else {
				qq = memHeapRemoveQubit(M, closestSet[i]);
			}
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
  //printf("Obtain\n");  
  int sum_dist;
	vector<int> physicalIDs = findClosestNew(CoM, num_qbits, &sum_dist);
  //printf("Obtained\n");  
  //std::cerr << physicalIDs.size() << "\n";
	for (size_t i = 0; i < num_qbits; i++) {
		unusedQubits.erase(std::remove(unusedQubits.begin(), unusedQubits.end(), physicalIDs[i]), unusedQubits.end());
		res[i].addr = &newt[i];
		res[i].idx = AllQubits->N;
		qubitsAdd(&newt[i]);
		logicalPhysicalMap.insert(make_pair(res[i].addr,physicalIDs[i]));
		physicalLogicalMap.insert(make_pair(physicalIDs[i],res[i].addr));
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
	}// else {
	//	std::cerr << inter[0] << "\n";
	//}
	//std::cerr << "memHeapAlloc " << num_qbits << "\n";
	if (doStall(num_qbits, heap_idx)) {
		//std::cerr << "stall here?\n";
		qbitElement_t temp_res[num_qbits];
		int temp_new = memHeapNewTempQubits(num_qbits, &temp_res[0]);// marked as waiting
		if (temp_new != num_qbits) {
			fprintf(stderr, "Unable to initialize %u temporary qubits.\n", num_qbits);
			exit(1);
		}
		//std::cerr << "here!\n";
		acquire_str *temp_acq = (acquire_str*)malloc(sizeof(acquire_str));
		//std::cerr << "here!\n";
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
		//std::cerr << "Allocating " << num_qbits << " qubits.\n";
		if (heap_idx == 0) {
			// find num_qbits of qubits in the global memoryheap
			qbitElement_t res[num_qbits];
			// check if there are available in the heap
			//std::cerr << "haha\n";
			int heap_num;
			if (allocPolicy == _MINQ) {
				heap_num = num_qbits;
			} else if (allocPolicy == _HALF){
				heap_num = num_qbits / 2;
			} else if (allocPolicy == _CLOSEST_QUBIT) {
				for (int j = 0; j < num_qbits; j++) {
					int num = memHeapClosestQubits(1, memoryHeap, &res[j], inter, ninter); 
					if (num != 1) {
						fprintf(stderr, "Unable to initialize %u qubits.\n", 1);
						exit(1);
					}
					// Store the addresses into result
					//for (size_t i = 0; i < num_qbits; i++) {
					result[j] = res[j].addr;
						//logicalPhysicalMap.insert(make_pair(res[i].addr, res[i].idx));
						//physicalLogicalMap.insert(make_pair(res[i].idx, res[i].addr));
					//}
					//std::cerr << "hihi\n";
				}
				return num_qbits;

			} else if (allocPolicy == _CLOSEST_BLOCK) {

				int num = memHeapClosestQubits(num_qbits, memoryHeap, &res[0], inter, ninter); 
				if (num != num_qbits) {
					fprintf(stderr, "Unable to initialize %u qubits.\n", num_qbits);
					exit(1);
				}
				// Store the addresses into result
				for (size_t i = 0; i < num_qbits; i++) {
					result[i] = res[i].addr;
					//logicalPhysicalMap.insert(make_pair(res[i].addr, res[i].idx));
					//physicalLogicalMap.insert(make_pair(res[i].idx, res[i].addr));
				}
				//std::cerr << "hihi\n";
				return num_qbits;

			} else {
				heap_num = num_qbits;
			}

			int num = memHeapGetQubits(heap_num, memoryHeap, &res[0], inter, ninter);
			// malloc any extra qubits needed
			int num_new = 0;
			if (num < num_qbits) {
				//std::cerr << "hehe\n";
				num_new = memHeapNewQubits(num_qbits-num, &res[num]);
			}
			if (num + num_new != num_qbits) {
				fprintf(stderr, "Unable to initialize %u qubits.\n", num_qbits);
				exit(1);
			}
			// Store the addresses into result
			for (size_t i = 0; i < num_qbits; i++) {
				result[i] = res[i].addr;
				//logicalPhysicalMap.insert(make_pair(res[i].addr, res[i].idx));
				//physicalLogicalMap.insert(make_pair(res[i].idx, res[i].addr));
			}
			//std::cerr << "hihi\n";
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

int exhaustiveOnOff(int index){
  string filename = "on_off_sequences.txt";
  string outfilename = "on_off_sequences_temp.txt";
	std::ifstream file(filename.c_str());
	std::ofstream outfile(outfilename.c_str());
	if( !(file) ){
		std::cout << "Cannot find on_off_sequences.txt file\n";
		exit(1);
	}
	vector<string> vec;
	string seq;
	string current_seq;
	int iter = 0;
	getline(file,current_seq);
//	while (getline(file, seq)) {
//			if (iter > 1) outfile << seq;
//			else current_seq = seq;
//	}
	
  if (debugRevMemHybrid) {
		std::cout << " Controlled Uncompute Index: " << index << " ";
		std::cout << current_seq[index] << "\n";
	}
//	std::remove(filename);
//	std::rename(outfilename,filename);
	int val = int(current_seq[index] - '0');


	return val;
}

/* freeOnOff: return 1 if uncompute, 0 otherwise*/
/* In nested call structure, the freeOnOff decisions in compute-uncompute pair 
 * should always be the same.
 * Fact: program order will guarantee a DFS order in the call graph.
 * So we can use a global call graph (constructed on the fly) to 
 * identify the decision pairs.
 */
int freeOnOff(int nOut, int nAnc, int nGate, int flag) {
	if (current_node == NULL){
		fprintf(stderr, "Current node invalid when freeOnOff is called\n");
		exit(1);
	}

	if (current_node->on_off == -1){
		if (freePolicy == _EAGER) {
			current_node->on_off = 1;
		} else if (freePolicy == _NOFREE) {
			current_node->on_off = 0;
		} else if (freePolicy == _OPT) {
			int weight_q = 1;
			int weight_g = 1;
			int total_q = AllQubits->N;
			if (nOut > nAnc) {
				current_node->on_off = 0;
			} else if (weight_q * (nAnc-nOut+total_q) < weight_g * nGate) {
				current_node->on_off = 0;
			}
			current_node->on_off = 1;
		}
		else if (freePolicy == _EXT) {
			current_node->on_off = exhaustiveOnOff( CURRENT_IDX++ );
		}
	}
	return current_node->on_off;
}




void schedule(gate_t *new_gate) {
	int numOp = new_gate->num_operands; 
	vector<qbit_t*> operands = new_gate->operands; 
	int gateID = new_gate->gate_id;

	string gate_name = new_gate->gate_name;
	//std::cerr << "Scheduling " << gate_name << "\n";


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
	num_gate_scheduled++;
	if (num_gate_scheduled % 10000 == 0) {
		std::cerr << num_gate_scheduled << " gates scheduled.\n";
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
	vector<pair<int,int> > swaps;
	if (swapflag) {
		swaps = resolveInteraction(operands, numOp);
	}
	//std::cerr << "building gate\n";
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
  callGraph = callGraphNew();
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
	callGraphDelete(callGraph);
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
