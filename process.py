#!/usr/bin/python

lines = []
vols = []
with open("first.results.out", "r") as f:
    lines = f.readlines()
    linesplits = [x.split() for x in lines]
    #print(len(linesplits))
    for x in range(1023):
        qbs = int(linesplits[10*x+1][-1][:-1])
        ts = int(linesplits[10*x+3][-1][:-1])
        vol = (qbs, ts)
        vols.append(vol)
min_q = 0
min_g = 0
min_vol = 10000000
min_idx = -1

for x in range(len(vols)):
    vol = vols[x][0]*vols[x][1]
    if (vol < min_vol):
        min_vol = vol
        min_idx = x
        min_q = vols[x][0]
        min_g = vols[x][1]

print (str(min_idx)+": "+str(format(min_idx,'010b')))
print ("Min vol: " + str(min_q) + " x " + str(min_g) + " = " + str(min_vol))

