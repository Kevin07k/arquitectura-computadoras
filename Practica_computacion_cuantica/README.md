# Quantum Computing Laboratory: "Quantum Hello World" and the Bell State (|Φ⁺⟩)

## Description

Design and develop a two-qubit quantum circuit in Python using Qiskit that prepares the Bell state |Φ⁺⟩ = (|00⟩ + |11⟩)/√2. The circuit uses two qubits (q0, q1) and two classical bits. A Hadamard gate on q0 places it in an equiprobable superposition of |0⟩ and |1⟩. A CNOT gate with q0 as control and q1 as target then entangles the pair, producing maximum entanglement. Both qubits are measured in the computational basis and the results are stored in the classical register.

The circuit is executed on the AerSimulator backend with 10,000 shots, and the counts are collected with `get_counts()`. The laboratory also prints the ASCII diagram of the circuit with `qc.draw("text")` and analyzes the statistics: only the states |00⟩ and |11⟩ appear, each close to 50%, while |01⟩ and |10⟩ never occur (0.00%). The laboratory explains why this shows 100% quantum correlation: the qubits never disagree when measured, even though each individual outcome is random.

## Purpose

Developed to introduce the basic building blocks of quantum computation: qubits, superposition, entanglement and measurement. The laboratory provides practical experience with building circuits (`QuantumCircuit`), applying gates (H, CNOT), measuring in the computational basis and running statistical simulations with shots. It contrasts a classical bit, which holds a single definite value, with a qubit in superposition, and shows that the Bell state produces correlations that independent classical bits cannot reproduce.

## How to Run

```sh
source Practica_computacion_cuantica/.venv/bin/activate
python3 Practica_computacion_cuantica/lab1_hola_mundo.py
```

Expected output: `{'00': ~5000, '11': ~5000}`

## Requested By

Ing. Paulo César Loayza Carrasco.
