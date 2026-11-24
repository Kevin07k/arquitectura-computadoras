# Computer Architecture Laboratory: Keyboard Ports, Circular FIFO and Video Memory (VRAM)

## Description

Design and develop an interactive terminal in assembly language that integrates keyboard input through isolated I/O ports, an eight-byte circular FIFO in RAM, and a memory-mapped 4×4 character display. The program polls keyboard status at port 0x01, reads ASCII data from port 0x02, and inserts incoming characters into the queue at addresses 0x70–0x77. Queue management uses KB_HEAD, KB_TAIL and KB_COUNT at addresses 0x78, 0x79 and 0x7A, with circular pointer updates, empty-queue detection and rejection of new characters when the queue is full.

The main loop retrieves characters in FIFO order and writes them to VRAM at addresses 0xF0–0xFF using a cursor initially positioned at 0xF0. Display addressing follows 0xF0 + (row × 4) + column. The laboratory includes VRAM reads and writes, vertical scrolling, and Backspace support using ASCII 0x08: when the cursor is beyond the first cell, it moves back one position and replaces the previous character with a space (0x20). It also compares isolated I/O control signals with memory-access signals and explains why reading port 0x02 does not access RAM address 0x02.

## Purpose

Developed to investigate how a CPU communicates with input and display devices through separate I/O and memory address spaces. The laboratory provides practical experience with keyboard polling, circular-buffer management, memory-mapped video, cursor control and scrolling. It reinforces the distinction between IO_READ/IO_WRITE and MEM_READ/MEM_WRITE and integrates these concepts into a terminal that captures, buffers, displays and deletes characters while respecting the specified memory map.

## Requested By

Ing. Paulo César Loayza Carrasco.
