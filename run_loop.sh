FILE=first
n=12
N=4096
cd build/
make -j 12 >/dev/null 2>/dev/null 
cd -  
rm -rf ${FILE}*  
rm -rf scripts/rev-memory-manager-hybrid.bcpp  
echo "[run.sh] ${FILE} begins..."
touch ${FILE}.results.out
echo "[run.sh] Generating on_off_sequences.txt of ${n} bits..."
python gen-all-seqs.py -n ${n}
echo "[run.sh] Running ${N} iterations..."
for i in $(seq 1 ${N}) ; do
	./scripts/gen-rev-mem-optimized.sh Algorithms/${FILE}.scaffold >>${FILE}.err 2>> ${FILE}.err2
	tail -n +2 on_off_sequences.txt > seq.tmp && mv seq.tmp on_off_sequences.txt	
	#tail "${FILE}/${FILE}.flat000k.freq" >> "${FILE}.results${i}.out"
	bits=$(($i - 1))
	echo "bit seq: ${bits}" 
	tail "${FILE}/${FILE}.flat000k.freq" 
	#cat ${FILE}.results${i}.out >> ${FILE}.results.out
done >> ${FILE}.results.out
#tail ${FILE}/*freq > ${FILE}.out 
