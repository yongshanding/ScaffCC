import sys
import os
import re


def main():
    filepath = sys.argv[1]
    total_q = int(sys.argv[2])
    q = 0
    cnt = 0
    if not os.path.isfile(filepath):
        print("File path {} does not exist. Exiting...".format(filepath))
        sys.exit()
    circ = {}
    with open(filepath) as fp:
        for line in fp:
            #print("line {} contents {}".format(cnt, line))
            q0, t = record_inst(re.split(": |\n| ", line), circ)
            if t > cnt: cnt = t
            if q0 != 0: q = q0
    #print(circ)
    translate(circ, total_q, cnt)
    print("# Num qubits used: %d out of %d"% (q, total_q))
    print("# Depth: %d "% cnt)
    #sorted_words = order_bag_of_words(bag_of_words, desc=True)
    #print("Most frequent 10 words {}".format(sorted_words[:10]))

class Gate:
    def __init__(self, gname, ops):
        self.gate = gname
        self.ops = []
        regex = r"q(\d+)"
        #print(gname, ops)
        for op in ops:
            #print(op)
            match = re.search(regex, op)
            qid = int(match.group(1))
            self.ops.append(qid)

def record_inst(words, circ):
    q = 0
    words = list(filter(lambda x: len(x) > 0, words))
    #print(words)
    t = 0
    if len(words) >= 3:
        if ("Total" in words and "number" in words and "qubits" in words and "used" in words):
            q = int(float(words[5]))
        if (words[0].isnumeric()):
            t = int(words[0])
            gname = words[1]
            ops = words[2:]
            if len(ops) > 3:
                return q, t
            g = Gate(gname, ops)
            if t in circ:
                circ[t].append(g)
            else:
                circ[t] = [g]
    return q, t

def translate(circ_in, q, d):
    filepath = sys.argv[1]
    circ_out = "# Qiskit circuit generated from {} file\n".format(filepath)
    circ_out += "circ = QuantumCircuit({}, {})\n".format(q, q)
    meas = []
    for i in range(d):
        if i in circ_in:
            insts = circ_in[i]
            for inst in insts:
                if inst.gate == "H":
                    circ_out += "circ.h({})\n".format(inst.ops[0])
                if inst.gate == "T":
                    circ_out += "circ.t({})\n".format(inst.ops[0])
                if inst.gate == "Tdag":
                    circ_out += "circ.tdg({})\n".format(inst.ops[0])
                if inst.gate == "CNOT":
                    if (inst.ops[0] == inst.ops[1]):
                        continue
                    circ_out += "circ.cx({}, {})\n".format(inst.ops[0], inst.ops[1])
                if inst.gate == "swap":
                    circ_out += "circ.swap({}, {})\n".format(inst.ops[0], inst.ops[1])

                if inst.gate == "MeasZ":
                    meas.append(inst.ops[0])
        #else:
        #    print("Error: depth %d not found in circuit" % i)
    if len(meas) > 0:
        meas.sort()
        circ_out += "circ.measure({}, {})\n".format(meas, meas)
    print(circ_out)
    return circ_out


if __name__ == '__main__':
   main()
