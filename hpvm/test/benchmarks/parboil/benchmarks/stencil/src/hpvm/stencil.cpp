
/***************************************************************************
 *cr
 *cr            (C) Copyright 2010 The Board of Trustees of the
 *cr                        University of Illinois
 *cr                         All Rights Reserved
 *cr
 ***************************************************************************/

#include "common.h"
#include "file.h"
#include <hpvm.h>
#include <parboil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_data(float *A0, int nx, int ny, int nz, FILE *fp) {
  int s = 0;
  int i, j, k;
  for (i = 0; i < nz; i++) {
    for (j = 0; j < ny; j++) {
      for (k = 0; k < nx; k++) {
        fread(A0 + s, sizeof(float), 1, fp);
        s++;
      }
    }
  }
  return 0;
}

typedef struct __attribute__((__packed__)) {
  float c0, c1;
  float *A0;
  size_t bytes_A0;
  float *Anext;
  size_t bytes_Anext;
  int nx, ny, nz;
  size_t dim_X1, dim_Y1, dim_Z1;
  size_t dim_X2, dim_Y2, dim_Z2;
} RootIn;

void naive_kernel(float c0, float c1, float *A0, size_t bytes_A0, float *Anext,
                  size_t bytes_Anext, int nx, int ny, int nz) {
  __hpvm__hint(hpvm::DEVICE);
  __hpvm__attributes(2, A0, Anext, 1, Anext);

  void *thisNode = __hpvm__getNode();
  void *parentNode = __hpvm__getParentNode(thisNode);

  int lx = __hpvm__getNodeInstanceID_x(thisNode);
  int ly = __hpvm__getNodeInstanceID_y(thisNode);
  int lz = __hpvm__getNodeInstanceID_z(thisNode);

  int gx = __hpvm__getNodeInstanceID_x(parentNode);
  int gy = __hpvm__getNodeInstanceID_y(parentNode);
  int gz = __hpvm__getNodeInstanceID_z(parentNode);

  int gridx = __hpvm__getNumNodeInstances_x(thisNode);
  int gridy = __hpvm__getNumNodeInstances_y(thisNode);
  int gridz = __hpvm__getNumNodeInstances_z(thisNode);

  int i = gx * gridx + lx + 1;
  int j = gy * gridy + ly + 1;
  int k = gz * gridz + lz + 1;

  if (i < nx - 1) {
    Anext[Index3D(nx, ny, i, j, k)] = c1 * (A0[Index3D(nx, ny, i, j, k + 1)] +
                                            A0[Index3D(nx, ny, i, j, k - 1)] +
                                            A0[Index3D(nx, ny, i, j + 1, k)] +
                                            A0[Index3D(nx, ny, i, j - 1, k)] +
                                            A0[Index3D(nx, ny, i + 1, j, k)] +
                                            A0[Index3D(nx, ny, i - 1, j, k)]) -
                                      A0[Index3D(nx, ny, i, j, k)] * c0;
  }
}

void stencilLvl1(float c0, float c1, float *A0, size_t bytes_A0, float *Anext,
                 size_t bytes_Anext, int nx, int ny, int nz, size_t dim_X1,
                 size_t dim_Y1, size_t dim_Z1) {
  __hpvm__hint(hpvm::DEVICE);
  __hpvm__attributes(2, A0, Anext, 1, Anext);
  void *stencil_node =
      __hpvm__createNodeND(3, naive_kernel, dim_X1, dim_Y1, dim_Z1);
  __hpvm__bindIn(stencil_node, 0, 0, 0);
  __hpvm__bindIn(stencil_node, 1, 1, 0);
  __hpvm__bindIn(stencil_node, 2, 2, 0);
  __hpvm__bindIn(stencil_node, 3, 3, 0);
  __hpvm__bindIn(stencil_node, 4, 4, 0);
  __hpvm__bindIn(stencil_node, 5, 5, 0);
  __hpvm__bindIn(stencil_node, 6, 6, 0);
  __hpvm__bindIn(stencil_node, 7, 7, 0);
  __hpvm__bindIn(stencil_node, 8, 8, 0);
}

