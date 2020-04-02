import sys
import os
import re

samples = 2**13

def main():
    filepath = sys.argv[1]
    
    if not os.path.isfile(filepath):
        print("File path {} does not exist. Exiting...".format(filepath))
        sys.exit()
    with open(filepath) as fp:
        record_data(fp)
    
    

def read_distribution(line):
    extracted = re.split("{|}|: |, ", line)[1:-1]
    #print(extracted)
    dist = []
    for i in range(1, len(extracted), 2):
        if not (extracted[i].isdigit()): print(extracted[i])
        dist.append(int(extracted[i]))
    return dist

def compute_tv_distance(ideal, noisy):
    paired = zip(ideal, noisy)
    dist_tv = 0
    for (a, b) in paired:
        #print(a,b)
        dist_tv += abs(a - b) / samples
    return 0.5 * dist_tv
  
def record_data(fp):
    lines = []
    for line in fp:
        lines.append(line)
    for (i, line) in enumerate(lines):
        #print("line {} contents {}".format(cnt, line))
        line = lines[i].strip().split('/')
        if "nisq_sim" in line: # filename line
            key = line[-1]
            assert(len(lines) >= i+5)
            ideal = []
            noisy = []
            if "No noise" in lines[i+1]:
                ideal = read_distribution(lines[i+2])
            if "With noise" in lines[i+3]:
                noisy = read_distribution(lines[i+4])
            assert(len(ideal) == len(noisy))
            d = compute_tv_distance(ideal, noisy)
            print(key, d)
            

if __name__ == '__main__':
    main()
