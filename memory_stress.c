#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <getopt.h>

#ifndef MADV_PAGEOUT
#define MADV_PAGEOUT 21
#endif

// Default configuration constants (in Megabytes)
#define DEFAULT_CHUNK_RAM_MB         64    // Chunk size during fast RAM filling
#define DEFAULT_CHUNK_SWAP_MB        16    // Smaller chunk size during Swap to give kernel I/O time
#define RAM_LOW_THRESHOLD_MB        400    // When free/available RAM falls below this, trigger SWAP phase
#define DEFAULT_SWAP_SAFETY_MARGIN_MB 500  // Keep at least this much SWAP free to prevent OOM panic / system freeze
#define PAGE_SIZE_BYTES            4096

// Node structure to preserve pointers and prevent compiler dead-code elimination
typedef struct Node {
    char *buffer;
    size_t size;
    bool paged_out;
    struct Node *next;
} Node;

// System memory metrics
typedef struct {
    double ram_total_gb;
    double ram_free_gb;
    double ram_avail_gb;
    double swap_total_gb;
    double swap_free_gb;
    double swap_used_gb;
    double swap_used_pct;
    size_t ram_avail_mb;
    size_t swap_free_mb;
    size_t swap_used_mb;
    size_t swap_total_mb;
} SystemMemory;

// Volatile flag for graceful Ctrl+C interruption
static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

// Reads precise memory and swap information from /proc/meminfo (with sysinfo fallback)
static bool get_system_memory(SystemMemory *sm) {
    memset(sm, 0, sizeof(*sm));

    FILE *f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[256];
        unsigned long long total_kb = 0, free_kb = 0, avail_kb = 0;
        unsigned long long swap_total_kb = 0, swap_free_kb = 0;

        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "MemTotal:", 9) == 0)
                sscanf(line + 9, "%llu", &total_kb);
            else if (strncmp(line, "MemFree:", 8) == 0)
                sscanf(line + 8, "%llu", &free_kb);
            else if (strncmp(line, "MemAvailable:", 13) == 0)
                sscanf(line + 13, "%llu", &avail_kb);
            else if (strncmp(line, "SwapTotal:", 10) == 0)
                sscanf(line + 10, "%llu", &swap_total_kb);
            else if (strncmp(line, "SwapFree:", 9) == 0)
                sscanf(line + 9, "%llu", &swap_free_kb);
        }
        fclose(f);

        if (avail_kb == 0) avail_kb = free_kb; // Fallback for very old kernels

        sm->ram_total_gb  = (double)total_kb / (1024.0 * 1024.0);
        sm->ram_free_gb   = (double)free_kb  / (1024.0 * 1024.0);
        sm->ram_avail_gb  = (double)avail_kb / (1024.0 * 1024.0);
        sm->ram_avail_mb  = (size_t)(avail_kb / 1024ULL);

        sm->swap_total_gb = (double)swap_total_kb / (1024.0 * 1024.0);
        sm->swap_free_gb  = (double)swap_free_kb  / (1024.0 * 1024.0);
        sm->swap_total_mb = (size_t)(swap_total_kb / 1024ULL);
        sm->swap_free_mb  = (size_t)(swap_free_kb / 1024ULL);

        unsigned long long swap_used_kb = (swap_total_kb >= swap_free_kb) ? (swap_total_kb - swap_free_kb) : 0;
        sm->swap_used_gb  = (double)swap_used_kb / (1024.0 * 1024.0);
        sm->swap_used_mb  = (size_t)(swap_used_kb / 1024ULL);
        sm->swap_used_pct = (swap_total_kb > 0) ? ((double)swap_used_kb * 100.0 / (double)swap_total_kb) : 0.0;
        return true;
    }

    // Fallback to sysinfo()
    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        perror("sysinfo error");
        return false;
    }

    unsigned long long unit = si.mem_unit ? si.mem_unit : 1;
    unsigned long long total_bytes = si.totalram * unit;
    unsigned long long free_bytes  = si.freeram * unit;
    unsigned long long swap_total  = si.totalswap * unit;
    unsigned long long swap_free   = si.freeswap * unit;
    unsigned long long swap_used   = (swap_total >= swap_free) ? (swap_total - swap_free) : 0;

    sm->ram_total_gb  = (double)total_bytes / (1024.0 * 1024.0 * 1024.0);
    sm->ram_free_gb   = (double)free_bytes  / (1024.0 * 1024.0 * 1024.0);
    sm->ram_avail_gb  = sm->ram_free_gb;
    sm->ram_avail_mb  = (size_t)(free_bytes / (1024ULL * 1024ULL));

    sm->swap_total_gb = (double)swap_total  / (1024.0 * 1024.0 * 1024.0);
    sm->swap_free_gb  = (double)swap_free   / (1024.0 * 1024.0 * 1024.0);
    sm->swap_used_gb  = (double)swap_used   / (1024.0 * 1024.0 * 1024.0);
    sm->swap_total_mb = (size_t)(swap_total / (1024ULL * 1024ULL));
    sm->swap_free_mb  = (size_t)(swap_free / (1024ULL * 1024ULL));
    sm->swap_used_mb  = (size_t)(swap_used / (1024ULL * 1024ULL));
    sm->swap_used_pct = (swap_total > 0) ? ((double)swap_used * 100.0 / (double)swap_total) : 0.0;
    return true;
}

