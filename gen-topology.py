#!usr/bin/python

# Script for generating arbitrary topology


import os, sys, getopt, math, json

def gen_topology(outf, nq, ty):
    res = dict()
    res["QubitLayout"] = [i for i in range(nq)]
    if (ty == 0):
        # linear topology
        res["QubitConnectivity"] = dict()
        res["QubitConnectivity"]["NumQbits"] = nq
        for i in range(nq):
            if i == 0:
                res["QubitConnectivity"][str(i)] = [1]
            elif i == nq-1:
                res["QubitConnectivity"][str(i)] = [nq-2]
            else:
                res["QubitConnectivity"][str(i)] = [i-1, i+1]
    if (ty == 1):
        # square grid topology
        res["QubitConnectivity"] = dict()
        res["QubitConnectivity"]["NumQbits"] = nq
        side = int(math.ceil(math.sqrt(nq)))
        print("side length: " + str(side))
        #i = 0
        for row in range(side):
            for col in range(side):
                neighbors = []
                if row > 0 and row < side-1:
                    neighbors.append((row-1) * side + col)
                    neighbors.append((row+1) * side + col)
                elif row == 0 and (not row == side-1):
                    neighbors.append((row+1) * side + col)
                elif (not row == 0) and row == side-1:
                    neighbors.append((row-1) * side + col)
                if col > 0 and col < side-1:
                    neighbors.append(row * side + col-1)
                    neighbors.append(row * side + col+1)
                elif col == 0 and (not col == side-1):
                    neighbors.append(row * side + col+1)
                elif (not col == 0) and col == side-1:
                    neighbors.append(row * side + col-1)
                res["QubitConnectivity"][str(row * side + col)] = neighbors
    json_data = json.dumps(res)
    outf.write(json_data)

def main():
    outname= ""
    num_qubtis = 0
    ty = 0
    try:
        opt, args = getopt.getopt(sys.argv[1:], "ho:q:t:", ["help", "output=", "qubits=", "type="])
    except getopt.GetOptError as err:
        print(err)
        print("Usage: gen-topology.py -o <output file> -q <qubits> -t <type, 0:linear, 1:grid, 2:other>")
        sys.exit(2)
    for o,a in opt:
        if o in ("-h", "--help"):
            print("Usage: gen-topology.py -o <output file> -q <qubits> -t <type, 0:linear, 1:grid, 2:other>")
            sys.exit()
        elif o in ("-o", "--output"):
            outname = a
        elif o in ("-q", "--qubits"):
            num_qubits = int(a)
        elif o in ("-t", "--type"):
            ty = int(a)
        else:
            print("Usage: gen-topology.py -o <output file> -q <qubits> -t <type, 0:linear, 1:grid, 2:other>")
            sys.exit()
    if (num_qubits > 0 and ty >= 0):
        if (not outname):
            print("Please specify valid output scaffold filename")
        else:
            with open(outname, 'w') as outfile:
                gen_topology(outfile, num_qubits, ty)
    else:
        print("Usage: gen-topology.py -o <output file> -q <qubits> -t <type, 0:linear, 1:grid, 2:other>")
 
if __name__ == "__main__":
  main()
