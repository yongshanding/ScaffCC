#!usr/bin/python

# Script for generating synthesized benchmarks


import os, sys, getopt, math

def rand_synth(outf, nq, ng, nl):
    outf.write("// Scaffold file synthesized by synth-bench.py")
    outf.write("// qubits: " + str(nq) + " gates: " + str(ng) + " levels: " + str(nl) + " ")
    outf.write("#include \"qalloc.h\"")
    outf.write("#include \"uncompute.h\"")
    # TODO: random sample from synth logic library

def main():
    s = -1
    outname= ""
    num_qubtis = 0
    num_gates = 0
    try:
        opt, args = getopt.getopt(sys.argv[1:], "ho:q:g:L:", ["help", "output=", "qubits=", "gates=", "levels="])
    except getopt.GetOptError as err:
        print(err)
        print("Usage: synth-bench.py -o <output file> -q <qubits> -g <gates> -L <levels>")
        sys.exit(2)
    for o,a in opt:
        if o in ("-h", "--help"):
            print("Usage: synth-bench.py -o <output file> -q <qubits> -g <gates> -L <levels>")
            sys.exit()
        elif o in ("-o", "--output"):
            outname = a
        elif o in ("-q", "--qubits"):
            num_qubits = int(a)
        elif o in ("-g", "--gates"):
            num_gates = int(a)
        elif o in ("-L", "--levels"):
            L = int(a)
        else:
            print("Usage: synth-bench.py -o <output file> -q <qubits> -g <gates> -L <levels>")
            sys.exit()
    if (num_qubits > 0 and num_gates > 0 and L > 0):
        if (outname == "")
            print("Please specify valid output scaffold filename")
        else:
            with open(outname, 'w') as outfile:
                rand_synth(outfile, num_qubits, num_gates, L)
    else:
        print("qubits, gates, or levels needs to be a positive integer")
        print("Usage: synth-bench.py -o <output file> -q <qubits> -g <gates> -L <levels>")
 
if __name__ == "__main__":
    main()
