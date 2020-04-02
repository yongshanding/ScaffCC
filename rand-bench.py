#!usr/bin/python

# Script for generating synthetic benchmarks


import os, sys, getopt, math, random

SEED = 15859211
# call graph degree: number of distinct callees in a function
DEFAULT_MAX_DEGREE = 3
TL = 0 # tab level

builtR = []

def tab():
    global TL
    TL += 1

def untab():
    global TL
    TL -= 1

def writeline(outf, line_str):
    outf.write("\t"*TL + line_str + "\n")

def sample_gates(ng, nq, na):
    # returns the operands
    res = []
    for ig in range(ng):
        # Sample a random gate (CNOT or Toffoli)
        num_op = random.randint(2,3)
        ops = random.sample(range(nq+na), num_op)
        res.append(ops)
    return res
   
    buff += "\t"*TL +  + "\n"
    buff_r += "\t"*TL +  + "\n"

def build_fun(i, all_calls, outf):
    # all_calls[i]: (i, callees, [nq,na,ng])
    callees = all_calls[i][1]
    nq = all_calls[i][2][0]
    na = all_calls[i][2][1]
    ng = all_calls[i][2][2]
    parent_degree = all_calls[i][2][3]
    buff = ""
    buff_r = ""
    # Write a function with branching degree b
    ##writeline(outf, "// Function " + str(i) + " with degree " + str(len(callees)))
    buff += "\t"*TL + "// Function " + str(i) + " with degree " + str(len(callees)) + "\n"
    buff_r += "\t"*TL + "// Function " + str(i) + " with degree " + str(len(callees)) + "\n"
    ##writeline(outf, "// nq: " + str(nq) + ", na: " + str(na) + ", ng: " + str(ng))
    buff += "\t"*TL + "// nq: " + str(nq) + ", na: " + str(na) + ", ng: " + str(ng) + "\n"
    buff_r += "\t"*TL + "// nq: " + str(nq) + ", na: " + str(na) + ", ng: " + str(ng) + "\n"
    #if (rev == 0):
    #    #writeline(outf, "void func" + str(i) + "(qbit **q, int n) {")
    #else:
    #    #writeline(outf, "void func" + str(i) + "R(qbit **q, int n) {")
    buff += "\t"*TL + "void func" + str(i) + "(qbit **q, int n) {" + "\n"
    buff += "\t"*TL + "printf(\"func" + str(i) + "\\n\");" + "\n"
    buff += "\t"*TL + "fflush(stderr);" + "\n"
    buff_r += "\t"*TL + "void func" + str(i) + "R(qbit **q, int n) {" + "\n"
    buff_r += "\t"*TL + "printf(\"func" + str(i) + "R\\n\");" + "\n"
    buff_r += "\t"*TL + "fflush(stderr);" + "\n"

    tab()
    all_ops = sample_gates(ng, nq, na)
    interqs = [x for ops in all_ops for x in ops if x < nq] # TODO? remove duplicates
    #writeline(outf, "qbit *anc[" + str(na) + "]; // ancilla")
    buff += "\t"*TL + "qbit *anc[" + str(na) + "]; // ancilla" + "\n"
    buff_r += "\t"*TL + "qbit *anc[" + str(na) + "]; // ancilla" + "\n"
    #writeline(outf, "qbit *nb[" + str(len(interqs)) + "]; // interacting bits")
    buff += "\t"*TL + "qbit *nb[" + str(len(interqs)) + "]; // interacting bits" + "\n"
    buff_r += "\t"*TL + "qbit *nb[" + str(len(interqs)) + "]; // interacting bits" + "\n"
    num_out = random.randint(1, nq)
    outs = random.sample(range(nq), num_out)
    #writeline(outf, "qbit *res["+str(num_out)+"];") # rename the outputs
    buff += "\t"*TL + "qbit *res["+str(num_out)+"];" + "\n"
    buff_r += "\t"*TL + "qbit *res["+str(num_out)+"];" + "\n"
    callees_nq = []
    for j in range(len(callees)):
        # num_q = random.randint(3, nq+na)
        # need to sample the same number of qubits that callee needs
        num_q = all_calls[callees[j]][2][0]
        callees_nq.append(num_q)
        #writeline(outf, "qbit *nq"+str(j)+"["+str(num_q)+"];") # rename callee inputs
        buff += "\t"*TL + "qbit *nq"+str(j)+"["+str(num_q)+"];" + "\n"
        buff_r += "\t"*TL + "qbit *nq"+str(j)+"["+str(num_q)+"];" + "\n"
    # Building the interaction set (distinct qubits or weighted by appearances?)
    for (j, x) in enumerate(interqs):
        #writeline(outf, "nb["+str(j)+"] = q["+str(x)+"];")
        buff += "\t"*TL + "nb["+str(j)+"] = q["+str(x)+"];" + "\n"
        buff_r += "\t"*TL + "nb["+str(j)+"] = q["+str(x)+"];" + "\n"
    # Identify the output bits
    for (j, x) in enumerate(outs):
        #writeline(outf, "res["+str(j)+"] = q["+str(x)+"];")
        buff += "\t"*TL + "res["+str(j)+"] = q["+str(x)+"];" + "\n"
        buff_r += "\t"*TL + "res["+str(j)+"] = q["+str(x)+"];" + "\n"
    # Start computations
    all_ins = []
    all_ins_r = []
    for ops in all_ops:
        if (len(ops) == 2):
            op0 = "q["+str(ops[0])+"]" if (ops[0]<nq) else "anc["+str(ops[0]-nq)+"]"
            op1 = "q["+str(ops[1])+"]" if (ops[1]<nq) else "anc["+str(ops[1]-nq)+"]"
            all_ins.append("CNOT( " + op0 + ", " + op1 + " );")
            all_ins_r.append("CNOT( " + op0 + ", " + op1 + " );")
        else:
            op0 = "q["+str(ops[0])+"]" if (ops[0]<nq) else "anc["+str(ops[0]-nq)+"]"
            op1 = "q["+str(ops[1])+"]" if (ops[1]<nq) else "anc["+str(ops[1]-nq)+"]"
            op2 = "q["+str(ops[2])+"]" if (ops[2]<nq) else "anc["+str(ops[2]-nq)+"]"
            all_ins.append("Toffoli( " + op0 + ", " + op1 + ", " + op2 + " );")
            all_ins_r.append("Toffoli( " + op0 + ", " + op1 + ", " + op2 + " );")

    if (len(callees) == 0):
        #writeline(outf, "// Leaf function")
        buff += "\t"*TL + "// Leaf function" + "\n"
        buff_r += "\t"*TL + "// Leaf function" + "\n"
        #writeline(outf, "Compute {")
        buff += "\t"*TL + "Compute (0, " + str(na) + ", " + str(ng+num_out+ng) + ", "+ str(ng+num_out) + ", 0, " + str(parent_degree) + ", 0) {" + "\n"
        buff_r += "\t"*TL + "_computeModule(0, " + str(na) + ", " + str(ng+num_out+ng) + ", "+ str(ng+num_out) + ", 0, " + str(parent_degree) + ", 0);" + "\n"
        buff_r += "\t"*TL + "acquire(" + str(na) + ", anc, " + str(len(interqs)) + ", nb);" + "\n"
        buff_r += "\t"*TL + "Recompute (res, 0, anc, " + str(na) + ", " + str(ng+num_out+ng) + ", "+ str(ng+num_out) + "){" + "\n"
        tab()
        # Now ready to allocate the ancilla
        #writeline(outf, "acquire(" + str(na) + ", anc, " + str(len(interqs)) + ", nb);")
        buff += "\t"*TL + "acquire(" + str(na) + ", anc, " + str(len(interqs)) + ", nb);" + "\n"
        for ins in all_ins:
            #writeline(outf, ins)
            buff += "\t"*TL + ins + "\n"
        for ins in reversed(all_ins):
            buff_r += "\t"*TL + ins + "\n"
        untab()
        #writeline(outf, "}")
        buff += "\t"*TL + "}" + "\n"
        buff_r += "\t"*TL + "}" + "\n"
        #writeline(outf, "Store {")
        buff += "\t"*TL + "Store {" + "\n"
        buff_r += "\t"*TL + "Restore {" + "\n"
        tab()
        temp_bits = random.sample(range(nq+na), num_out) 
        for j in range(num_out):
            if (temp_bits[j]<nq):
                # need to be different from outs
                if (temp_bits[j] == outs[j]):
                    temp_op = "anc["+str(random.randint(0,na-1))+"]"
                else:
                    temp_op = "q["+str(temp_bits[j])+"]" 
            else:
                temp_op = "anc["+str(temp_bits[j]-nq)+"]"
            #writeline(outf, "CNOT( " + temp_op + ", res[" + str(j) + "] );")
            buff += "\t"*TL + "CNOT( " + temp_op + ", res[" + str(j) + "] );" + "\n"
            buff_r += "\t"*TL + "CNOT( " + temp_op + ", res[" + str(j) + "] );" + "\n"
        untab()
        #writeline(outf, "}")
        buff += "\t"*TL + "}" + "\n"
        buff_r += "\t"*TL + "}" + "\n"
        #writeline(outf, "Uncompute(res, 0, anc, " + str(na) + ", " + str(ng+num_out+ng) + ", "+ str(ng+num_out) + "){")
        buff += "\t"*TL + "Uncompute(res, 0, anc, " + str(na) + ", " + str(ng+num_out+ng) + ", "+ str(ng+num_out) + "){" + "\n"
        buff_r += "\t"*TL + "Unrecompute {" + "\n"
        tab()
        for ins in reversed(all_ins):
            #writeline(outf, ins)
            buff += "\t"*TL + ins + "\n"
        for ins in all_ins:
            buff_r += "\t"*TL + ins + "\n"
        untab()
        #writeline(outf, "} Free(anc, " + str(na) +") {}")
        buff += "\t"*TL + "} Free(anc, " + str(na) +") {}" + "\n"
        buff_r += "\t"*TL + "} Refree(anc, " + str(na) +") {}" + "\n"

    else:
        #writeline(outf, "// Non-leaf function")
        buff += "\t"*TL + "// Non-leaf function" + "\n"
        buff_r += "\t"*TL + "// Non-leaf function" + "\n"
        # Interleaving function calls among gates
        temp_nq = ""
        temp_nq_r = ""
        for (j, c) in enumerate(callees):
            num_q = callees_nq[j]
            callee_q = random.sample(range(nq+na), num_q)
            for (k,cq) in enumerate(callee_q):
                cq_op = "q["+str(cq)+"]" if (cq<nq) else "anc["+str(cq-nq)+"]"
                #writeline(outf, "nq"+str(j)+"["+str(k)+"] = " + cq_op + ";")
                temp_nq += "\t"*TL + "nq"+str(j)+"["+str(k)+"] = " + cq_op + ";" + "\n"
                temp_nq_r += "\t"*TL + "nq"+str(j)+"["+str(k)+"] = " + cq_op + ";" + "\n"
            all_ins.append("func" + str(c) + "(nq"+str(j)+", "+str(num_q)+");")
            all_ins_r.append("func" + str(c) + "R(nq"+str(j)+", "+str(num_q)+");")
        indices = range(len(all_ins))
        random.shuffle(indices)
        shuffle_ins = [all_ins[i] for i in indices]
        shuffle_ins_r = [all_ins_r[i] for i in indices]
        #writeline(outf, "Compute {")
        buff += "\t"*TL + "Compute (0, " + str(na) + ", " + str(ng+num_out+ng) + ", "+ str(ng+num_out) + ", " + str(len(callees))+ ", " + str(parent_degree) + ", 0){" + "\n"
        buff_r += "\t"*TL + "_computeModule(0, " + str(na) + ", " + str(ng+num_out+ng) + ", "+ str(ng+num_out) + ", " + str(len(callees))+ ", " + str(parent_degree) + ", 0);" + "\n"
        buff_r += "\t"*TL + "acquire(" + str(na) + ", anc, " + str(len(interqs)) + ", nb);" + "\n"
        buff_r += temp_nq_r
        buff_r += "\t"*TL + "Recompute"+ "(res, 0, anc, " + str(na) + ", " + str(ng+num_out+ng) + ", "+ str(ng+num_out) + "){" + "\n"
        tab()
        # Now ready to allocate the ancilla
        #writeline(outf, "acquire(" + str(na) + ", anc, " + str(len(interqs)) + ", nb);")
        buff += "\t"*TL + "acquire(" + str(na) + ", anc, " + str(len(interqs)) + ", nb);" + "\n"
        buff += temp_nq
        # In-place reverse if building reverse version of function
        shuffle_ins_r.reverse()
        #for ins in reversed(shuffle_ins_r):
        #    #writeline(outf, ins)
        #    buff_r += "\t"*TL + ins + "\n"
        for ins in shuffle_ins:
            #writeline(outf, ins)
            buff += "\t"*TL + ins + "\n"
            buff_r += "\t"*TL + ins + "\n"

        untab()
        #writeline(outf, "}")
        buff += "\t"*TL + "}" + "\n"
        buff_r += "\t"*TL + "}" + "\n"
        #writeline(outf, "Store {")
        buff += "\t"*TL + "Store {" + "\n"
        buff_r += "\t"*TL + "Restore {" + "\n"
        tab()
        temp_bits = random.sample(range(nq+na), num_out) 
        for j in range(num_out):
            if (temp_bits[j]<nq):
                # need to be different from outs
                if (temp_bits[j] == outs[j]):
                    temp_op = "anc["+str(random.randint(0,na-1))+"]"
                else:
                    temp_op = "q["+str(temp_bits[j])+"]" 
            else:
                temp_op = "anc["+str(temp_bits[j]-nq)+"]"
            #writeline(outf, "CNOT( " + temp_op + ", res[" + str(j) + "] );")
            buff += "\t"*TL + "CNOT( " + temp_op + ", res[" + str(j) + "] );" + "\n"
            buff_r += "\t"*TL + "CNOT( " + temp_op + ", res[" + str(j) + "] );" + "\n"
        untab()
        #writeline(outf, "}")
        buff += "\t"*TL + "}" + "\n"
        buff_r += "\t"*TL + "}" + "\n"
        #writeline(outf, "Uncompute(res, 0, anc, " + str(na) + ", " + str(ng+num_out+ng) + ", "+ str(ng+num_out) + "){")
        buff += "\t"*TL + "Uncompute(res, 0, anc, " + str(na) + ", " + str(ng+num_out+ng) + ", "+ str(ng+num_out) + "){" + "\n"
        buff_r += "\t"*TL + "Unrecompute {" + "\n"
        tab()
        #for ins in reversed(shuffle_ins):
        #    #writeline(outf, ins)
        #    buff_r += "\t"*TL + ins + "\n"
        for ins in shuffle_ins_r:
            #writeline(outf, ins)
            buff += "\t"*TL + ins + "\n"
            buff_r += "\t"*TL + ins + "\n"

        untab()
        #writeline(outf, "} Free(anc, " + str(na) +") {}")
        buff += "\t"*TL + "} Free(anc, " + str(na) +") {}" + "\n"
        buff_r += "\t"*TL + "} Refree(anc, " + str(na) +") {}" + "\n"
    untab()
    #writeline(outf, "}")
    buff += "\t"*TL + "}" + "\n"
    buff_r += "\t"*TL + "}" + "\n"

    outf.write(buff)
    outf.write(buff_r)

