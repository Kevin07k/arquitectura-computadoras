#include <stdbool.h>
#include <stdio.h>

bool restar_y_detectar_zero(int a, int b, int *resultado) {
    unsigned char zero_flag;

    __asm__("subl %[val_b], %[val_a]\n\t" // val_a = val_a - val_b (actualiza
                                          // banderas)
            "setz %[zf]\n\t"       // zf = 1 si ZF == 1, de lo contrario 0
            : [val_a] "+r"(a),     // Entrada/Salida 'a' almacena el resultado
              [zf] "=q"(zero_flag) // Salida: captura de la bandera ZF
            : [val_b] "r"(b)       // Entrada: 'b'
            : "cc" // Informa al compilador que cambiamos las banderas
                   // (condition codes)
    );

    *resultado = a;
    return (bool)zero_flag;
}

int main() {
    int res;

    // Caso 1: Los valores son iguales (activa ZF)
    bool es_cero = restar_y_detectar_zero(42, 42, &res);
    printf("42 - 42 = %d | Zero Flag activa: %s\n", res, es_cero ? "SI" : "NO");

    // Caso 2: Los valores son distintos (no activa ZF)
    es_cero = restar_y_detectar_zero(50, 20, &res);
    printf("50 - 20 = %d | Zero Flag activa: %s\n", res, es_cero ? "SI" : "NO");

    return 0;
}
