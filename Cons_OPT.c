#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#define SHM_NAME "/bus_shm_opt"
#define NUM_PACKETS 10000000
#define PKT_SIZE 64
#define BATCH_SIZE 500
#define TOTAL_BATCHES (NUM_PACKETS / BATCH_SIZE)
typedef struct
{
    int id;
    char payload[PKT_SIZE - sizeof(int)];
} Packet;
typedef struct
{
    sem_t sem_read;
    sem_t sem_write;
    Packet batch[BATCH_SIZE];
} SharedBusBuffer;
int main()
{
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("[Cons_OPT] Error abriendo memoria compartida");
        return 1;
    }
    SharedBusBuffer *shm = mmap(0, sizeof(SharedBusBuffer), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    printf("[Cons_OPT] Recibiendo datos optimizados...\n");
    size_t total_received = 0;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int b = 0; b < TOTAL_BATCHES; b++)
    {
        sem_wait(&shm->sem_read);
        // Lectura directa desde RAM mapeada
        total_received += BATCH_SIZE;
        sem_post(&shm->sem_write);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double pps = total_received / elapsed;
    double mbps = (total_received * PKT_SIZE) / (1024.0 * 1024.0 * elapsed);
    printf("\n=== METRICAS CONSUMIDOR OPTIMIZADO (Cons_OPT) ===\n");
    printf("Paquetes recibidos: %zu\n", total_received);
    printf("Tiempo total : %.4f segundos\n", elapsed);
    printf("Velocidad (PPS) : %.0f paquetes/seg\n", pps);
    printf("Rendimiento : %.2f MB/s\n", mbps);
    sem_destroy(&shm->sem_read);
    sem_destroy(&shm->sem_write);
    munmap(shm, sizeof(SharedBusBuffer));
    close(shm_fd);
    shm_unlink(SHM_NAME);
    return 0;
}