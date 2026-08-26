#include <stdio.h>
#define TAMANO 5

int main(void) {
    int arreglo[TAMANO] = {0}; // Inicializado en ceros
    __asm__ (
        "movl $0, %%ecx\n\t"          // %%ecx = contador del bucle (i = 0)
        "mi_bucle:\n\t"               // Inicio de la etiqueta del bucle
        "cmpl %1, %%ecx\n\t"          // Compara 'i' (%%ecx) con TAMANO (%1)
        "jge fin_bucle\n\t"           // Si i >= TAMANO, sal del bucle (Jump if Greater or Equal)
        // Asignación: arreglo[i] = (i + 1) * 10
        "movl %%ecx, %%eax\n\t"       // %%eax = i
        "addl $1, %%eax\n\t"          // %%eax = i + 1
        "imull $10, %%eax\n\t"        // %%eax = (i + 1) * 10
        // Guardar valor en la memoria: base + (índice * tamaño_elemento)
        "movl %%eax, (%0, %%rcx, 4)\n\t" // arreglo[i] = %%eax
        "incl %%ecx\n\t"              // i++ (incrementa contador)
        "jmp mi_bucle\n\t"            // Salta de vuelta al inicio del bucle
        "fin_bucle:\n\t"
        :                             // No hay operandos de salida (modificamos la memoria directamente)
        : "r" (arreglo), "r" (TAMANO) // %0 = dirección base del arreglo, %1 = número de elementos
        : "%rax", "%rcx", "memory"    // Registros modificados + avisa modificación de memoria
    );

    // Verificación e impresión de los valores
    printf("Contenido del arreglo:\n");
    for (int i = 0; i < TAMANO; i++) {
        printf("arreglo[%d] = %d\n", i, arreglo[i]);
    }
    return 0;
}
