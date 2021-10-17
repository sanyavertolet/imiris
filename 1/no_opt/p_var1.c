#include <stdio.h>
#include <inttypes.h>
#include <x86intrin.h>

#define SIZE 1024
#define FREQ 2000000000

uint64_t rdtsc() {
    uint64_t lower, higher;
    __asm__ __volatile__ ("rdtsc" : "=a"(lower), "=d"(higher));
    return ((((uint64_t) higher) << 32) | (uint64_t) lower);
}


double a[SIZE][SIZE];                            //matrix A
double b[SIZE][SIZE];                            //matrix B
double c[SIZE][SIZE];                            //matrix C
int i;

void main(int argc, char **argv) {
    int j,k;
    uint64_t start = rdtsc();
    for(i = 0; i < SIZE; i++) { 
        for(j = 0; j < SIZE; j++) {
            b[i][j] = 2.0;                       //init matrix B
            c[i][j] = 1.9;                       //init matrix C
        }
    }
    for(j = 0; j < SIZE; j++) {
        for(i = 0; i < SIZE; i++) {
            a[i][j] = 0;                              
            for(k = 0; k < SIZE; k++) {
                a[i][j] += b[i][k] + c[k][j];
            }        
        }
    }
    uint64_t end = rdtsc();
    printf("%" PRIu64 " cycles\n", end - start);
    printf("%lf sec\n", (double)((end - start)) / FREQ);
}