def build_main(callees, all_calls, outf, nq, na, ng):
    if (len(callees) == 0):
        print ("Error. main function should have at least one callees.")
        sys.exit()

    writeline(outf, "// main function")
    writeline(outf, "int main() {")
    tab()
    # allocating qubits
    writeline(outf, "qbit *new[" + str (nq) + "];")
    writeline(outf, "acquire(" + str(nq) + ", new, 0, NULL);")
    writeline(outf, "// Intialize inputs")
    all_bits = range(nq)
    num_ones = random.randint(0, nq)
    bits = random.sample(all_bits, num_ones)
    for b in bits:
        writeline(outf, "X (new[" + str(b) + "]);")
    writeline(outf, "// Start computation")
    if (len(callees) == 1):
        for c in callees:
            writeline(outf, "func" + str(c) + "(new, " + str(nq) + ");")

    else:
        callees_nq = []
        all_ins = []
        for j in range(len(callees)):
            # num_q = random.randint(3, nq+na)
            # need to sample the same number of qubits that callee needs
            num_q = all_calls[callees[j]][2][0]
            callees_nq.append(num_q)
            writeline(outf, "qbit *nq"+str(j)+"["+str(num_q)+"];") # rename callee inputs

        for (j, c) in enumerate(callees):
            num_q = callees_nq[j]
            callee_q = random.sample(range(nq), num_q)
            for (k,cq) in enumerate(callee_q):
                cq_op = "new["+str(cq)+"]" 
                writeline(outf, "nq"+str(j)+"["+str(k)+"] = " + cq_op + ";")
            all_ins.append("func" + str(c) + "(nq"+str(j)+", "+str(num_q)+");")
        for ins in all_ins:
            writeline(outf, ins)
    writeline(outf, "return 0;")
    untab()
    writeline(outf, "}")

