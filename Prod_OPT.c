#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedBusBuffer));
    SharedBusBuffer *shm = mmap(0, sizeof(SharedBusBuffer), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    sem_init(&shm->sem_read, 1, 0);
    sem_init(&shm->sem_write, 1, 1);
    printf("[Prod_OPT] Iniciando transferencia optimizada (Zero-Copy Batching)...\n");
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int b = 0; b < TOTAL_BATCHES; b++)
    {
        sem_wait(&shm->sem_write);
        // Llenar lote directamente en memoria compartida (Zero-Copy)
        for (int i = 0; i < BATCH_SIZE; i++)
        {
            shm->batch[i].id = b * BATCH_SIZE + i;
            memset(shm->batch[i].payload, 'B', sizeof(shm->batch[i].payload));
        }
        sem_post(&shm->sem_read);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double pps = NUM_PACKETS / elapsed;
    double mbps = (NUM_PACKETS * PKT_SIZE) / (1024.0 * 1024.0 * elapsed);
    printf("\n=== METRICAS PRODUTOR OPTIMIZADO (Prod_OPT) ===\n");
    printf("Paquetes enviados : %d\n", NUM_PACKETS);
    printf("Tiempo total : %.4f segundos\n", elapsed);
    printf("Velocidad (PPS) : %.0f paquetes/seg\n", pps);
    printf("Rendimiento : %.2f MB/s\n", mbps);
    munmap(shm, sizeof(SharedBusBuffer));
    close(shm_fd);
    return 0;
}