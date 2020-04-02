from qiskit import QuantumCircuit, execute
from qiskit import Aer, IBMQ
from qiskit.providers.aer.noise import NoiseModel
import qiskit.providers.aer.noise as noise
from qiskit.transpiler import CouplingMap, Layout
from qiskit.transpiler.passes import BasicSwap, LookaheadSwap, StochasticSwap
import time
import numpy as np
import sys

# Choose a real device to simulate from IBMQ provider
#provider = IBMQ.load_account()
#backend = provider.get_backend('ibmq_vigo')
app_name = sys.argv[1]
print(app_name)

numq = 18
coupling = [[0, 1], [1, 2], [2, 3], [3, 4], [5, 6], [6, 7], [7, 8], [8, 9], [10, 11], [11, 12], [12, 13], [13, 14], [15, 16], [16, 17], [0, 5], [5, 10], [10, 15], [1, 6], [6, 11], [11, 16], [2, 7], [7, 12], [12, 17], [3, 8], [8, 13], [4, 9], [9, 14]]
coupling_map = CouplingMap(couplinglist=coupling)



# Generate an Aer noise model for device
#noise_model1 = NoiseModel.from_backend(backend)
#basis_gates1 = noise_model1.basis_gates



# Generate an Aer noise model as depolarizing noise

# Error probabilities
prob_1 = 0.001  # 1-qubit gate
prob_2 = 0.01   # 2-qubit gate

# Depolarizing quantum errors
error_1 = noise.depolarizing_error(prob_1, 1)
error_2 = noise.depolarizing_error(prob_2, 2)

# Add errors to noise model
noise_model2 = noise.NoiseModel()
noise_model2.add_all_qubit_quantum_error(error_1, ['h', 't', 'tdg'])
noise_model2.add_all_qubit_quantum_error(error_2, ['cx', 'swap'])

# Get basis gates from noise model
basis_gates2 = noise_model2.basis_gates

# Generate an Aer noise model as depolarizing noise and t1/t2 relaxation

# Error probabilities
prob_1 = 0.001  # 1-qubit gate
prob_2 = 0.01   # 2-qubit gate

# Depolarizing quantum errors
error_1 = noise.depolarizing_error(prob_1, 1)
error_2 = noise.depolarizing_error(prob_2, 2)


# Amplitude and phase damping quantum errors
# T1 and T2 values for qubits 0-24
T1s = np.random.normal(50e3, 10e3, numq) # Sampled from normal distribution mean 50 microsec
T2s = np.random.normal(70e3, 10e3, numq)  # Sampled from normal distribution mean 70 microsec

# Truncate random T2s <= T1s
T2s = np.array([min(T2s[j], 2 * T1s[j]) for j in range(numq)])

# Instruction times (in nanoseconds)
time_h = 50   # u2(0,pi)
time_t = 0  # u1(pi/4)
time_tdg = 0 # u1(-pi/4)
time_cx = 300
time_swap = 300
#time_reset = 1000  # 1 microsecond
time_measure = 1000 # 1 microsecond

# QuantumError objects
#errors_reset = [thermal_relaxation_error(t1, t2, time_reset)
#                for t1, t2 in zip(T1s, T2s)]
errors_measure = [noise.thermal_relaxation_error(t1, t2, time_measure)
                  for t1, t2 in zip(T1s, T2s)]
errors_h  = [noise.thermal_relaxation_error(t1, t2, time_h).compose(error_1)
              for t1, t2 in zip(T1s, T2s)]
errors_t  = [noise.thermal_relaxation_error(t1, t2, time_t).compose(error_1)
              for t1, t2 in zip(T1s, T2s)]
errors_tdg  = [noise.thermal_relaxation_error(t1, t2, time_tdg).compose(error_1)
              for t1, t2 in zip(T1s, T2s)]
errors_cx = [[noise.thermal_relaxation_error(t1a, t2a, time_cx).expand(
             noise.thermal_relaxation_error(t1b, t2b, time_cx))
              for t1a, t2a in zip(T1s, T2s)]
               for t1b, t2b in zip(T1s, T2s)]
errors_swap = [[noise.thermal_relaxation_error(t1a, t2a, time_swap).expand(
             noise.thermal_relaxation_error(t1b, t2b, time_swap))
              for t1a, t2a in zip(T1s, T2s)]
               for t1b, t2b in zip(T1s, T2s)]

print(len(errors_cx))
# Add errors to noise model
noise_thermal = NoiseModel()
#noise_thermal.add_all_qubit_quantum_error(error_1, ['h', 't', 'tdg'])
#noise_thermal.add_all_qubit_quantum_error(error_2, ['cx', 'swap'])
for j in range(numq):
    #noise_thermal.add_quantum_error(errors_reset[j], "reset", [j])
    noise_thermal.add_quantum_error(errors_measure[j], "measure", [j])
    noise_thermal.add_quantum_error(errors_h[j], "h", [j])
    noise_thermal.add_quantum_error(errors_t[j], "t", [j])
    noise_thermal.add_quantum_error(errors_tdg[j], "tdg", [j])
    for k in range(numq):
        if j != k:
            noise_thermal.add_quantum_error(errors_cx[j][k], "cx", [j, k])
            noise_thermal.add_quantum_error(errors_swap[j][k], "swap", [j, k])


# Get basis gates from noise model
basis_gates3 = noise_thermal.basis_gates

#load(nisq_sim/best_adder_4_lazy.circ)
exec(open(app_name).read())

backend = Aer.get_backend('qasm_simulator')

print("===== No noise ================")
sys.stdout.flush()
job = execute(circ, backend, coupling_map=coupling_map, shots=1024*8)

result = job.result()

print(result.get_counts(0))

print("With noise_model3 ===========")
sys.stdout.flush()

backend = Aer.get_backend('qasm_simulator')
job = execute(circ, backend, coupling_map=coupling_map, noise_model=noise_thermal, basis_gates=basis_gates3, shots=1024*8)

result = job.result()

print(result.get_counts(0))


#print("===== With noise_model3 ===========")
#job = execute(circ, backend, coupling_map=coupling_map, noise_model=noise_thermal, basis_gates=basis_gates3, shots=1024*8)

#result = job.result()

#print(result.get_counts(0))
