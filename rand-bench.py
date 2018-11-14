#!usr/bin/python

# Script for generating synthetic benchmarks


import os, sys, getopt, math, random

SEED = 33001
# call graph degree: number of distinct callees in a function
MAX_DEGREE = 3
TL = 0 # tab level

def tab():
    global TL
    TL += 1

def untab():
    global TL
    TL -= 1

def writeline(outf, line_str):
    outf.write("\t"*TL + line_str + "\n")

def build_fun(i, callees, outf, nq, na, ng):
    # Write a function with branching degree b
    writeline(outf, "// Function " + str(i) + " with degree " + str(len(callees)))
    writeline(outf, "void func" + str(i) + "() {")
    tab()
    if (len(callees) == 0):
        writeline(outf, "// Leaf function")
    else:
        for c in callees:
            writeline(outf, "func" + str(c) + "();")
    writeline(outf, "return;")
    untab()
    writeline(outf, "}")

def build_main(callees, outf, nq, na, ng):
    if (len(callees) == 0):
        print ("Error. main function should have at least one callees.")
        sys.exit()
    writeline(outf, "// main function")
    writeline(outf, "int main() {")
    tab()
    for c in callees:
        writeline(outf, "func" + str(c) + "();")
    writeline(outf, "return 0;")
    untab()
    writeline(outf, "}")
    

def rand_synth(outf, nq, na, ng, nl):
    writeline(outf, "// Scaffold file synthesized by rand-bench.py")
    writeline(outf, "// qubits: " + str(nq) + " ancilla: " + str(na) + " gates: " + str(ng) + " levels: " + str(nl) + " ")
    writeline(outf, "#include \"qalloc.h\"")
    writeline(outf, "#include \"uncompute.h\"")
    # Build a tree-like program with depth nl, and random branching
    # First create the random branching array A[l] is the branching 
    random.seed(SEED)
    branch = [random.randint(0,MAX_DEGREE) for li in xrange(MAX_DEGREE**nl)]
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
        call_lists.append(range(acc, acc+b))
        acc += b
        if i in cuts:
            ll += 1 
        if ll >= nl:
            break
        
        
    print(call_lists)
    print(cuts)
    for calls in reversed(call_lists):
        for c in calls:
            callees = call_lists[c] if (c < len(call_lists)) else []
            degrees = len(callees)
            build_fun(c, callees, outf, nq, na, ng)

    # Lastly, build main
    build_main(call_lists[0], outf, nq, na, ng)
    #for li in xrange(nl):
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
    try:
        opt, args = getopt.getopt(sys.argv[1:], "ho:q:a:g:L:", ["help", "output=", "qubits=", "ancilla=", "gates=", "levels="])
    except getopt.GetOptError as err:
        print(err)
        print("Usage: rand-bench.py -o <output file> -q <qubits> -a <ancilla> -g <gates> -L <levels>")
        sys.exit(2)
    for o,a in opt:
        if o in ("-h", "--help"):
            print("Usage: rand-bench.py -o <output file> -q <qubits> -a <ancilla> -g <gates> -L <levels>")
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
        else:
            print("Usage: rand-bench.py -o <output file> -q <qubits> -a <ancilla> -g <gates> -L <levels>")
            sys.exit()
    if (num_qubits > 0 and num_ancilla > 0 and num_gates > 0 and L > 0):
        if (not outname):
            print("Please specify valid output scaffold filename")
        else:
            with open(outname, 'w') as outfile:
                rand_synth(outfile, num_qubits, num_ancilla,  num_gates, L)
    else:
        print("qubits, gates, or levels needs to be a positive integer")
        print("Usage: rand-bench.py -o <output file> -q <qubits> -a <ancilla> -g <gates> -L <levels>")
 
if __name__ == "__main__":
  main()
