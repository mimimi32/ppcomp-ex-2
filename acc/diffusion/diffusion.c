#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#define OUTPUT_DIR "diffusion_output"

#define NX 150
#define NY 150

float dens[2][NY][NX];

double time_diff_sec(struct timeval st, struct timeval et)
{
    return (double)(et.tv_sec-st.tv_sec)+(et.tv_usec-st.tv_usec)/1000000.0;
}

void init()
{
    int x, y;
    int cx = NX/2, cy = 0; /* center of ink */
    int rad = (NX+NY)/8; /* radius of ink */
    
    for(y = 0; y < NY; y++) {
        for(x = 0; x < NX; x++) {
            float v = 0.0;
            if (((x-cx)*(x-cx)+(y-cy)*(y-cy)) < rad*rad) {
                v = 1.0;
            }
            dens[0][y][x] = v;
            dens[1][y][x] = v;
        }
    }
    return;
}

/* Write density field to VTK legacy binary format (big-endian as required by VTK spec) */
void write_vtk(int step, int buf)
{
    char filename[64];
    snprintf(filename, sizeof(filename), OUTPUT_DIR "/diffusion_%04d.vtk", step);
    FILE *fp = fopen(filename, "wb");
    if (!fp) { perror(filename); return; }

    fprintf(fp, "# vtk DataFile Version 3.0\n");
    fprintf(fp, "Diffusion t=%d\n", step);
    fprintf(fp, "BINARY\n");
    fprintf(fp, "DATASET STRUCTURED_POINTS\n");
    fprintf(fp, "DIMENSIONS %d %d 1\n", NX, NY);
    fprintf(fp, "ORIGIN 0.0 0.0 0.0\n");
    fprintf(fp, "SPACING 1.0 1.0 1.0\n");
    fprintf(fp, "POINT_DATA %d\n", NX * NY);
    fprintf(fp, "SCALARS density float 1\n");
    fprintf(fp, "LOOKUP_TABLE default\n");

    float row[NX];
    for (int y = 0; y < NY; y++) {
        for (int x = 0; x < NX; x++) {
            float v = dens[buf][y][x];
            unsigned int u;
            memcpy(&u, &v, 4);
            /* swap to big-endian */
            u = ((u & 0xFF000000u) >> 24) | ((u & 0x00FF0000u) >> 8)
              | ((u & 0x0000FF00u) << 8)  | ((u & 0x000000FFu) << 24);
            memcpy(&row[x], &u, 4);
        }
        fwrite(row, sizeof(float), NX, fp);
    }
    fclose(fp);
    printf("Written %s\n", filename);
}

/* Calculate nt time steps, writing VTK every write_interval steps */
void calc(int nt, int write_interval)
{
    int t, x, y;

    for (t = 0; t < nt; t++) {
        int from = t % 2;
        int to   = (t + 1) % 2;

        for (y = 1; y < NY-1; y++) {
            for (x = 1; x < NX-1; x++) {
                dens[to][y][x] = 0.2 * (dens[from][y][x]
                                        + dens[from][y][x-1]
                                        + dens[from][y][x+1]
                                        + dens[from][y-1][x]
                                        + dens[from][y+1][x]);
            }
        }

        if ((t + 1) % write_interval == 0)
            write_vtk(t + 1, to);
    }

    return;
}

int  main(int argc, char *argv[])
{
    struct timeval t1, t2;
    int nt = 200;            /* number of time steps */
    int write_interval = 10; /* write VTK every N steps */

    if (argc >= 2) nt             = atoi(argv[1]);
    if (argc >= 3) write_interval = atoi(argv[2]);

    printf("nt=%d  write_interval=%d\n", nt, write_interval);

    mkdir(OUTPUT_DIR, 0755);

    init();
    write_vtk(0, 0);

    gettimeofday(&t1, NULL);

    calc(nt, write_interval);

    gettimeofday(&t2, NULL);

    /* write final step if not already covered by the interval */
    if (nt % write_interval != 0)
        write_vtk(nt, nt % 2);

    {
        double sec;
        double gflops;
        int op_per_point = 5; // 4 add & 1 multiply per point

        sec = time_diff_sec(t1, t2);
        printf("Elapsed time: %.3lf sec\n", sec);
        gflops = ((double)NX*NY*nt*op_per_point)/sec/1e+9;
        printf("Speed: %.3lf GFlops\n", gflops);
    }

    return 0;
}
