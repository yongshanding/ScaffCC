#!/bin/bash
test_array=( "best_adder_32" "best_adder_64" "best_multiplier_32" "best_multiplier_64" "best_modExp" "sha2" "salsa")

for i in $(seq 0 $(((${#test_array[*]}-1))))
do
	cp rev-memory-manager-hybrid_EAGER.cpp scripts/rev-memory-manager-hybrid.cpp
	./run.sh ${test_array[$i]}
	cp rev-memory-manager-hybrid_NOFREE.cpp scripts/rev-memory-manager-hybrid.cpp
	./run.sh best_adder_32 ${test_array[$i]}
	cp rev-memory-manager-hybrid_OPTG.cpp scripts/rev-memory-manager-hybrid.cpp
	./run.sh best_adder_32 ${test_array[$i]}
done



