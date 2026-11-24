/* Compilar: gcc -std=c11 -Wall -Wextra -pedantic prog1_escribir_cola.c -o prog1
 * Ejecutar: ./prog1
 * Ejecutar escritor y lector de forma secuencial: comparten BDA_FILE.
 * BIOS/puertos simulados; TIOCSTI y dispositivos Linux dependen del sistema.
 */
/* Base: codigo del PDF del laboratorio. Ajustado para ejecucion en Linux. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

/* BDA_FILE permite aislar pruebas sin modificar la memoria de otra ejecucion.
 * Simulacion de BIOS: no se ejecutan interrupciones reales desde Linux.
 * 16 celdas fisicas, 15 utiles: se reserva una para distinguir llena/vacia.
 * Los puertos anexos no forman parte del mapa fisico de la BDA.
 */
static const char *archivo_bda(void) {
    const char *ruta = getenv("BDA_FILE");
    return ruta && *ruta ? ruta : "/tmp/bios_keyboard_bda.bin";
}
#define BDA_FILE archivo_bda()

#pragma pack(push, 1)
typedef struct {
uint16_t head;         /* Offset 0x001A: Puntero de lectura */
uint16_t tail;         /* Offset 0x001C: Puntero de escritura */
uint16_t buffer[16];   /* Offset 0x001E a 0x003D: 16 palabras (Scan-code en MSB, ASCII en LSB) */
uint8_t port_60_data; /* Registro de dato del Puerto 0x60 */
uint8_t port_64_status;/* Registro de estado del Puerto 0x64 (Bit 0: Tecla lista) */
} BiosKeyboardMemory;
#pragma pack(pop)

static void inicializar_bda(BiosKeyboardMemory *bda) {
    memset(bda, 0, sizeof(*bda));
    bda->head = bda->tail = 0x001E;
}

void cargar_bda(BiosKeyboardMemory *bda) {
    inicializar_bda(bda);
    FILE *f = fopen(BDA_FILE, "rb");
    if (!f) return;
    BiosKeyboardMemory temporal;
    int valido = fread(&temporal, sizeof temporal, 1, f) == 1;
    fclose(f);
    if (valido && temporal.head >= 0x1E && temporal.head <= 0x3C &&
        temporal.tail >= 0x1E && temporal.tail <= 0x3C &&
        temporal.head % 2 == 0 && temporal.tail % 2 == 0) {
        *bda = temporal;
    } else {
        fprintf(stderr, "BDA incompleta o punteros invalidos: se reinicializa.\n");
    }
}

void guardar_bda(const BiosKeyboardMemory *bda) {
    FILE *f = fopen(BDA_FILE, "wb");
    if (!f) { perror("guardar BDA"); exit(EXIT_FAILURE); }
    int correcto = fwrite(bda, sizeof(*bda), 1, f) == 1;
    if (fclose(f) != 0) correcto = 0;
    if (!correcto) { fprintf(stderr, "Error escribiendo BDA\n"); exit(EXIT_FAILURE); }
}

static uint8_t scan_ascii(int c) {
    static const uint8_t letras[26] = {
        0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,
        0x31,0x18,0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C
    };
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    if (c >= 'A' && c <= 'Z') return letras[c - 'A'];
    if (c >= '1' && c <= '9') return (uint8_t)(2 + c - '1');
    if (c == '0') return 0x0B;
    if (c == ' ') return 0x39;
    if (c == '\b') return 0x0E;
    return 0; /* Sin correspondencia en esta tabla didactica. */
}

void metodo_inyectado(const char *texto) {




printf("\n--- [OPCIÓN 1: INYECTANDO EN COLA DE ENTRADA TTY] ---\n");
#if defined(__unix__) || defined(__APPLE__)
int fd = open("/dev/tty", O_RDWR);
if (fd < 0) fd = STDIN_FILENO;
int i = 0;
while (texto[i] != '\0') {
if (ioctl(fd, TIOCSTI, &texto[i]) < 0) {
perror("Error en ioctl TIOCSTI");
fprintf(stderr, "El kernel puede deshabilitar TIOCSTI. No se modifican permisos; use las opciones BDA 2/3.\n");
if (fd != STDIN_FILENO) close(fd);
return;
}
i++;
}
if (fd != STDIN_FILENO) close(fd);
printf(">> ¡Éxito! Se inyectaron %d caracteres en la cola de entrada del terminal.\n", i);
#else
printf("TIOCSTI solo disponible en entornos POSIX / Unix.\n");
#endif
}

