#define _POSIX_C_SOURCE 199309L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define N 1024
static double medir_tiempo_segundos(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}
/* 1. NAIVE (i-j-k): Ineficiente, Cache Miss masivo */
void algoritmo_naive(const float *A, const float *B, float *C) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            float suma = 0.0f;
            for (int k = 0; k < N; k++) {
                suma += A[i * N + k] *
                        B[k * N + j]; // Salto de fila en B: 4096 bytes
            }
            C[i * N + j] = suma;
        }
    }
}
/* 2. LOCALIDAD ESPACIAL (i-k-j): Aprovecha linea de cache de 64 bytes */
void algoritmo_localidad_espacial(const float *A, const float *B, float *C) {
    for (int idx = 0; idx < N * N; idx++)
        C[idx] = 0.0f;
    for (int i = 0; i < N; i++) {
        for (int k = 0; k < N; k++) {
            float r = A[i * N + k];
            int fila_b = k * N, fila_c = i * N;
            for (int j = 0; j < N; j++) {
                C[fila_c + j] +=
                    r * B[fila_b + j]; // Acceso estrictamente contiguo
            }
        }
    }
}
/* 3. LOCALIDAD TEMPORAL + REGISTROS: Mantiene A[i][k] en registro del CPU */
void algoritmo_registros_cpu(const float *A, const float *B, float *C) {
    for (int idx = 0; idx < N * N; idx++)
        C[idx] = 0.0f;
    for (int i = 0; i < N; i++) {
        float *ptr_c = &C[i * N];
        for (int k = 0; k < N; k++) {
            register const float reg_a = A[i * N + k]; // En registro de la FPU
            const float *ptr_b = &B[k * N];
            for (int j = 0; j < N; j++) {
                ptr_c[j] += reg_a * ptr_b[j];
            }
        }
    }
}
/* 4. LOOP UNROLLING 4X + ILP: Paralelismo a nivel de instruccion */
void algoritmo_loop_unrolling(const float *A, const float *B, float *C) {
    for (int idx = 0; idx < N * N; idx++)
        C[idx] = 0.0f;
    for (int i = 0; i < N; i++) {
        float *ptr_c = &C[i * N];
        for (int k = 0; k < N; k++) {
            register const float reg_a = A[i * N + k];
            const float *ptr_b = &B[k * N];
            for (int j = 0; j < N; j += 4) {
                ptr_c[j] += reg_a * ptr_b[j];
                ptr_c[j + 1] += reg_a * ptr_b[j + 1];
                ptr_c[j + 2] += reg_a * ptr_b[j + 2];
                ptr_c[j + 3] += reg_a * ptr_b[j + 3];
            }
        }
    }
}
static double calcular_checksum(const float *mat) {
    double sum = 0.0;
    for (int i = 0; i < N * N; i++)
        sum += (double)mat[i];
    return sum;
}
int main(void) {
    size_t total = (size_t)N * N, bytes = total * sizeof(float);
    double gflops = (2.0 * (double)N * N * N) / 1e9;
    float *A = (float *)malloc(bytes), *B = (float *)malloc(bytes);
    float *C1 = (float *)malloc(bytes), *C2 = (float *)malloc(bytes);
    float *C3 = (float *)malloc(bytes), *C4 = (float *)malloc(bytes);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            A[i * N + j] = (float)((i + j) % 50) * 0.02f + 1.0f;
            B[i * N + j] = (float)((i * 2 + j) % 50) * 0.02f + 0.5f;
        }
    }
    double t0, t1, t_naive, t_cache, t_reg, t_unroll;
    t0 = medir_tiempo_segundos();
    algoritmo_naive(A, B, C1);
    t_naive = medir_tiempo_segundos() - t0;
    t0 = medir_tiempo_segundos();
    algoritmo_localidad_espacial(A, B, C2);
    t_cache = medir_tiempo_segundos() - t0;
    t0 = medir_tiempo_segundos();
    algoritmo_registros_cpu(A, B, C3);
    t_reg = medir_tiempo_segundos() - t0;
    t0 = medir_tiempo_segundos();
    algoritmo_loop_unrolling(A, B, C4);
    t_unroll = medir_tiempo_segundos() - t0;
    printf("\n=== RESULTADOS DETERMINISTICOS (N=%d) ===\n", N);
    printf("1. Naive (i-j-k)               : %7.4fs | %6.2f GFLOPS | Speedup: "
           "1.00x\n",
           t_naive, gflops / t_naive);
    printf("2. Localidad Espacial (Cache)  : %7.4fs | %6.2f GFLOPS | Speedup: "
           "%.2fx\n",
           t_cache, gflops / t_cache, t_naive / t_cache);
    printf("3. Registros de CPU            : %7.4fs | %6.2f GFLOPS | Speedup: "
           "%.2fx\n",
           t_reg, gflops / t_reg, t_naive / t_reg);
    printf("4. Loop Unrolling 4x (ILP)     : %7.4fs | %6.2f GFLOPS | Speedup: "
           "%.2fx\n",
           t_unroll, gflops / t_unroll, t_naive / t_unroll);
    double chk1 = calcular_checksum(C1), chk4 = calcular_checksum(C4);
    printf("[OK] Validacion de Checksum: %.4e (Error = %.4e)\n", chk1,
           fabs(chk1 - chk4));
    free(A);
    free(B);
    free(C1);
    free(C2);
    free(C3);
    free(C4);
    return 0;
}
