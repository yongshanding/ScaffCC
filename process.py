#!/usr/bin/python

lines = []
vols = []
with open("results.processed", "r") as f:
    lines = f.readlines()
    linesplits = [x.split() for x in lines]
    print(len(linesplits))
    for x in range(255):
        qbs = int(linesplits[3*x][-1][:-1])
        ts = int(linesplits[3*x+1][-1][:-1])
        vol = qbs * ts
        vols.append(vol)

min_vol = 10000000
min_idx = -1

for x in range(len(vols)):
    if vols[x] < min_vol:
        min_vol = vols[x]
        min_idx = x

print (format(x,'08b'))
print (min_vol)

