#!usr/bin/python

# Script for generating all on/off sequences


import os, sys, getopt, math, random

def gen_seqs(outf, n):
    # n is number of bits
    for i in range(n):
        outf.write(str(random.randint(0,0)))
    outf.write("\n")

def main():
    num = 0
    try:
        opt, args = getopt.getopt(sys.argv[1:], "hn:", ["help", "num="])
    except getopt.GetOptError as err:
        print(err)
        print("Usage: gen-all-seqs.py -n <num_bits>")
        sys.exit(2)
    for o,a in opt:
        if o in ("-h", "--help"):
            print("Usage: gen-all-seqs.py -n <num_bits>")
            sys.exit()
        elif o in ("-n", "--num"):
            num = int(a)
        else:
            print("Usage: gen-all-seqs.py -n <num_bits>")
            sys.exit()
    if (num > 0):
        with open("on_off_sequences.txt", 'w') as outfile:
            gen_seqs(outfile, num)
    else:
        print("number of sequences need to be a positive integer")
        print("Usage: gen-all-seqs.py -n <num_bits>")
 
if __name__ == "__main__":
  main()