void metodo_asm_int16(char ascii, uint8_t scan_code) {
printf("\n--- [OPCIÓN 2: EN ASM CON INT 16h (SERVICIO 05h)] ---\n");
printf("Código Ensamblador equivalente en arquitectura x86:\n");
printf(" mov ah, 05h        ; AH = 05h (Insertar pulsación en buffer)\n");
printf(" mov ch, 0x%02X       ; CH = Scan-code (hardware)\n", scan_code);
printf(" mov cl, '%c' (0x%02X); CL = Código ASCII\n", ascii, (uint8_t)ascii);
printf(" int 16h            ; Llamada al vector de interrupción BIOS\n");

BiosKeyboardMemory bda;
cargar_bda(&bda);

uint16_t next_tail = (bda.tail >= 0x003C) ? 0x001E : bda.tail + 2;
if (next_tail == bda.head) {
printf(">> [BIOS INT 16h] AL = 1: BUFFER OVERFLOW. Buffer lleno.\n");
return;
}
int idx = (bda.tail - 0x001E) / 2;
bda.buffer[idx] = ((uint16_t)scan_code << 8) | (uint8_t)ascii;
bda.tail = next_tail;
bda.port_60_data = scan_code;
bda.port_64_status |= 0x01;

guardar_bda(&bda);
printf(">> [BIOS INT 16h] Tecla '%c' insertada con éxito. AL = 0.\n", ascii);
}

void metodo_directo_memoria(char ascii, uint8_t scan_code) {
printf("\n--- [OPCIÓN 3: DIRECTO A MEMORIA RAM (BDA 0x0040:0x001A)] ---\n");
BiosKeyboardMemory bda;
cargar_bda(&bda);

uint16_t next_tail = (bda.tail >= 0x003C) ? 0x001E : bda.tail + 2;
if (next_tail == bda.head) {
printf(">> [ERROR RAM] Buffer circular lleno en memoria física.\n");
return;
}

uint16_t offset_escrito = bda.tail;
int idx = (bda.tail - 0x001E) / 2;
bda.buffer[idx] = ((uint16_t)scan_code << 8) | (uint8_t)ascii;
bda.tail = next_tail;
bda.port_60_data = scan_code;
bda.port_64_status |= 0x01;

guardar_bda(&bda);
printf(">> [RAM BDA] Escribiendo palabra 0x%04X en offset 0x%04X\n", bda.buffer[idx], offset_escrito);
printf(">> [RAM BDA] Nuevo TAIL = 0x%04X | HEAD = 0x%04X\n", bda.tail, bda.head);
}

int main(void) {
int opcion;
char texto[100];
int c;

printf("====================================================================\n");
printf(" PROGRAMA 1: ESCRITOR EN COLA / MEMORIA DE TECLADO                  \n");
printf("====================================================================\n");
printf("Seleccione el método de escritura:\n");
printf(" [1] INYECTADO (En la cola de entrada del SO via ioctl TIOCSTI)\n");



printf(" [2] EN ASM CON INT 16h (Servicio 05h de la BIOS x86)\n");
printf(" [3] DIRECTO A MEMORIA (Segmento 0x0040:0x001A de la BIOS en RAM)\n");
printf("Ingrese opción (1-3): ");

if (scanf("%d", &opcion) != 1) return 1;
while ((c = getchar()) != '\n' && c != EOF);

switch (opcion) {
case 1:
printf("\nIngrese texto o palabra a inyectar: ");
if (fgets(texto, sizeof(texto), stdin)) {
size_t longitud = strlen(texto);
if (longitud == 0 || texto[longitud - 1] != '\n') {
    if (longitud + 1 >= sizeof texto) {
        fprintf(stderr, "Texto demasiado largo (maximo 98 caracteres).\n");
        return 1;
    }
    texto[longitud] = '\n';
    texto[longitud + 1] = '\0';
}
metodo_inyectado(texto);
}
break;
case 2:
printf("\nIngrese un carácter para enviar por INT 16h: ");
c = getchar();
if (c != '\n' && c != EOF) {
uint8_t sc = scan_ascii(c);
metodo_asm_int16(c, sc);
}
break;
case 3:
printf("\nIngrese un carácter para escribir directo a RAM: ");
c = getchar();
if (c != '\n' && c != EOF) {
metodo_directo_memoria((char)c, scan_ascii(c));
}
break;
default:
printf("Opción no válida.\n");
return 1;
}
return 0;
}