void stencilLvl2(float c0, float c1, float *A0, size_t bytes_A0, float *Anext,
                 size_t bytes_Anext, int nx, int ny, int nz, size_t dim_X1,
                 size_t dim_Y1, size_t dim_Z1, size_t dim_X2, size_t dim_Y2,
                 size_t dim_Z2) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(2, A0, Anext, 1, Anext);
  void *stencil_node =
      __hpvm__createNodeND(3, stencilLvl1, dim_X2, dim_Y2, dim_Z2);
  __hpvm__bindIn(stencil_node, 0, 0, 0);
  __hpvm__bindIn(stencil_node, 1, 1, 0);
  __hpvm__bindIn(stencil_node, 2, 2, 0);
  __hpvm__bindIn(stencil_node, 3, 3, 0);
  __hpvm__bindIn(stencil_node, 4, 4, 0);
  __hpvm__bindIn(stencil_node, 5, 5, 0);
  __hpvm__bindIn(stencil_node, 6, 6, 0);
  __hpvm__bindIn(stencil_node, 7, 7, 0);
  __hpvm__bindIn(stencil_node, 8, 8, 0);
  __hpvm__bindIn(stencil_node, 9, 9, 0);
  __hpvm__bindIn(stencil_node, 10, 10, 0);
  __hpvm__bindIn(stencil_node, 11, 11, 0);
}

void stencilLvl3(float c0, float c1, float *A0, size_t bytes_A0, float *Anext,
                 size_t bytes_Anext, int nx, int ny, int nz, size_t dim_X1,
                 size_t dim_Y1, size_t dim_Z1, size_t dim_X2, size_t dim_Y2,
                 size_t dim_Z2) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(2, A0, Anext, 1, Anext);
  void *stencil_node = __hpvm__createNodeND(0, stencilLvl2);
  __hpvm__bindIn(stencil_node, 0, 0, 0);
  __hpvm__bindIn(stencil_node, 1, 1, 0);
  __hpvm__bindIn(stencil_node, 2, 2, 0);
  __hpvm__bindIn(stencil_node, 3, 3, 0);
  __hpvm__bindIn(stencil_node, 4, 4, 0);
  __hpvm__bindIn(stencil_node, 5, 5, 0);
  __hpvm__bindIn(stencil_node, 6, 6, 0);
  __hpvm__bindIn(stencil_node, 7, 7, 0);
  __hpvm__bindIn(stencil_node, 8, 8, 0);
  __hpvm__bindIn(stencil_node, 9, 9, 0);
  __hpvm__bindIn(stencil_node, 10, 10, 0);
  __hpvm__bindIn(stencil_node, 11, 11, 0);
  __hpvm__bindIn(stencil_node, 12, 12, 0);
  __hpvm__bindIn(stencil_node, 13, 13, 0);
  __hpvm__bindIn(stencil_node, 14, 14, 0);
}

