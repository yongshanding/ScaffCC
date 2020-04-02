#!/bin/bash
#test_array=( "best_adder_32" "best_adder_64" "best_multiplier_32" "best_multiplier_64" "best_modExp" "sha2" "salsa" "jasmine" "elsa" "belle" "snowwhite")
#test_array=( "jasmine" "elsa" "belle" "snowwhite" )
#test_array=( "2of5" "rd53" "6sym" "best_adder_4")
test_array=( "little_jasmine" "little_elsa" "little_belle" ) 


OUT=nisq_sim

mkdir -p $OUT;

for i in $(seq 0 $(((${#test_array[*]}-1))))
do
	#cp rev-memory-manager-hybrid_simEAGER.cpp scripts/rev-memory-manager-hybrid.cpp
	#./run.sh ${test_array[$i]}
	#cp ${test_array[$i]}/*.freq $OUT/${test_array[$i]}_eager.freq

	#cp rev-memory-manager-hybrid_simNOFREE.cpp scripts/rev-memory-manager-hybrid.cpp
	#./run.sh ${test_array[$i]}
	#cp ${test_array[$i]}/*.freq $OUT/${test_array[$i]}_lazy.freq

	#cp rev-memory-manager-hybrid_simOPTG.cpp scripts/rev-memory-manager-hybrid.cpp
	#./run.sh ${test_array[$i]}
	#cp ${test_array[$i]}/*.freq $OUT/${test_array[$i]}_opt.freq

	cp rev-memory-manager-hybrid_simEAGER_LAAonly.cpp scripts/rev-memory-manager-hybrid.cpp
	./run.sh ${test_array[$i]}
	cp ${test_array[$i]}/*.freq $OUT/${test_array[$i]}_laa.freq
done