def printStructure(call_lists):
    print("[rand-bench.py] Program structure:")
    for (i,calls) in enumerate(call_lists):
        call_str = ",".join([str(c) for c in calls])
        print("\tFun " + str(i) + ": " + call_str)

def rand_synth(outf, nq, na, ng, nl, nd):
    writeline(outf, "// Scaffold file synthesized by rand-bench.py")
    writeline(outf, "// qubits: " + str(nq) + " ancilla: " + str(na) + " gates: " + str(ng) + " levels: " + str(nl) + " degrees: "+ str(nd))
    writeline(outf, "#include \"qalloc.h\"")
    writeline(outf, "#include \"uncompute.h\"")
    # Build a tree-like program with depth nl, and random branching
    # First create the random branching array A[l] is the branching 

    random.seed(SEED)
    print("[rand-bench.py] Random seed used: " + str(SEED))

    if (nl == 1):
        call_lists = [[i for i in range(1,nd+1)]]
    else:
        branch = [random.randint(0,nd) for li in range(nd**nl)]
        if branch[0]==0:
            branch[0]+=1
        cuts = [1]
        start = 0
        end = 1
        for b in branch:
            if end >= len(branch):
                break
            temp = 0
            for i in range(start,end):
                temp += branch[i]
            cuts.append(cuts[-1]+temp)
            start = end
            end = end + temp
        acc = 1
        ll = 0
        call_lists = []
        for (i, b) in enumerate(branch):
            call_lists.append([j for j in range(acc, acc+b)])
            acc += b
            if i in cuts:
                ll += 1 
            if ll >= nl:
                break
        
    print(call_lists)
        
    printStructure(call_lists)
    writeline(outf, "// Call list: " + (";".join([",".join([str(c) for c in calls]) for calls in call_lists])))
    #print(cuts)
    num_funs = 0
    for calls in call_lists:
        num_funs += len(calls)
    #print num_funs
    all_calls = [(0,[],[]) for _ in range(num_funs+1)]
    #all_callees = []
    #all_subs = []
    for (i, calls) in enumerate(call_lists):
        for c in calls:
            callees = call_lists[c] if (c < len(call_lists)) else []
            #degrees = len(callees)
            if (i == 0):
                if (len(calls) == 1):
                    subq = nq
                    suba = na
                    subg = ng
                else:
                    subq = random.randint(2, nq)
                    suba = random.randint(1, na)
                    subg = random.randint(1, ng)
                    
            else:
                subq = random.randint(2,min(nq, all_calls[i][2][0]+all_calls[i][2][1])) # fewer than nq+na of parent
                #suba = random.randint(3,min(na, all_calls[i][2][1]))
                suba = random.randint(1, na) # ancs dont have to be fewer 
                #subg = random.randint(1,min(ng, all_calls[i][2][2]))
                subg = random.randint(1, ng) # gates dont have to be fewer 
            #print c
            all_calls[c] = (c, callees, [subq, suba, subg, len(calls)])
            #all_subs.append([subq, suba, subg])
            
    builtR = [0 for _ in range(len(all_calls))]
    for (c, _, _) in reversed(all_calls[1:]):
        build_fun(c, all_calls, outf)

    # Lastly, build main
    build_main(call_lists[0], all_calls, outf, nq, na, ng)
    print("[rand-bench.py] Sythetic benchmark written to: " + outf.name)
    #for li in range(nl):
    #    # start from the leaf level (i=0), build the necessary functions
    #    b = branch[li]
    #    build_level(li, b, outf, nq, na, ng)
    # Constructing the tree structure