int main(int argc, char **argv) {
  struct pb_TimerSet timers;
  struct pb_Parameters *parameters;

  printf("OpenCL accelerated 7 points stencil codes****\n");
  printf("Author: Li-Wen Chang <lchang20@illinois.edu>\n");
  parameters = pb_ReadParameters(&argc, argv);

  /*pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);*/

  // declaration
  int nx, ny, nz;
  size_t size;
  int iteration;
  float c0 = 1.0 / 6.0;
  float c1 = 1.0 / 6.0 / 6.0;

  if (argc < 5) {
    printf("Usage: probe nx ny nz t\n"
           "nx: the grid size x\n"
           "ny: the grid size y\n"
           "nz: the grid size z\n"
           "t: the iteration time\n");
    return -1;
  }

  nx = atoi(argv[1]);
  if (nx < 1)
    return -1;
  ny = atoi(argv[2]);
  if (ny < 1)
    return -1;
  nz = atoi(argv[3]);
  if (nz < 1)
    return -1;
  iteration = atoi(argv[4]);
  if (iteration < 1)
    return -1;

  // host data
  float *h_A0;
  float *h_Anext;

  // load data from files

  size = nx * ny * nz;

  h_A0 = (float *)malloc(sizeof(float) * size);
  h_Anext = (float *)malloc(sizeof(float) * size);

  /*pb_SwitchToTimer(&timers, pb_TimerID_IO);*/
  FILE *fp = fopen(parameters->inpFiles[0], "rb");
  read_data(h_A0, nx, ny, nz, fp);
  fclose(fp);

  pb_InitializeTimerSet(&timers);
  __hpvm__init();

  pb_SwitchToTimer(&timers, hpvm_TimerID_MEM_TRACK);
  llvm_hpvm_track_mem(h_A0, sizeof(float) * size);
  llvm_hpvm_track_mem(h_Anext, sizeof(float) * size);

  pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);

  memcpy(h_Anext, h_A0, sizeof(float) * size);

  // only use 1D thread block
  size_t tx = 256;
  size_t block[3] = {tx, 1, 1};
  size_t grid[3] = {((unsigned)nx - 2 + tx - 1) / tx * tx, (unsigned)ny - 2,
                    (unsigned)nz - 2};
  // size_t grid[3] = {nx-2,ny-2,nz-2};
  size_t offset[3] = {1, 1, 1};

  printf("grid(%ld, %ld, %ld), block(%ld, %ld, %ld)\n", grid[0], grid[1],
         grid[2], block[0], block[1], block[2]);
  // main execution

  int t;
  size_t bytes = size * sizeof(float);
  printf("A[126,1,1] = %f\n", h_A0[Index3D(nx, ny, 126, 1, 1)]);
  printf("A[125,1,1] = %f\n", h_A0[Index3D(nx, ny, 125, 1, 1)]);
  for (t = 0; t < iteration; t++) {
    pb_SwitchToTimer(&timers, pb_TimerID_NONE);

    void *root_in = malloc(sizeof(RootIn));
    RootIn root_in_local = {c0,
                            c1,
                            h_A0,
                            bytes,
                            h_Anext,
                            bytes,
                            nx,
                            ny,
                            nz,
                            block[0],
                            block[1],
                            block[2],
                            grid[0] / block[0],
                            grid[1] / block[1],
                            grid[2] / block[2]};
    *(RootIn *)root_in = root_in_local;
    void *stencilDFG = __hpvm__launch(0, stencilLvl3, root_in);

    __hpvm__wait(stencilDFG);
    // printf("iteration %d\n",t);
    pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);
    float *h_temp = h_A0;
    h_A0 = h_Anext;
    h_Anext = h_temp;
  }

  float *h_temp = h_A0;
  h_A0 = h_Anext;
  h_Anext = h_temp;
  pb_SwitchToTimer(&timers, pb_TimerID_COPY);
  llvm_hpvm_request_mem(h_Anext, bytes);
  printf("A[126,1,1] = %f\n", h_Anext[Index3D(nx, ny, 126, 1, 1)]);
  printf("A[125,1,1] = %f\n", h_Anext[Index3D(nx, ny, 125, 1, 1)]);

  pb_SwitchToTimer(&timers, hpvm_TimerID_MEM_UNTRACK);

  llvm_hpvm_untrack_mem(h_A0);
  llvm_hpvm_untrack_mem(h_Anext);

  pb_SwitchToTimer(&timers, pb_TimerID_NONE);
  pb_PrintTimerSet(&timers);

  __hpvm__cleanup();

  if (parameters->outFile) {
    /*pb_SwitchToTimer(&timers, pb_TimerID_IO);*/
    outputData(parameters->outFile, h_Anext, nx, ny, nz);
  }
  /*pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);*/
  free(h_A0);
  free(h_Anext);
  pb_FreeParameters(parameters);

  return 0;
}
