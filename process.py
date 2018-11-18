#!/usr/bin/python

lines = []
vols = []
with open("first/first.results.out", "r") as f:
    lines = f.readlines()
    linesplits = [x.split() for x in lines]
    #print(len(linesplits))
    num = len(linesplits)/11-1
    for x in range(num):
        idx = int(linesplits[11*x][-1])
        qbs = int(linesplits[11*x+2][-1][:-1])
        ts = int(linesplits[11*x+4][-1][:-1])
        vol = (idx, qbs, ts)
        vols.append(vol)
min_q = 0
min_g = 0
min_vol = 10000000
min_idx = -1

for x in range(len(vols)):
    vol = vols[x][1]*vols[x][2]
    if (vol < min_vol):
        min_vol = vol
        min_idx = vols[x][0]
        min_q = vols[x][1]
        min_g = vols[x][2]
print ("Instances processed: " + str(len(vols)))
print ("Min idx: " + str(min_idx)+": "+str(format(min_idx,'010b')))
print ("Min vol: " + str(min_q) + " x " + str(min_g) + " = " + str(min_vol))

