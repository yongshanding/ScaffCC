#!/bin/bash
#test_array=( "best_adder_32" "best_adder_64" "best_multiplier_32" "best_multiplier_64" "best_modExp" "sha2" "salsa" "jasmine" "elsa" "belle")
test_array=("rd53" "6sym" "2of5" "best_adder_4")
#test_array=("best_adder_62")
#test_array=( "jasmine" "elsa" "belle" "snowwhite" )
#test_array=( "elsa" )

for i in $(seq 0 $(((${#test_array[*]}-1))))
do
	#cp rev-memory-manager-hybrid_OPTG_swap.cpp scripts/rev-memory-manager-hybrid.cpp
	#./run.sh ${test_array[$i]} 
    #tail -n 15 ${test_array[$i]}/*.freq > output/${test_array[$i]}_eager.out

	cp rev-memory-manager-hybrid_EAGER.cpp scripts/rev-memory-manager-hybrid.cpp
	./run.sh ${test_array[$i]}
    cp ${test_array[$i]}/*.freq for_sim/${test_array[$i]}_EAGER.freq 
	cp rev-memory-manager-hybrid_NOFREE.cpp scripts/rev-memory-manager-hybrid.cpp
	./run.sh ${test_array[$i]}
    cp ${test_array[$i]}/*.freq for_sim/${test_array[$i]}_NOFREE.freq 
    #tail -n 15 ${test_array[$i]}/*.freq > output/${test_array[$i]}_nofree.out
	#cp rev-memory-manager-hybrid_OPTG.cpp scripts/rev-memory-manager-hybrid.cpp
	#./run.sh ${test_array[$i]}
	#cp rev-memory-manager-hybrid_EAGER.cpp scripts/rev-memory-manager-hybrid.cpp
	#./run.sh ${test_array[$i]}
done



