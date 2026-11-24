/* Compilar: gcc -std=c11 -Wall -Wextra -pedantic escribir_monitor_linux.c -o monitor_linux
 * Ejecutar: ./monitor_linux
 * Ejecutar escritor y lector de forma secuencial: comparten BDA_FILE.
 * BIOS/puertos simulados; TIOCSTI y dispositivos Linux dependen del sistema.
 */
#define _POSIX_C_SOURCE 200809L
/* Base: codigo del PDF del laboratorio. Ajustado para ejecucion en Linux. */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#if defined(__linux__)
#include <sys/ioctl.h>
#include <linux/fb.h>
#endif

void opcion_linux_framebuffer(void) {
printf("\n====================================================================\n");
printf(" OPCIÓN 1: LINUX DIRECT FRAMEBUFFER (/dev/fb0 con mmap)              \n");
printf("====================================================================\n");
#if defined(__linux__)
int fbfd = open("/dev/fb0", O_RDWR);
if (fbfd == -1) {
perror(">> Error al abrir /dev/fb0 (Ejecuta con sudo o en consola TTY)");
return;
}
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
    ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) < 0) {
    perror("ioctl framebuffer"); close(fbfd); return;
}
if (vinfo.bits_per_pixel != 32 || vinfo.xres < 100 || vinfo.yres < 100 ||
    vinfo.red.length != 8 || vinfo.green.length != 8 || vinfo.blue.length != 8 ||
    vinfo.red.offset > 24 || vinfo.green.offset > 24 || vinfo.blue.offset > 24) {
    fprintf(stderr, "Se requiere framebuffer RGB de 32 bits y al menos 100x100.\n");
    close(fbfd); return;
}

size_t screensize = finfo.smem_len;
if (screensize == 0) { close(fbfd); return; }
uint8_t *vram = (uint8_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
if (vram == MAP_FAILED) { perror(">> Error mmap"); close(fbfd); return; }

int start_x = vinfo.xres / 2 - 50;
int start_y = vinfo.yres / 2 - 50;
int bpp = vinfo.bits_per_pixel / 8;

for (int y = start_y; y < start_y + 100; y++) {
for (int x = start_x; x < start_x + 100; x++) {
size_t loc = ((size_t)x + vinfo.xoffset) * (unsigned)bpp +
             ((size_t)y + vinfo.yoffset) * finfo.line_length;
if (loc <= screensize && screensize - loc >= 4) {
    uint32_t blanco = (UINT32_C(255) << vinfo.red.offset) |
                      (UINT32_C(255) << vinfo.green.offset) |
                      (UINT32_C(255) << vinfo.blue.offset);
    memcpy(vram + loc, &blanco, sizeof blanco);
}
}
}
printf(">> Cuadrado blanco dibujado en VRAM física.\n");
munmap(vram, screensize); close(fbfd);
#else




printf(">> Funcionalidad exclusiva del kernel Linux (/dev/fb0).\n");
#endif
}

void opcion_linux_vcsa(void) {
printf("\n====================================================================\n");
printf(" OPCIÓN 2: MEMORIA DE VIDEO DE CONSOLA VIRTUAL (/dev/vcsa)           \n");
printf("====================================================================\n");
#if defined(__linux__)
int fd = open("/dev/vcsa", O_RDWR);
if (fd == -1) { perror(">> Error al abrir /dev/vcsa (Requiere sudo)"); return; }
uint8_t cabecera[4];
if (read(fd, cabecera, sizeof cabecera) != (ssize_t)sizeof cabecera ||
    lseek(fd, 4, SEEK_SET) < 0) {
    perror("cabecera vcsa"); close(fd); return;
}
size_t celdas = (size_t)cabecera[0] * cabecera[1];
const char *mensaje = "HOLA DESDE MEMORIA /dev/vcsa";
for (size_t i = 0; mensaje[i] != '\0' && i < celdas; i++) {
uint8_t celda[2] = {(uint8_t)mensaje[i], 0x1E};
if (write(fd, celda, 2) != 2) { perror("write vcsa"); close(fd); return; }
}
printf(">> Texto escrito en la memoria de la consola virtual.\n");
close(fd);
#else
printf(">> Funcionalidad exclusiva de consolas TTY en Linux (/dev/vcsa).\n");
#endif
}

void opcion_linux_dev_mem(void) {
printf("\n====================================================================\n");
printf(" OPCIÓN 3: MEMORIA FÍSICA BRUTA (/dev/mem con mmap y ASM)            \n");
printf("====================================================================\n");
printf("Ejemplo explicativo del PDF; no se ejecuta acceso a /dev/mem. C + ASM:\n");
printf(" int mem_fd = open(\"/dev/mem\", O_RDWR | O_SYNC);\n");
printf(" uint8_t *vram = (uint8_t *)mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, 0xB8000);\n");
printf(" char caracter = 'X';\n");
printf(" uint8_t color = 0x4F;\n");
printf(" __asm__ __volatile__ (\n");
printf("      \"movb %%0, (%%1)    \\n\\t\"\n");
printf("      \"movb %%2, 1(%%1)   \\n\\t\"\n");
printf("      : : \"r\"(caracter), \"r\"(vram), \"r\"(color) : \"memory\"\n");
printf(" );\n");
}

