#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/sysinfo.h>

#define CHUNK_SIZE_MB 64
#define CHUNK_BYTES   (CHUNK_SIZE_MB * 1024ULL * 1024ULL)

// Node structure to preserve pointers and prevent compiler dead-code elimination
typedef struct Node {
    char *buffer;
    struct Node *next;
} Node;

// Reads current system memory and swap usage directly from sysinfo
static void log_memory_state(size_t total_allocated_mb) {
    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        perror("sysinfo error");
        return;
    }

    unsigned long long unit = si.mem_unit ? si.mem_unit : 1;
    double total_ram_gb  = (double)(si.totalram * unit) / (1024.0 * 1024.0 * 1024.0);
    double free_ram_gb   = (double)(si.freeram * unit)  / (1024.0 * 1024.0 * 1024.0);
    double total_swap_gb = (double)(si.totalswap * unit)/ (1024.0 * 1024.0 * 1024.0);
    double free_swap_gb  = (double)(si.freeswap * unit) / (1024.0 * 1024.0 * 1024.0);
    double used_swap_gb  = total_swap_gb - free_swap_gb;

    printf("[Allocated: %6zu MB] RAM Free: %.2f/%.2f GB | Swap Used: %.2f/%.2f GB\n",
           total_allocated_mb, free_ram_gb, total_ram_gb, used_swap_gb, total_swap_gb);
    fflush(stdout);
}

int main(void) {
    Node *head = NULL;
    size_t total_mb = 0;

    printf("=== Memory Saturation & Swap Pagination Utility ===\n");
    printf("Target Chunk Size: %d MB per cycle\n", CHUNK_SIZE_MB);
    printf("Starting allocation loop. Press Ctrl+C to abort.\n\n");

    while (1) {
        // 1. Allocate dynamic virtual memory block
        char *chunk = (char *)malloc(CHUNK_BYTES);
        if (!chunk) {
            fprintf(stderr, "\n[FATAL] malloc() returned NULL at %zu MB allocated.\n", total_mb);
            break;
        }

        // 2. Force physical page commitment (dirtying pages)
        // Linux uses lazy allocation (overcommit). Writing to each page forces a page fault.
        memset(chunk, 0xAA, CHUNK_BYTES);

        // 3. Track block in a singly-linked list to keep references alive
        Node *node = (Node *)malloc(sizeof(Node));
        if (!node) {
            fprintf(stderr, "\n[FATAL] Failed to allocate tracker node.\n");
            free(chunk);
            break;
        }
        node->buffer = chunk;
        node->next = head;
        head = node;

        total_mb += CHUNK_SIZE_MB;

        // 4. Inspect status
        log_memory_state(total_mb);

        // Small delay to observe metrics in htop/vmstat without instant OOM panic
        usleep(50000); 
    }

    printf("\nExecution halted. Cleaning up allocated heap blocks...\n");
    while (head) {
        Node *temp = head;
        head = head->next;
        free(temp->buffer);
        free(temp);
    }

    return 0;
}
