#!usr/bin/python

# Script for generating arbitrary topology


import os, sys, getopt, math, json, re

gate_pattern = re.compile("^\d+: ") #12: CNOT q1 q2
qubit_used = re.compile("^Total number of qubits used: \d+.")
method = 'latest' # or 'earliest'

class Site:
    def __init__(self, qid, side):
        self.qid = qid
        self.coords = (qid // side, qid % side)
        self.occupied = False

class Surface:
    def __init__(self, num_qubits):
        self.num_qubits = num_qubits
        self.side = int(math.sqrt(num_qubits))
        self.grid = [Site(i, self.side) for i in range(num_qubits)]
    def qid_to_coord(self, qid):
        return (qid // self.side, qid % self.side)
    def coord_to_qid(self, x, y):
        return x * self.side + y
    def check(self, qid):
        return self.grid[qid].occupied
    def occupy(self, qid):
        if self.grid[qid].occupied:
            print("q%d already occupied. Check again." % qid)
        self.grid[qid].occupied = True

class Sched:
    def __init__(self):
        self.schedule = []
        self.num_steps = 0
        
    def from_freq_file(self, freq):
        lines = freq.readlines()
        num_steps = 0
        for x in lines:
            if gate_pattern.match(x):
                linesplits = re.split(': ', x)
                t = int(linesplits[0])
                gate = linesplits[1]
                if (t >= num_steps):
                    for _ in range(t - num_steps + 1):
                        self.schedule.append([])
                    num_steps = t
                self.schedule[t].append(gate)
            elif qubit_used.match(x):
                linesplits = re.split(': |.', x)
        self.num_steps = num_steps
     
class Epoch:
    def __init__(self, num_qubits, distance):
        self.num_qubits = num_qubits
        self.d = distance
        self.epoch = [] # list of surf
        self.cycles = 0  
        self.latest_use = [0 for _ in range(num_qubits)] # comprise for simulation efficiency
    def add_cycles(self, n):
        for _ in range(n):
            self.epoch.append(Surface(self.num_qubits))       
            self.cycles += n
        
def get_event_timer(d, gname):
    if gname == 'H':
        return d+1
    elif gname == 'T':
        return d
    elif gname == 'Tdag':
        return d
    elif gname == 'CNOT':
        return 2 * d + 1 
    elif gname == 'MeasZ':
        return d
    elif gname == 'X':
        return 0
    else:
        print("Gate event (%s) not recognized." % gname)
    
def get_sched(infile):
    #Assume num_qubits is a perfect square, and we build a 2d grid out of it
    g = Sched()
    g.from_freq_file(infile)
    return g

def get_routes(surf, q1, q2):
    # Return 2 possible xy routings: [[p10, p11, p12], [p20, p21, p22]]
    result = []
    (q1x, q1y) = surf.qid_to_coord(q1)
    (q2x, q2y) = surf.qid_to_coord(q2)
    # X-Y
    xypath = [(q1x, q1y)]
    if (q1x <= q2x):
        for x in range(q1x+1, q2x+1):
            xypath.append((x, q1y))
        if (q1y <= q2y):
            for y in range(q1y+1, q2y+1):
                xypath.append((q2x, y))
        else:
            for y in range(q1y-1, q2y-1, -1):
                xypath.append((q2x, y))
    else: 
        for x in range(q1x-1, q2x-1, -1):
            xypath.append((x, q1y))
        if (q1y <= q2y):
            for y in range(q1y+1, q2y+1):
                xypath.append((q2x, y))
        else:
            for y in range(q1y-1, q2y-1, -1):
                xypath.append((q2x, y))
    result.append(xypath)

    # X-Y
    yxpath = [(q1x, q1y)]
    if (q1y <= q2y):
        for y in range(q1y+1, q2y+1):
            yxpath.append((q1x, y))
        if (q1x <= q2x):
            for x in range(q1x+1, q2x+1):
                yxpath.append((x, q2y))
        else:
            for x in range(q1x-1, q2x-1, -1):
                yxpath.append((x, q2y))
    else: 
        for y in range(q1y-1, q2y-1, -1):
            yxpath.append((q1x, y))
        if (q1x <= q2x):
            for x in range(q1x+1, q2x+1):
                yxpath.append((x, q2y))
        else:
            for x in range(q1x-1, q2x-1, -1):
                yxpath.append((x, q2y))
    result.append(yxpath)
    return result

def schedule_gate(epoch, gname, ops, method):
    def _occupy_for_time(start, hold, qid, method):
        if (method == 'earliest'):
            if (start + hold >= epoch.cycles):
                epoch.add_cycles(start + hold - epoch.cycles)
            for i in range(start, start + hold):
                epoch.epoch[i].occupy(qid)
        elif (method == 'latest'):
            epoch.latest_use[qid] = start + hold
            
    def _check_path(surf, path):
        path_clear = True
        for coord in path:
            path_clear = path_clear and surf.check(surf.coord_to_qid(coord[0], coord[1]))
        return path_clear
    # find the earliest time we can apply gname
    start = -1

    if (epoch.cycles == 0):
        epoch.add_cycles(1)
    if (method == 'earlies'):
        if (len(ops) == 1):
            for (i, surf) in enumerate(epoch.epoch):
                # check if gname can be applied
                if (surf.check(ops[0]) == False):
                    # Found a start time
                    start = i
                    break
            if (start >= 0):
                _occupy_for_time(start, get_event_timer(epoch.d, gname), ops[0])
            return
        elif (len(ops) == 2):
            surf = epoch.epoch[0]
            routes = get_routes(surf, ops[0], ops[1])
            p = -1
            
            for (i, surf) in enumerate(epoch.epoch):
                for (j, path) in enumerate(routes):
                    if _check_path(surf, path):
                        # found start time
                        start = i
                        p = j
                        break
                if (start >= 0):
                    break
            if (start >= 0):
                for coord in path:
                    _occupy_for_time(start, get_event_timer(epoch.d, gname), surf.coord_to_qid(coord[0], coord[1]))
            return
        else:
            print("Multi-qubit gate with more than 2 qubits not supported: %s " % gname, ops)
            return
    elif (method == 'latest'):
        # Note: this is within a timestep in a circuit, so no data dependencies
        if (len(ops) == 1):
            # route can go around a qubit so no need to update latest_use
            start = epoch.latest_use[ops[0]]
            hold = get_event_timer(epoch.d, gname)
            end = start + hold
            # epoch.latest_use[ops[0]] = end
            if (epoch.cycles < end):
                epoch.cycles = end
        elif (len(ops) == 2):
            surf = epoch.epoch[0]
            routes = get_routes(surf, ops[0], ops[1])
            p = [0 for _ in range(len(routes))]
            for (j, path) in enumerate(routes):
                for (x,y) in path:
                    qid = surf.coord_to_qid(x, y)
                    if (epoch.latest_use[qid] > p[j]):
                        p[j] = epoch.latest_use[qid]
            pmin = p[0]
            jmin = 0
            for j in range(len(routes)):
                if (p[j] < pmin):
                    pmin = p[j] 
                    jmin = j
            start = pmin + 1
            hold = get_event_timer(epoch.d, gname)
            end = start + hold
            for (x,y) in routes[jmin]:
                epoch.latest_use[surf.coord_to_qid(x, y)] = end
                if (epoch.cycles < end):
                    epoch.cycles = end
        
                

def simulate(num_qubits, surf, sched, outfile, d, method='latest'):
    total_cycles = 0
    total_volume = 0
    active_qubits = [-1 for _ in range(num_qubits)] # active since which cycle, or -1: inactive
    volume_on_qubits = [0 for _ in range(num_qubits)]
    completed = 0
    total_steps = len(sched.schedule)
    tenpercent = total_steps // 10
    print("Begin simulation of %d steps." % total_steps)
    outfile.write("Begin simulation of %d steps.\n" % total_steps)
    for step in sched.schedule:
        if (completed % tenpercent == 0):
            print("Completed %d0%%." % (completed / tenpercent))
            outfile.write("Completed %d0%%.\n" % (completed / tenpercent))
        epoch = Epoch(num_qubits, d)
        for gate in step:
            gatesplits = re.split(' ', gate)[:-1] # exluding last one: \n
            #print("Simulate gate: ", gatesplits)
            gname = gatesplits[0]
            ops = []
            for i in range(1, len(gatesplits)):
                try:
                    qid = int(gatesplits[i][1:])
                except:
                    break
                if (qid >= 0 and qid < num_qubits):
                    ops.append(qid)
            if (len(ops) < len(gatesplits) - 1):
                continue
            else:
                for q in ops:
                    if (gname == 'Free'):
                        if (active_qubits[q] == -1):
                            print("q%d freed before allocated." % q)
                        # complete a qubit usage segment
                        volume_on_qubits += total_cycles - active_qubits[q]
                    elif (active_qubits[q] == -1):
                        active_qubits[q] = total_cycles
                        #print("Set alloc: ", total_cycles)
                    
                schedule_gate(epoch, gname, ops, method)
        total_cycles += epoch.cycles
        completed += 1
    print("Total cycles: ", total_cycles)
    outfile.write("Total cycles: %d \n" % total_cycles)
    for i in range(num_qubits):
        if (active_qubits[i] != -1):
            volume_on_qubits[i] += total_cycles - active_qubits[i]
        total_volume += volume_on_qubits[i]
    print("Total volume: ", total_volume)
    outfile.write("Total volume: %d \n" % total_volume)
    return



def main():
    outname= ""
    inname= ""
    num_qubtis = 0
    d = 0
    try:
        opt, args = getopt.getopt(sys.argv[1:], "ho:q:i:d:", ["help", "output=", "qubits=", "input=", "distance="])
    except getopt.GetOptError as err:
        print(err)
        print("Usage: simulate_braid.py -o <output file> -q <qubits> -i <input freq file> -d <surface code distance>")
        sys.exit(2)
    for o,a in opt:
        if o in ("-h", "--help"):
            print("Usage: simulate_braid.py -o <output file> -q <qubits> -i <input freq file> -d <surface code distance>")
            sys.exit()
        elif o in ("-o", "--output"):
            outname = a
        elif o in ("-q", "--qubits"):
            num_qubits = int(a)
        elif o in ("-i", "--input"):
            inname = a
        elif o in ("-d", "--distance"):
            d = int(a)
        else:
            print("Usage: simulate_braid.py -o <output file> -q <qubits> -i <input freq file> -d <surface code distance>")
            sys.exit()
    if (num_qubits > 0 and d > 0):
        if (not outname or not inname):
            print("Please specify valid input/output filename")
        else:
            with open(outname, 'w') as outfile:
                with open(inname, 'r') as infile:
                    surf = Surface(num_qubits)
                    sched = get_sched(infile)

                    simulate(num_qubits, surf, sched, outfile, d, method)
    else:

        print("Usage: simulate_braid.py -o <output file> -q <qubits> -i <input freq file> -d <surface code distance>")
 
if __name__ == "__main__":
  main()
