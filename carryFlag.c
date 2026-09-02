#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

bool sumar_uint8_con_carry(uint8_t a, uint8_t b, uint8_t *resultado) {
    unsigned char carry_flag;

    __asm__("addb %[val_b], %[val_a]\n\t" // Suma de 8 bits (byte)
            "setc %[cf]\n\t"              // cf = 1 si CF == 1
            : [val_a] "+q"(a),            // Entrada/Salida (registro de 8 bits)
              [cf] "=q"(carry_flag)
            : [val_b] "q"(b)
            : "cc");

    *resultado = a;
    return (bool)carry_flag;
}

int main() {
    uint8_t res;

    // Caso 1: Sin acarreo (200 + 50 = 250 <= 255)
    bool hay_carry = sumar_uint8_con_carry(200, 50, &res);
    printf("200 + 50 = %u | Carry Flag: %s\n", res,
           hay_carry ? "SI (Acarreo)" : "NO");

    // Caso 2: Con acarreo (200 + 60 = 260 > 255 -> res en 8 bits es 4)
    hay_carry = sumar_uint8_con_carry(200, 60, &res);
    printf("200 + 60 = %u | Carry Flag: %s\n", res,
           hay_carry ? "SI (Acarreo)" : "NO");

    return 0;
}
