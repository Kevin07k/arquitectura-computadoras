#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#define FIFO_PATH "/tmp/bus_fifo_no_opt"
#define NUM_PACKETS 10000000
#define PKT_SIZE 64
typedef struct
{
    int id;
    char payload[PKT_SIZE - sizeof(int)];
} Packet;
int main()
{
    mkfifo(FIFO_PATH, 0666);
    printf("[Cons_NO] Conectando a FIFO...\n");
    int fd = open(FIFO_PATH, O_RDONLY);
    Packet pkt;
    size_t total_received = 0;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    // Lectura paquete por paquete (10,000,000 de syscalls read)
    while (total_received < NUM_PACKETS)
    {
        if (read(fd, &pkt, sizeof(Packet)) > 0)
        {
            total_received++;
        }
    }
    close(fd);
    unlink(FIFO_PATH);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double pps = total_received / elapsed;
    double mbps = (total_received * PKT_SIZE) / (1024.0 * 1024.0 * elapsed);
    printf("\n=== METRICAS CONSUMIDOR NO OPTIMIZADO (Cons_NO) ===\n");
    printf("Paquetes recibidos: %zu\n", total_received);
    printf("Tiempo total : %.4f segundos\n", elapsed);
    printf("Velocidad (PPS) : %.0f paquetes/seg\n", pps);
    printf("Rendimiento : %.2f MB/s\n", mbps);
    return 0;
}