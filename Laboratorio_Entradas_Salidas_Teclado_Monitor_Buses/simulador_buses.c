/* Seccion 5 del PDF: micropasos T0..T6 y conflicto de bus.
 * gcc -std=c11 -Wall -Wextra -pedantic simulador_buses.c -o buses
 * ./buses            ./buses conflicto
 * ISA didactica: opcode, direccion de un byte; LOAD/STORE/IN/OUT usan AX.
 * No es una emulacion x86: reproduce sus conceptos de buses y registros.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

enum { LOAD = 1, STORE, IN, OUT, HALT };
typedef struct {
    uint16_t pc, mar, ax;
    uint8_t mdr, ir, dir;
    uint16_t address_bus;
    uint8_t data_bus;
    bool mem_read, mem_write, io_read, io_write;
    uint8_t ram[256], puertos[256];
} CPU;

static void limpiar(CPU *c) {
    c->mem_read = c->mem_write = c->io_read = c->io_write = false;
}

static bool validar(const CPU *c) {
    if ((int)c->mem_read + c->mem_write + c->io_read + c->io_write > 1) {
        fprintf(stderr, "EXCEPCION: Conflicto de Bus (mas de una señal activa).\n");
        return false;
    }
    return true;
}

/* Un unico emisor: RAM o puerto en lectura; MDR en escritura.
 * La direccion del bus se alimenta exclusivamente de MAR.
 */
static void transferir(CPU *c) {
    if (!validar(c)) exit(EXIT_FAILURE);
    c->address_bus = c->mar;
    if (c->mar > 255) { fprintf(stderr, "Direccion invalida\n"); exit(EXIT_FAILURE); }
    if (c->mem_read) c->data_bus = c->ram[c->address_bus];
    if (c->io_read) {
        c->data_bus = c->puertos[c->address_bus];
        if (c->address_bus == 0x60) c->puertos[0x64] &= (uint8_t)~1u;
    }
    if (c->mem_write || c->io_write) c->data_bus = c->mdr;
    if (c->mem_write) c->ram[c->address_bus] = c->data_bus;
    if (c->io_write) c->puertos[c->address_bus] = c->data_bus;
}

static void mostrar(const CPU *c, unsigned t, const char *accion) {
    printf("T%u %-37s PC=%02X MAR=%02X MDR=%02X IR=%02X AX=%04X "
           "ADDR=%02X DATA=%02X MR=%d MW=%d IOR=%d IOW=%d\n",
           t, accion, c->pc, c->mar, c->mdr, c->ir, c->ax,
           c->address_bus, c->data_bus, c->mem_read, c->mem_write,
           c->io_read, c->io_write);
}

static bool instruccion(CPU *c) {
    limpiar(c); c->mar = c->pc;
    mostrar(c, 0, "MAR <- PC (bus interno)");
    c->mem_read = true; transferir(c); c->pc++;
    mostrar(c, 1, "AddrBus <- MAR; RAM -> DataBus; PC++");
    c->mdr = c->data_bus; c->ir = c->mdr; limpiar(c);
    mostrar(c, 2, "MDR <- DataBus; IR <- MDR");
    mostrar(c, 3, "UC decodifica IR");
    if (c->ir == HALT) return false;
    if (c->ir < LOAD || c->ir > OUT) {
        fprintf(stderr, "Opcode invalido\n"); exit(EXIT_FAILURE);
    }
    /* Lectura explicita del operando: no hay acceso magico a RAM. */
    c->mar = c->pc; c->mem_read = true; transferir(c);
    c->mdr = c->data_bus; c->dir = c->mdr; c->pc++;
    mostrar(c, 4, "Operando RAM -> bus -> MDR -> DIR");
    limpiar(c); c->mar = c->dir;
    if (c->ir == LOAD) c->mem_read = true;
    if (c->ir == IN) c->io_read = true;
    if (c->ir == STORE) { c->mdr = (uint8_t)c->ax; c->mem_write = true; }
    if (c->ir == OUT) { c->mdr = (uint8_t)c->ax; c->io_write = true; }
    transferir(c);
    mostrar(c, 5, "Transferencia externa segun opcode");
    if (c->ir == LOAD || c->ir == IN) { c->mdr = c->data_bus; c->ax = c->mdr; }
    limpiar(c);
    mostrar(c, 6, "Lectura: MDR -> AX; fin del ciclo");
    return true;
}

int main(int argc, char **argv) {
    CPU c = {0};
    if (argc == 2 && strcmp(argv[1], "conflicto") == 0) {
        c.mem_read = c.io_read = true;
        return validar(&c) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (argc != 1) { fprintf(stderr, "Uso: %s [conflicto]\n", argv[0]); return 1; }
    const uint8_t programa[] = { LOAD, 0x80, STORE, 0xF0, IN, 0x60, OUT, 0x61, HALT };
    memcpy(c.ram, programa, sizeof programa);
    c.ram[0x80] = 'A'; c.ram[0x60] = 0xAA;
    c.puertos[0x60] = 0x1E; c.puertos[0x64] = 1;
    while (instruccion(&c)) {}
    if (c.ram[0xF0] != 'A' || c.puertos[0x61] != 0x1E ||
        c.ram[0x60] != 0xAA || c.puertos[0x64] != 0) return 1;
    puts("OK: LOAD, STORE, IN, OUT; RAM/puertos aislados; un emisor por transferencia.");
    return 0;
}