def main():
    s = -1
    outname= ""
    num_qubtis = 0
    num_ancilla= 0
    num_gates = 0
    L = 0
    d = DEFAULT_MAX_DEGREE
    try:
        opt, args = getopt.getopt(sys.argv[1:], "ho:q:a:g:L:d:s:", ["help", "output=", "qubits=", "ancilla=", "gates=", "levels=", "degree=", "seed="])
    except getopt.GetOptError as err:
        print(err)
        print("Usage: rand-bench.py -o <output file> -q <qubits> -a <ancilla> -g <gates> -L <levels> -d <degree> -s <seed, optional>")
        sys.exit(2)
    for o,a in opt:
        if o in ("-h", "--help"):
            print("Usage: rand-bench.py -o <output file> -q <qubits> -a <ancilla> -g <gates> -L <levels> -d <degree> -s <seed, optional>")
            sys.exit()
        elif o in ("-o", "--output"):
            outname = a
        elif o in ("-q", "--qubits"):
            num_qubits = int(a)
        elif o in ("-a", "--ancilla"):
            num_ancilla = int(a)
        elif o in ("-g", "--gates"):
            num_gates = int(a)
        elif o in ("-L", "--levels"):
            L = int(a)
        elif o in ("-d", "--degree"):
            d = int(a)
        elif o in ("-s", "--seed"):
            s = int(a)
        else:
            print("Usage: rand-bench.py -o <output file> -q <qubits> -a <ancilla> -g <gates> -L <levels> -d <degree> -s <seed, optional>")
            sys.exit()
    if (num_qubits > 0 and num_ancilla > 0 and num_gates > 0 and L > 0 and d > 0):
        if (not outname):
            print("Please specify valid output scaffold filename")
        else:
            if (not s == -1):
                global SEED
                SEED = s
            with open(outname, 'w') as outfile:
                rand_synth(outfile, num_qubits, num_ancilla,  num_gates, L, d)
    else:
        print("qubits, gates, or levels needs to be a positive integer")
        print("Usage: rand-bench.py -o <output file> -q <qubits> -a <ancilla> -g <gates> -L <levels> -d <degree> -s <seed, optional>")
 
if __name__ == "__main__":
  main()