#define SHM_FILAS 10
#define SHM_COLS 40

typedef struct {
uint8_t ascii;
uint8_t attribute;
} __attribute__((packed)) CeldaVram;

void opcion_linux_shm_simulada(void) {
printf("\n====================================================================\n");
printf(" OPCIÓN 4: VRAM MAPEADA EN MEMORIA (mmap) CON PROYECCIÓN CRT        \n");
printf("====================================================================\n");
/* Archivo local persistente, compartible con otro proceso mediante mmap. */
FILE *archivo = fopen("vram_monitor.bin", "w+b");
if (!archivo) { perror("vram_monitor.bin"); return; }
size_t tam = SHM_FILAS * SHM_COLS * sizeof(CeldaVram);
for (size_t i = 0; i < tam; i++) {
    if (fputc(0, archivo) == EOF) { perror("inicializar VRAM"); fclose(archivo); return; }
}
if (fflush(archivo) != 0) { perror("fflush"); fclose(archivo); return; }
CeldaVram *pvram = mmap(NULL, tam, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(archivo), 0);
if (pvram == MAP_FAILED) { perror("mmap VRAM"); fclose(archivo); return; }

const char *linea1 = ">>> VRAM MAPEADA EN MEMORIA RAM <<<";
const char *linea2 = "Formato VGA: [Byte ASCII] + [Color]";
const char *linea3 = "Escrito directamente con punteros C";

for (int i = 0; linea1[i] != '\0'; i++) {
pvram[2 * SHM_COLS + 3 + i].ascii = (uint8_t)linea1[i];
pvram[2 * SHM_COLS + 3 + i].attribute = 0x1E;
}
for (int i = 0; linea2[i] != '\0'; i++) {
pvram[4 * SHM_COLS + 2 + i].ascii = (uint8_t)linea2[i];
pvram[4 * SHM_COLS + 2 + i].attribute = 0x2A;
}
for (int i = 0; linea3[i] != '\0'; i++) {
pvram[6 * SHM_COLS + 2 + i].ascii = (uint8_t)linea3[i];
pvram[6 * SHM_COLS + 2 + i].attribute = 0x4F;
}

printf(" +--- MONITOR DE VIDEO: LECTURA EN VIVO DE LA VRAM ---+\n");
for (int f = 0; f < SHM_FILAS; f++) {
printf(" |");
for (int c = 0; c < SHM_COLS; c++) {
CeldaVram cel = pvram[f * SHM_COLS + c];



char ch = (cel.ascii >= 32 && cel.ascii <= 126) ? (char)cel.ascii : ' ';
/* VGA: azul=1, rojo=4; ANSI usa rojo=1, azul=4. */
static const int ansi[8] = {0, 4, 2, 6, 1, 5, 3, 7};
int fg = 30 + ansi[cel.attribute & 0x07];
int bg = 40 + ansi[(cel.attribute >> 4) & 0x07];
int bold = (cel.attribute & 0x08) ? 1 : 0;
if (cel.attribute == 0) printf(" ");
else printf("\033[%d;%d;%dm%c\033[0m", bold, fg, bg, ch);
}
printf("|\n");
}
printf("   +----------------------------------------------------+\n");
if (msync(pvram, tam, MS_SYNC) != 0) perror("msync");
if (munmap(pvram, tam) != 0) perror("munmap");
fclose(archivo);
}

int main(void) {
int opcion;
printf("====================================================================\n");
printf(" ESCRITURA DIRECTA EN MEMORIA DEL MONITOR EN LINUX                  \n");
printf("====================================================================\n");
printf("Seleccione la opción de memoria de video en Linux:\n");
printf(" [1] LINUX DIRECT FRAMEBUFFER (/dev/fb0 con mmap) -> Píxeles gráficos\n");
printf(" [2] CONSOLA VIRTUAL DE LINUX (/dev/vcsa) -> Modo texto [ASCII, Color]\n");
printf(" [3] MEMORIA FÍSICA BRUTA (/dev/mem con mmap y ASM) -> Bus físico\n");
printf(" [4] VRAM DIDÁCTICA EN MEMORIA (mmap) -> Monitor CRT simulado\n");
printf("Ingrese opción (1-4): ");

if (scanf("%d", &opcion) != 1) return 1;
switch (opcion) {
case 1: opcion_linux_framebuffer(); break;
case 2: opcion_linux_vcsa(); break;
case 3: opcion_linux_dev_mem(); break;
case 4: opcion_linux_shm_simulada(); break;
default: printf("Opción no válida.\n"); return 1;
}
return 0;
}
