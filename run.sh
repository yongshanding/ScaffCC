FILE=$1
if [ "$FILE" = "" ]
then
	echo "need a input file"
	exit 0
fi

cd build/
make -j 12 #>/dev/null 2>/dev/null 
cd -  
rm -rf ${FILE}*  
rm -rf scripts/rev-memory-manager-hybrid.bcpp  
echo "[run.sh] ${FILE} begins..."
./scripts/gen-rev-mem-optimized.sh Algorithms/${FILE}.scaffold #>${FILE}.err 2> ${FILE}.err2
tail -n 14 ${FILE}/*freq 
tail ${FILE}/*freq > ${FILE}.out 
echo $FILE >> tsv_order.txt
