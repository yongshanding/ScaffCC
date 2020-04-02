#!/bin/bash
#test_array=( "best_adder_32" "best_adder_64" "best_multiplier_32" "best_multiplier_64" "best_modExp" "sha2" "salsa" "jasmine" "elsa" "belle" "snowwhite")
#test_array=( "jasmine" "elsa" "belle" "snowwhite" )
test_array=( "2of5" "rd53" "6sym" "best_adder_4")


for i in $(seq 0 $(((${#test_array[*]}-1))))
do
    python3 freq2qiskit.py ${test_array[$i]}_eager.freq 16 > ${test_array[$i]}_eager.circ
    python3 freq2qiskit.py ${test_array[$i]}_opt.freq 16 > ${test_array[$i]}_opt.circ
    python3 freq2qiskit.py ${test_array[$i]}_lazy.freq 25 > ${test_array[$i]}_lazy.circ
done



