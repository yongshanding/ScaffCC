#!/bin/bash
#test_array=( "best_adder_32" "best_adder_64" "best_multiplier_32" "best_multiplier_64" "best_modExp" "sha2" "salsa" "jasmine" "elsa" "belle")
#test_array=( "jasmine" "elsa" "belle" "snowwhite" )
test_array=( "jasmine" )

src_folder=PARALLEL

for i in $(seq 0 $(((${#test_array[*]}-1))))
do
	cp $src_folder/rev-memory-manager-hybrid_p2.cpp scripts/rev-memory-manager-hybrid.cpp
	./run.sh ${test_array[$i]}
	cp $src_folder/rev-memory-manager-hybrid_p4.cpp scripts/rev-memory-manager-hybrid.cpp
	./run.sh ${test_array[$i]}
	cp $src_folder/rev-memory-manager-hybrid_p8.cpp scripts/rev-memory-manager-hybrid.cpp
	./run.sh ${test_array[$i]}
done