static void print_banner(const SystemMemory *sm, size_t safety_margin_mb, size_t max_swap_target_mb) {
    printf("========================================================================\n");
    printf("    STRESS TEST: SATURACION DE RAM Y PAGINACION FORZADA A SWAP         \n");
    printf("    Arquitectura de Computadoras - Analisis de Jerarquia de Memoria     \n");
    printf("========================================================================\n");
    printf("[SISTEMA] RAM Total      : %.2f GB\n", sm->ram_total_gb);
    printf("[SISTEMA] RAM Disponible : %.2f GB (Libre: %.2f GB)\n", sm->ram_avail_gb, sm->ram_free_gb);
    printf("[SISTEMA] SWAP Total     : %.2f GB (%zu MB)\n", sm->swap_total_gb, sm->swap_total_mb);
    printf("[SISTEMA] SWAP en Uso    : %.2f GB (%.1f%%)\n", sm->swap_used_gb, sm->swap_used_pct);
    printf("[CONTROL] Margen Alarma  : %zu MB libres en Swap (Evita OOM Panic / Freeze)\n", safety_margin_mb);
    if (max_swap_target_mb > 0) {
        printf("[CONTROL] Limite Swap    : Forzar hasta %zu MB de Swap\n", max_swap_target_mb);
    } else {
        printf("[CONTROL] Limite Swap    : Forzar Swap maximo posible respetando margen de seguridad\n");
    }
    printf("------------------------------------------------------------------------\n");
    printf(" Presiona Ctrl+C en cualquier momento para abortar y liberar memoria.\n");
    printf("========================================================================\n\n");
    fflush(stdout);
}

// Log formatting with phase indicator
static void log_state(const char *phase, size_t total_mb, const SystemMemory *sm, const char *extra_note) {
    printf("[%s] Asignado: %6zu MB | RAM Disp: %5.2f GB | Swap Usado: %5.2f/%5.2f GB (%4.1f%%) %s\n",
           phase, total_mb, sm->ram_avail_gb, sm->swap_used_gb, sm->swap_total_gb, sm->swap_used_pct,
           extra_note ? extra_note : "");
    fflush(stdout);
}

static void show_help(const char *progname) {
    printf("Uso: %s [opciones]\n", progname);
    printf("Opciones:\n");
    printf("  -r, --chunk-ram <MB>     Tamano del bloque en fase RAM (defecto: %d MB)\n", DEFAULT_CHUNK_RAM_MB);
    printf("  -s, --chunk-swap <MB>    Tamano del bloque en fase Swap (defecto: %d MB)\n", DEFAULT_CHUNK_SWAP_MB);
    printf("  -m, --safety-margin <MB> Margen minimo de Swap libre de seguridad (defecto: %d MB)\n", DEFAULT_SWAP_SAFETY_MARGIN_MB);
    printf("  -t, --target-swap <MB>   Cantidad maxima de Swap a llenar en MB (0 = todo lo seguro)\n");
    printf("  -v, --verify-thrash      Realiza un barrido de lectura sobre paginas antiguas para forzar fallos de pagina mayores\n");
    printf("  -h, --help               Muestra esta ayuda\n");
}

