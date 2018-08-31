FILE=best_multiplier
cd build/
make -j 12 #>/dev/null 2>/dev/null 
cd -  
rm -rf ${FILE}*  
rm -rf scripts/rev-memory-manager-hybrid.bcpp  
echo "[run.sh] ${FILE} begins..."
touch ${FILE}.results.out
for i in $(seq 1 256) ; do
	./scripts/gen-rev-mem-optimized.sh Algorithms/${FILE}.scaffold #>${FILE}.err 2> ${FILE}.err2
	tail -n +2 on_off_sequences.txt > seq.tmp && mv seq.tmp on_off_sequences.txt	
	tail "${FILE}/${FILE}.flat000k.freq" >> "${FILE}.results${i}.out"
done
#tail ${FILE}/*freq > ${FILE}.out 
cat ${FILE}/${FILE}.results*.out > ${FILE}.results.out
