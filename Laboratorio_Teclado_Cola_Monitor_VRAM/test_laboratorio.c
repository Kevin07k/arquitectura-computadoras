/* Ejecuta las mismas funciones usadas por el programa interactivo. */
#define main laboratorio_main
#include "laboratorio.c"
#undef main
#include <assert.h>

static void enviar(const char *s) {
    for (; *s; s++) {
        recibir_tecla((uint8_t)*s);
        cpu_capturar();
        cpu_consumir();
    }
}

int main(void) {
    fifo_inicializar();
    vram_inicializar();
    ram[2] = 0xAA;
    recibir_tecla('A');
    recibir_tecla('B');
    assert(io_read(1) == 1);
    assert(io_read(2) == 'A');
    assert(io_read(1) == 0 && ram[2] == 0xAA);

    for (unsigned i = 0; i < 9; i++) {
        recibir_tecla((uint8_t)('A' + i));
        cpu_capturar();
    }
    assert(ram[COUNT] == 8 && descartadas == 1);
    assert(io_read(1) == 0);
    for (unsigned i = 0; i < 8; i++) {
        uint8_t c;
        assert(fifo_extraer(&c) && c == 'A' + i);
    }
    uint8_t c;
    assert(!fifo_extraer(&c));
    assert(ram[HEAD] == 0 && ram[TAIL] == 0);
    enviar("XYZ");
    assert(ram[HEAD] == 3 && ram[TAIL] == 3 && ram[COUNT] == 0);

    vram_inicializar();
    enviar("\bAB\bC");
    assert(memcmp(&ram[VRAM], "AC  ", 4) == 0 && cursor == 2);
    vram_inicializar();
    enviar("ABCDEFGHIJKLMNOP");
    assert(vram_direccion(2, 1) == 0xF9 && ram[0xF9] == 'J');
    assert(cursor == 16);
    enviar("\bP");
    assert(cursor == 16 && ram[0xFF] == 'P');
    enviar("Q");
    assert(memcmp(&ram[VRAM], "EFGHIJKLMNOPQ   ", 16) == 0);
    enviar("RSTUVWXYZ");
    assert(cursor <= 16);
    puts("OK: puertos, latch, FIFO, overflow, wrap, vacio, VRAM, Backspace y scroll.");
    return 0;
}
