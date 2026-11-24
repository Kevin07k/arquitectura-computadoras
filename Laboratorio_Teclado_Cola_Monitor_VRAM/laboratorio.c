
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

#define HEAD  0x78
#define TAIL  0x79
#define COUNT 0x7A
#define FIFO  0x70
#define VRAM  0xF0
#define CAP   8

static uint8_t ram[256];
static uint8_t port_status, port_data;
static unsigned descartadas;
static unsigned cursor;
static struct termios original;
static int raw_activo;

/* Control del teclado de Ubuntu */
static void restaurar_terminal(void) {
    if (raw_activo)
        tcsetattr(STDIN_FILENO, TCSANOW, &original);
}

static void activar_terminal(void) {
    if (tcgetattr(STDIN_FILENO, &original) != 0) {
        perror("tcgetattr");
        exit(1);
    }
    struct termios t = original;
    t.c_lflag &= ~(ICANON | ECHO | ISIG);
    t.c_iflag &= ~(IXON | ICRNL);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) != 0) {
        perror("tcsetattr");
        exit(1);
    }
    raw_activo = 1;
    atexit(restaurar_terminal);
}

static int teclado_disponible(void) {
    fd_set fds;
    struct timeval tv = {0, 100000};
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds,
                  NULL, NULL, &tv) > 0;
}

/* E/S aislada: no accede a ram[] */
static uint8_t io_read(uint8_t puerto) {
    if (puerto == 0x01)
        return port_status;
    if (puerto == 0x02) {
        uint8_t dato = port_data;
        port_status = 0;
        return dato;
    }
    return 0;
}

/* Hardware de teclado simulado */
static void recibir_tecla(uint8_t ascii) {
    if (port_status & 1) return; /* Latch ocupado: no sobrescribir. */
    port_data = ascii;
    port_status = 1;
}

/* Cola circular en RAM */
static void fifo_inicializar(void) {
    ram[HEAD] = ram[TAIL] = ram[COUNT] = 0;
    memset(&ram[FIFO], 0, CAP);
    descartadas = 0;
}

static int fifo_insertar(uint8_t c) {
    if (ram[COUNT] == CAP) {
        descartadas++;
        return 0;
    }
    ram[FIFO + ram[TAIL]] = c;
    ram[TAIL] = (ram[TAIL] + 1) % CAP;
    ram[COUNT]++;
    return 1;
}

static int fifo_extraer(uint8_t *c) {
    if (ram[COUNT] == 0)
        return 0;
    *c = ram[FIFO + ram[HEAD]];
    ram[HEAD] = (ram[HEAD] + 1) % CAP;
    ram[COUNT]--;
    return 1;
}

static void mostrar_fifo(void) {
    printf("HEAD=%u TAIL=%u COUNT=%u | ",
           ram[HEAD], ram[TAIL], ram[COUNT]);
    for (int i = 0; i < CAP; i++)
        printf("%02X ", ram[FIFO + i]);
    printf("| descartadas=%u\n", descartadas);
}

/* Acceso a memoria mapeada VRAM */
static uint8_t vram_direccion(int fila, int columna) {
    return (uint8_t)(VRAM + fila * 4 + columna);
}

static void vram_inicializar(void) {
    for (int i = VRAM; i <= 0xFF; i++)
        ram[i] = ' ';
    cursor = 0;
}

static void vram_scroll(void) {
    /* Leer fila inferior y escribir en superior */
    for (int i = 0; i < 12; i++)
        ram[VRAM + i] = ram[VRAM + i + 4];

    for (int i = 12; i < 16; i++)
        ram[VRAM + i] = ' ';

    cursor = 12;
}

static void imprimir_monitor(void) {
    printf("\n      MONITOR 4x4\n");
    printf("     +---------+\n");
    for (int f = 0; f < 4; f++) {
        printf("     | ");
        for (int c = 0; c < 4; c++) {
            unsigned char ch = ram[vram_direccion(f,c)];
            putchar(ch >= 32 && ch <= 126 ? ch : '.');
            if (c != 3) putchar(' ');
        }
        printf(" |\n");
    }
    printf("     +---------+\n");
    printf("CURSOR=%u | HEAD=%u TAIL=%u COUNT=%u\n",
           cursor, ram[HEAD], ram[TAIL], ram[COUNT]);
    printf("Overflow FIFO: %u\n", descartadas);
}

/* Controlador interactivo */
static void procesar_caracter(uint8_t c) {
    if (c == 0x08 || c == 0x7F) {
        if (cursor > 0) {
            cursor--;
            ram[VRAM + cursor] = ' ';
        }
        return;
    }

    if (c == '\r' || c == '\n') {
        cursor = ((cursor / 4) + 1) * 4;
        if (cursor >= 16)
            vram_scroll();
        return;
    }

    if (c < 32 || c > 126) return;

    if (cursor >= 16)
        vram_scroll();

    ram[VRAM + cursor] = c;
    cursor++;
}

