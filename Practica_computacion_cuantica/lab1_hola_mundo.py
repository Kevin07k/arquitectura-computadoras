from qiskit import QuantumCircuit
from qiskit_aer import AerSimulator

# 1. Definir circuito con 2 cúbits y 2 bits clásicos
qc = QuantumCircuit(2, 2)

# 2. Superposición equiprobable en q0 mediante Hadamard
qc.h(0)

# 3. Entrelazamiento máximo mediante CNOT (control: q0, target: q1)
qc.cx(0, 1)

# 4. Medición sobre la base computacional
qc.measure([0, 1], [0, 1])

# 5. Ejecutar simulación con 10,000 disparos (shots)
backend = AerSimulator()
counts = backend.run(qc, shots=10000).result().get_counts()

print(qc.draw("text", vertical_compression="low"))
print(counts)
# Salida esperada: {'00': ~5000, '11': ~5000}