int main(int argc, char *argv[]) {
    // Register signal handlers for clean, safe termination
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    size_t chunk_ram_mb = DEFAULT_CHUNK_RAM_MB;
    size_t chunk_swap_mb = DEFAULT_CHUNK_SWAP_MB;
    size_t safety_margin_mb = DEFAULT_SWAP_SAFETY_MARGIN_MB;
    size_t max_swap_target_mb = 0;
    bool verify_thrash = false;

    // CLI argument parsing
    static struct option long_options[] = {
        {"chunk-ram",     required_argument, 0, 'r'},
        {"chunk-swap",    required_argument, 0, 's'},
        {"safety-margin", required_argument, 0, 'm'},
        {"target-swap",   required_argument, 0, 't'},
        {"verify-thrash", no_argument,       0, 'v'},
        {"help",          no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "r:s:m:t:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'r': chunk_ram_mb = (size_t)strtoul(optarg, NULL, 10); break;
            case 's': chunk_swap_mb = (size_t)strtoul(optarg, NULL, 10); break;
            case 'm': safety_margin_mb = (size_t)strtoul(optarg, NULL, 10); break;
            case 't': max_swap_target_mb = (size_t)strtoul(optarg, NULL, 10); break;
            case 'v': verify_thrash = true; break;
            case 'h': show_help(argv[0]); return 0;
            default: show_help(argv[0]); return 1;
        }
    }

    SystemMemory sm;
    if (!get_system_memory(&sm)) {
        fprintf(stderr, "[ERROR] No se pudo leer el estado de memoria del sistema.\n");
        return 1;
    }

    if (sm.swap_total_mb == 0) {
        fprintf(stderr, "\n[ADVERTENCIA CRITICA] Tu sistema NO tiene particion o archivo SWAP activo!\n");
        fprintf(stderr, "El kernel no podra realizar paginacion secundaria. Si la RAM se agota, el OOM-Killer matara el proceso.\n");
        fprintf(stderr, "Para habilitar swap temporal: sudo fallocate -l 4G /swapfile && sudo chmod 600 /swapfile && sudo mkswap /swapfile && sudo swapon /swapfile\n\n");
    }

    print_banner(&sm, safety_margin_mb, max_swap_target_mb);

    // FIFO list: head is oldest block, tail is newest block
    Node *head = NULL;
    Node *tail = NULL;
    Node *pageout_cursor = NULL; // Points to oldest node not yet advised with MADV_PAGEOUT

    size_t total_mb = 0;
    bool swap_phase_active = false;
    bool reached_safety_ceiling = false;

    while (g_running) {
        if (!get_system_memory(&sm)) break;

        // Check if we reached the emergency safety reserve on Swap
        if (sm.swap_total_mb > 0) {
            if (sm.swap_free_mb <= safety_margin_mb) {
                printf("\n>>> [FRENO DE EMERGENCIA ACTIVADO] Margen de seguridad alcanzado: Swap libre (%zu MB) <= %zu MB <<<\n",
                       sm.swap_free_mb, safety_margin_mb);
                printf("Deteniendo nuevas asignaciones para evitar colapso del sistema o disparo del OOM Killer.\n");
                reached_safety_ceiling = true;
                break;
            }

            if (max_swap_target_mb > 0 && sm.swap_used_mb >= max_swap_target_mb) {
                printf("\n>>> [OBJETIVO ALCANZADO] Se alcanzaron los %zu MB solicitados en SWAP. <<<\n",
                       max_swap_target_mb);
                reached_safety_ceiling = true;
                break;
            }
        }

        // Determine current phase and allocation parameters
        bool ram_is_saturated = (sm.ram_avail_mb <= RAM_LOW_THRESHOLD_MB);
        size_t current_chunk_mb;
        useconds_t sleep_time_us;
        const char *phase_label;

        if (!ram_is_saturated && !swap_phase_active) {
            // FASE 1: Llenado rapido de memoria RAM fisica
            current_chunk_mb = chunk_ram_mb;
            sleep_time_us = 20000; // 20 ms
            phase_label = "RAM FILL";
        } else {
            // FASE 2: Saturacion de RAM alcanzada -> Forzar SWAP como memoria de emergencia
            if (!swap_phase_active) {
                swap_phase_active = true;
                pageout_cursor = head;
                printf("\n------------------------------------------------------------------------\n");
                printf(">>> RAM FISICA AGOTADA (Disponible <= %d MB) <<<\n", RAM_LOW_THRESHOLD_MB);
                printf(">>> ACTIVANDO MODO DE EMERGENCIA: FORZANDO PAGINACION A SWAP <<<\n");
                printf(" [1] Aplicando madvise(MADV_PAGEOUT) a bloques antiguos.\n");
                printf(" [2] Reduciendo paso a %zu MB para sincronizar con la velocidad I/O de disco.\n", chunk_swap_mb);
                printf("------------------------------------------------------------------------\n\n");
            }
            current_chunk_mb = chunk_swap_mb;
            sleep_time_us = 80000; // 80 ms (permite a kswapd y direct reclaim escribir al disco sin saturar la cola)
            phase_label = "FORCING SWAP";
        }

        size_t chunk_bytes = current_chunk_mb * 1024ULL * 1024ULL;

        // 1. Allocate page-aligned dynamic virtual memory
        // Page alignment is required for madvise() system calls.
        char *chunk = NULL;
        int alloc_res = posix_memalign((void **)&chunk, PAGE_SIZE_BYTES, chunk_bytes);
        if (alloc_res != 0 || !chunk) {
            fprintf(stderr, "\n[AVISO] posix_memalign() no pudo reservar %zu MB (errno: %d).\n",
                    current_chunk_mb, alloc_res);
            break;
        }

        // 2. Force physical frame commitment (Dirtying pages)
        // Linux utiliza sobreasignacion (overcommit). Si no escribimos, solo son paginas virtuales.
        // Escribimos en cada bloque para forzar fallos de pagina menores y consumir memoria real.
        memset(chunk, 0xAA, chunk_bytes);

        // 3. Track block in FIFO linked list
        Node *node = (Node *)malloc(sizeof(Node));
        if (!node) {
            fprintf(stderr, "\n[ERROR] Fallo al reservar nodo de seguimiento.\n");
            free(chunk);
            break;
        }
        node->buffer = chunk;
        node->size = chunk_bytes;
        node->paged_out = false;
        node->next = NULL;

        if (!head) {
            head = tail = node;
            pageout_cursor = head;
        } else {
            tail->next = node;
            tail = node;
        }

        total_mb += current_chunk_mb;

        // 4. Force kernel eviction to SWAP if in SWAP phase
        // Le indicamos explícitamente al planificador de memoria virtual del kernel que desaloje
        // las paginas anonimas sucias mas antiguas al dispositivo de intercambio secundario (SWAP).
        const char *extra_note = NULL;
        if (swap_phase_active && pageout_cursor) {
            if (!pageout_cursor->paged_out) {
                int madv_res = madvise(pageout_cursor->buffer, pageout_cursor->size, MADV_PAGEOUT);
                if (madv_res == 0) {
                    pageout_cursor->paged_out = true;
                    extra_note = "<- [madvise MADV_PAGEOUT emitido]";
                }
            }
            pageout_cursor = pageout_cursor->next;
        }

        // 5. Inspect status
        get_system_memory(&sm);
        log_state(phase_label, total_mb, &sm, extra_note);

        usleep(sleep_time_us);
    }

    // Phase 3 (Optional or Verification): Paging Thrashing Demonstration
    if (g_running && (reached_safety_ceiling || verify_thrash) && head) {
        printf("\n========================================================================\n");
        printf(" ESTADO ESTABLE ALCANZADO (RAM saturada + SWAP en uso activo)\n");
        printf(" Total asignado por el proceso: %zu MB (%.2f GB)\n", total_mb, (double)total_mb / 1024.0);
        printf(" RAM disponible: %.2f GB | SWAP usado: %.2f/%.2f GB (%.1f%%)\n",
               sm.ram_avail_gb, sm.swap_used_gb, sm.swap_total_gb, sm.swap_used_pct);
        printf("========================================================================\n");

        if (verify_thrash) {
            printf("\n[VERIFICACION DE PAGINACION] Iniciando barrido de lectura sobre bloques antiguos...\n");
            printf("Al acceder a paginas expulsadas al Swap, la CPU generara Fallos de Pagina Mayores (Major Page Faults).\n");
            printf("Observa la columna 'si' (swap-in) y 'so' (swap-out) en 'vmstat 1'. Presiona Ctrl+C para finalizar.\n\n");

            size_t sweep_cycles = 0;
            while (g_running) {
                sweep_cycles++;
                Node *scan = head;
                size_t scanned_mb = 0;

                while (scan && g_running) {
                    // Touch 1 byte per 4KB page
                    volatile char dummy = 0;
                    for (size_t offset = 0; offset < scan->size; offset += PAGE_SIZE_BYTES) {
                        dummy += scan->buffer[offset];
                    }
                    (void)dummy;
                    scanned_mb += (scan->size / (1024 * 1024));

                    if (scanned_mb % 512 == 0) {
                        get_system_memory(&sm);
                        printf("[SWAP THRASH #%zu] Leyendo %zu MB | Swap Usado: %.2f GB | RAM Disp: %.2f GB\n",
                               sweep_cycles, scanned_mb, sm.swap_used_gb, sm.ram_avail_gb);
                        usleep(100000);
                    }
                    scan = scan->next;
                }
                usleep(500000);
            }
        } else {
            printf("\nManteniendo la memoria ocupada. Observa en otra terminal con 'free -h', 'htop' o 'vmstat 1'.\n");
            printf("Presiona Ctrl+C para liberar toda la memoria y finalizar limpiamente.\n");
            while (g_running) {
                sleep(1);
            }
        }
    }

    // Cleanup phase: safely release all allocated buffers
    printf("\n\n------------------------------------------------------------------------\n");
    printf("[FINALIZANDO] Liberando bloques de memoria del heap y restaurando SWAP...\n");
    size_t freed_count = 0;
    while (head) {
        Node *temp = head;
        head = head->next;
        free(temp->buffer);
        free(temp);
        freed_count++;
    }

    if (get_system_memory(&sm)) {
        printf("[ESTADO FINAL] Nodos liberados: %zu | RAM Disp: %.2f GB | Swap Usado: %.2f GB\n",
               freed_count, sm.ram_avail_gb, sm.swap_used_gb);
    }
    printf("Proceso finalizado correctamente. Memoria devuelta al sistema operativo.\n");
    printf("========================================================================\n");

    return 0;
}