/* CPU: sondeo de estado, lectura y FIFO */
static void cpu_capturar(void) {
    if (io_read(0x01) & 1) {
        uint8_t ascii = io_read(0x02);
        fifo_insertar(ascii);
    }
}

static void cpu_consumir(void) {
    uint8_t c;
    while (fifo_extraer(&c))
        procesar_caracter(c);
}

/* Experimento 1 */
static void experimento1(void) {
    printf("\nEXP 1: LECTURA POR PUERTO\n");
    ram[0x02] = 0xAA; /* Centinela de RAM */
    recibir_tecla('A');
    printf("Puerto 0x01 antes: %u\n", io_read(0x01));
    uint8_t c = io_read(0x02);
    printf("Puerto 0x02: 0x%02X (%c)\n", c, c);
    printf("Puerto 0x01 despues: %u\n", io_read(0x01));
    printf("RAM[0x02] permanece: 0x%02X\n", ram[0x02]);
}

/* Experimento 2 */
static void experimento2(void) {
    printf("\nEXP 2: FIFO CIRCULAR\n");
    fifo_inicializar();
    for (int i = 0; i < 10; i++) {
        uint8_t c = 'A' + i;
        printf("Insertar %c: %s\n", c,
               fifo_insertar(c) ? "OK" : "OVERFLOW");
    }
    mostrar_fifo();

    uint8_t c;
    for (int i = 0; i < 5; i++) {
        if (fifo_extraer(&c))
            printf("Extraido: %c\n", c);
    }
    mostrar_fifo();

    /* Prueba de vuelta circular de TAIL */
    for (int i = 0; i < 5; i++)
        fifo_insertar('K' + i);
    mostrar_fifo();

    printf("Orden final de extraccion: ");
    while (fifo_extraer(&c)) putchar(c);
    putchar('\n');
    mostrar_fifo();
}

/* Experimento 3 */
static void experimento3(void) {
    printf("\nEXP 3: VRAM Y SCROLL\n");
    vram_inicializar();

    uint8_t dir = vram_direccion(2, 1);
    ram[dir] = 'X';
    printf("Direccion (2,1): 0x%02X\n", dir);
    printf("Lectura VRAM[0xF9]: %c\n", ram[dir]);

    /* Llenar pantalla y desplazar */
    for (int i = 0; i < 16; i++)
        ram[VRAM + i] = 'A' + i;

    printf("Antes del scroll:\n");
    imprimir_monitor();
    vram_scroll();
    printf("Despues del scroll:\n");
    imprimir_monitor();
}

/* Reto integrador */
static void terminal_interactiva(void) {
    fifo_inicializar();
    vram_inicializar();
    activar_terminal();

    printf("\033[2J\033[H");
    printf("TERMINAL SIMULADA - Ctrl+C para salir\n");
    printf("Escribe caracteres o usa Backspace.\n");
    printf("La pantalla es de 4x4.\n");
    imprimir_monitor();

    for (;;) {
        if (!teclado_disponible())
            continue;

        /* Una lectura puede contener varias teclas */
        uint8_t buffer[128];
        ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (n == 0) return;
        if (n < 0) { perror("read"); return; }

        for (ssize_t i = 0; i < n; i++) {
            uint8_t c = buffer[i];
            if (c == 3) return; /* Ctrl+C */
            if (c == 127) c = 0x08;

            recibir_tecla(c);
            cpu_capturar();
        }

        cpu_consumir();

        printf("\033[H\033[J");
        printf("TERMINAL SIMULADA - Ctrl+C para salir\n");
        printf("Escribe caracteres o usa Backspace.\n");
        printf("La pantalla es de 4x4.\n");
        imprimir_monitor();
        fflush(stdout);
    }
}

int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "texto")) {
        fifo_inicializar();
        vram_inicializar();
        for (const unsigned char *p = (const unsigned char *)argv[2]; *p; p++) {
            recibir_tecla(*p);
            cpu_capturar();
            cpu_consumir();
        }
        imprimir_monitor();
        return 0;
    }
    if (argc < 2) {
        printf("Uso: %s [exp1|exp2|exp3|terminal|texto CADENA]\n", argv[0]);
        return 1;
    }

    if (!strcmp(argv[1], "exp1"))
        experimento1();
    else if (!strcmp(argv[1], "exp2"))
        experimento2();
    else if (!strcmp(argv[1], "exp3"))
        experimento3();
    else if (!strcmp(argv[1], "terminal"))
        terminal_interactiva();
    else {
        fprintf(stderr, "Opcion desconocida: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
