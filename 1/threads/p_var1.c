#include <stdio.h>
#include <inttypes.h>
#include <omp.h>

#define SIZE 1024
#define FREQ 2000000000                             //proccessor's frequency

uint64_t rdtsc() {
    uint64_t lower, higher;
    __asm__ __volatile__ ("rdtsc" : "=a"(lower), "=d"(higher));
    return ((((uint64_t) higher) << 32) | (uint64_t) lower);
}


double a[SIZE][SIZE];                               //matrix A
double b[SIZE][SIZE];                               //matrix B
double c[SIZE][SIZE];                               //matrix C
int i;

void main(int argc, char **argv) {
    double b_precalc[SIZE];                         //sum of rows of matrix b
    double c_precalc[SIZE];                         //sum of rows of matrix c (transposed)
    int j,k;
    uint64_t start = rdtsc();                       //get start_tsc
    #pragma omp parallel for
    for(i = 0; i < SIZE; ++i) {                     //all for cycles now have ++i instead of i++ - less operations are performed
        for(j = 0; j < SIZE; ++j) {
            b[i][j] = 2.0;                          //init matrix B
            c[i][j] = 1.9;                          //init matrix C
        }
    }
    #pragma omp parallel for
    for(i = 0; i < SIZE; ++i) {
        b_precalc[i] = 0;
        c_precalc[i] = 0;
        for(j = 0; j < SIZE; ++j) {
            b_precalc[i] += b[i][j];
            c_precalc[i] += c[j][i];
        }
    }
    #pragma omp parallel for
    for(i = 0; i < SIZE; ++i) {                     //outer for now iterates through i
        for(j = 0; j < SIZE; ++j) {                 //inner for now iterates through j
            a[i][j] += b_precalc[i] + c_precalc[j];
        }
    }
    uint64_t end = rdtsc();                         //get end_tsc
    printf("%" PRIu64 " cycles\n", end - start);    //count cycles
    printf("%lf sec\n", (double)((end - start)) / FREQ);//count time
}

