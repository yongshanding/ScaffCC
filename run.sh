FILE=sha2
cd build/
make -j 12 #>/dev/null 2>/dev/null 
cd -  
rm -rf ${FILE}*  
rm -rf scripts/rev-memory-manager-hybrid.bcpp  
echo "[run.sh] ${FILE} begins..."
./scripts/gen-rev-mem-optimized.sh Algorithms/${FILE}.scaffold #>${FILE}.err 2> ${FILE}.err2
tail -n 12 ${FILE}/*freq 
tail ${FILE}/*freq > ${FILE}.out 
