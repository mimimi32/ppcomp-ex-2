#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>

#define BS 256

double time_diff_sec(struct timeval st, struct timeval et)
{
    return (double)(et.tv_sec-st.tv_sec)+(et.tv_usec-st.tv_usec)/1000000.0;
}

// compute sum of v of all threads
// the result is stored in *dp, after all threads finish
__device__ void calc_sum(double *dp, int n, double v)
{
    __shared__ double vs[BS]; // vs[] is on shared memory of this thread block
    int tid = threadIdx.x;

    // reduction inside thread block
    vs[threadIdx.x] = v;
    __syncthreads();

    for (int stride = BS / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            vs[tid] += vs[tid + stride];
        }
        __syncthreads();
    }

    // now sum in thread block is at vs[0]
    // final reduction
    if (tid == 0) {
        // do *dp += vs[0] in atomic
        atomicAdd(dp, vs[0]);
    }

    return;
}

__global__ void pi_kernel(int n, double *dp)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    double v = 0.0;

    double dx = 1.0 / (double)n;
    double x;
    double y;
    
    if (i < n) {
        x = (double)i * dx;
        y = sqrt(1.0 - x*x);
        v = dx*y;
    }
    else {
        v = 0.0;
    }

    // compute sum of v
    calc_sum(dp, n, v);
}

double pi(int n, double *dp)
{
    double sum = 0.0;

    // copy sum(=0.0) to dp
    cudaMemcpy(dp, &sum, sizeof(double), cudaMemcpyDefault);
    
    pi_kernel<<<(n+BS-1)/BS, BS, BS*sizeof(double)>>>(n, dp);

    // copy final result from dp to sum
    cudaMemcpy(&sum, dp, sizeof(double), cudaMemcpyDefault);
    
    return 4.0*sum;
}

int main(int argc, char *argv[])
{
    int n;
    int i;
    double *dp;

    if (argc < 2) {
        printf("Specify #divisions.\n");
        exit(1);
    }

    n = atoi(argv[1]);

    // temporal device buffer for calc_sum
    cudaMalloc((void**)&dp, sizeof(double));
    
    /* Repeat same computation for 5 times */
    for (i = 0; i < 5; i++) {
        struct timeval st;
        struct timeval et;
        double sec;
        double res;

        gettimeofday(&st, NULL); /* get start time */
        res = pi(n, dp);
        cudaDeviceSynchronize();
        gettimeofday(&et, NULL); /* get end time */

        sec = time_diff_sec(st, et);
        printf("Result=%.15lf: Pi took %lf sec --> %.3lf Gsamples/sec\n",
               res, sec, (double)n/sec/1e+9);
    }

    cudaFree(dp);
    return 0;
}
