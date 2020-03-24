/***************************************************************************
 *cr
 *cr            (C) Copyright 2010 The Board of Trustees of the
 *cr                        University of Illinois
 *cr                         All Rights Reserved
 *cr
 ***************************************************************************/

/*
 * Main entry of dense matrix-matrix multiplication kernel
 */

#include <hpvm.h>
#include <iostream>
#include <malloc.h>
#include <math.h>
#include <parboil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <vector>

// I/O routines
extern bool readColMajorMatrixFile(const char *fn, int &nr_row, int &nr_col,
                                   std::vector<float> &v);
extern bool writeColMajorMatrixFile(const char *fn, int, int,
                                    std::vector<float> &);
extern char *readFile(const char *);

// Parameters of tile sizes
#define TILE_SZ 16

#define CHECK_ERROR(errorMessage)                                              \
  if (clStatus != CL_SUCCESS) {                                                \
    std::cout << errorMessage << " Error!\n";                                  \
    std::cout << "Line: " << __LINE__ << "\n";                                 \
    exit(1);                                                                   \
  }

typedef struct __attribute__((__packed__)) {
  float *A;
  size_t bytes_A;
  int lda;
  float *B;
  size_t bytes_B;
  int ldb;
  float *C;
  size_t bytes_C;
  int ldc;
  int k;
  float alpha;
  float beta;
  size_t dim_X1, dim_Y1, dim_X2, dim_Y2;
} RootIn;

void mysgemmNT(float *A, size_t bytes_A, int lda, float *B, size_t bytes_B,
               int ldb, float *C, size_t bytes_C, int ldc, int k, float alpha,
               float beta) {
  __hpvm__hint(hpvm::DEVICE);
  __hpvm__attributes(3, A, B, C, 1, C);

  void *thisNode = __hpvm__getNode();
  void *parentNode = __hpvm__getParentNode(thisNode);
  int lx = __hpvm__getNodeInstanceID_x(thisNode);
  int ly = __hpvm__getNodeInstanceID_y(thisNode);
  int gx = __hpvm__getNodeInstanceID_x(parentNode);
  int gy = __hpvm__getNodeInstanceID_y(parentNode);
  int gridx = __hpvm__getNumNodeInstances_x(thisNode);
  int gridy = __hpvm__getNumNodeInstances_y(thisNode);
  int m = gx * gridx + lx;
  int n = gy * gridy + ly;

  float c = 0.0f;
  for (int i = 0; i < k; ++i) {
    float a = A[m + i * lda];
    float b = B[n + i * ldb];
    c += a * b;
  }
  C[m + n * ldc] = C[m + n * ldc] * beta + alpha * c;
}

void basicSgemmLvl1(float *A, size_t bytes_A, int lda, float *B, size_t bytes_B,
                    int ldb, float *C, size_t bytes_C, int ldc, int k,
                    float alpha, float beta, size_t dim_X1, size_t dim_Y1) {
  __hpvm__hint(hpvm::DEVICE);
  __hpvm__attributes(3, A, B, C, 1, C);
  void *sgemm_node =
      __hpvm__createNodeND(2, mysgemmNT, (size_t)dim_X1, (size_t)dim_Y1);
  __hpvm__bindIn(sgemm_node, 0, 0, 0);
  __hpvm__bindIn(sgemm_node, 1, 1, 0);
  __hpvm__bindIn(sgemm_node, 2, 2, 0);
  __hpvm__bindIn(sgemm_node, 3, 3, 0);
  __hpvm__bindIn(sgemm_node, 4, 4, 0);
  __hpvm__bindIn(sgemm_node, 5, 5, 0);
  __hpvm__bindIn(sgemm_node, 6, 6, 0);
  __hpvm__bindIn(sgemm_node, 7, 7, 0);
  __hpvm__bindIn(sgemm_node, 8, 8, 0);
  __hpvm__bindIn(sgemm_node, 9, 9, 0);
  __hpvm__bindIn(sgemm_node, 10, 10, 0);
  __hpvm__bindIn(sgemm_node, 11, 11, 0);
}

void basicSgemmLvl2(float *A, size_t bytes_A, int lda, float *B, size_t bytes_B,
                    int ldb, float *C, size_t bytes_C, int ldc, int k,
                    float alpha, float beta, size_t dim_X1, size_t dim_Y1,
                    size_t dim_X2, size_t dim_Y2) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(3, A, B, C, 1, C);
  void *sgemm_node =
      __hpvm__createNodeND(2, basicSgemmLvl1, (size_t)dim_X2, (size_t)dim_Y2);
  __hpvm__bindIn(sgemm_node, 0, 0, 0);
  __hpvm__bindIn(sgemm_node, 1, 1, 0);
  __hpvm__bindIn(sgemm_node, 2, 2, 0);
  __hpvm__bindIn(sgemm_node, 3, 3, 0);
  __hpvm__bindIn(sgemm_node, 4, 4, 0);
  __hpvm__bindIn(sgemm_node, 5, 5, 0);
  __hpvm__bindIn(sgemm_node, 6, 6, 0);
  __hpvm__bindIn(sgemm_node, 7, 7, 0);
  __hpvm__bindIn(sgemm_node, 8, 8, 0);
  __hpvm__bindIn(sgemm_node, 9, 9, 0);
  __hpvm__bindIn(sgemm_node, 10, 10, 0);
  __hpvm__bindIn(sgemm_node, 11, 11, 0);
  __hpvm__bindIn(sgemm_node, 12, 12, 0);
  __hpvm__bindIn(sgemm_node, 13, 13, 0);
}

// A wrapper level used in codegen for some backends
void basicSgemmLvl3(float *A, size_t bytes_A, int lda, float *B, size_t bytes_B,
                    int ldb, float *C, size_t bytes_C, int ldc, int k,
                    float alpha, float beta, size_t dim_X1, size_t dim_Y1,
                    size_t dim_X2, size_t dim_Y2) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(3, A, B, C, 1, C);
  void *sgemm_node = __hpvm__createNodeND(0, basicSgemmLvl2);
  __hpvm__bindIn(sgemm_node, 0, 0, 0);
  __hpvm__bindIn(sgemm_node, 1, 1, 0);
  __hpvm__bindIn(sgemm_node, 2, 2, 0);
  __hpvm__bindIn(sgemm_node, 3, 3, 0);
  __hpvm__bindIn(sgemm_node, 4, 4, 0);
  __hpvm__bindIn(sgemm_node, 5, 5, 0);
  __hpvm__bindIn(sgemm_node, 6, 6, 0);
  __hpvm__bindIn(sgemm_node, 7, 7, 0);
  __hpvm__bindIn(sgemm_node, 8, 8, 0);
  __hpvm__bindIn(sgemm_node, 9, 9, 0);
  __hpvm__bindIn(sgemm_node, 10, 10, 0);
  __hpvm__bindIn(sgemm_node, 11, 11, 0);
  __hpvm__bindIn(sgemm_node, 12, 12, 0);
  __hpvm__bindIn(sgemm_node, 13, 13, 0);
  __hpvm__bindIn(sgemm_node, 14, 14, 0);
  __hpvm__bindIn(sgemm_node, 15, 15, 0);
}

__attribute__((noinline)) void basicSgemm(char transa, char transb, int m,
                                          int n, int k, float alpha, float *A,
                                          size_t bytesA, int lda, float *B,
                                          size_t bytesB, int ldb, float beta,
                                          float *C, size_t bytesC, int ldc) {
  if ((transa != 'N') && (transa != 'n')) {
    std::cerr << "unsupported value of 'transa' in regtileSgemm()" << std::endl;
    return;
  }

  if ((transb != 'T') && (transb != 't')) {
    std::cerr << "unsupported value of 'transb' in regtileSgemm()" << std::endl;
    return;
  }

  // In this code we assume the matrix sizes are multiple of tile size
  if ((m % TILE_SZ) || (n % TILE_SZ)) {
    std::cerr << "unsupported size of matrix. m should be multiple of "
              << TILE_SZ << "; n should be multiple of " << TILE_SZ
              << std::endl;
  }

  size_t db[2] = {TILE_SZ, TILE_SZ},
         dg[2] = {m / TILE_SZ * db[0], n / TILE_SZ * db[1]};

  void *root_in = malloc(sizeof(RootIn));
  RootIn root_in_local = {A,
                          bytesA,
                          lda,
                          B,
                          bytesB,
                          ldb,
                          C,
                          bytesC,
                          ldc,
                          k,
                          alpha,
                          beta,
                          db[0],
                          db[1],
                          dg[0] / db[0],
                          dg[1] / db[1]};
  *(RootIn *)root_in = root_in_local;
  void *sgemmDFG = __hpvm__launch(0, basicSgemmLvl3, root_in);
  __hpvm__wait(sgemmDFG);
}

int main(int argc, char *argv[]) {

  struct pb_Parameters *params;
  struct pb_TimerSet timers;

  size_t A_sz, B_sz, C_sz;
  int matArow, matAcol;
  int matBrow, matBcol;
  std::vector<float> matA, matBT;

  /* Read command line. Expect 3 inputs: A, B and B^T
     in column-major layout*/
  params = pb_ReadParameters(&argc, argv);

  unsigned iter = 0;
  while (params->inpFiles[iter] != NULL) {
    printf("Found input file %d - %s\n", iter, params->inpFiles[iter]);
    iter++;
  }
  if ((params->inpFiles[0] == NULL) || (params->inpFiles[1] == NULL) ||
      (params->inpFiles[2] == NULL) || (params->inpFiles[3] != NULL)) {
    printf("Expecting three input filenames\n");
    exit(-1);
    return 0;
  }

  /* Read in data */
  // load A
  readColMajorMatrixFile(params->inpFiles[0], matArow, matAcol, matA);

  printf("This is in between two reads\n");
  // load B^T
  readColMajorMatrixFile(params->inpFiles[2], matBcol, matBrow, matBT);

  pb_InitializeTimerSet(&timers);
  __hpvm__init();

  pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);
  // copy A to device memory
  A_sz = matArow * matAcol * sizeof(float);
  B_sz = matBrow * matBcol * sizeof(float);

  // allocate space for C
  C_sz = matArow * matBcol * sizeof(float);

  // OpenCL memory allocation
  std::vector<float> matC(matArow * matBcol);

  llvm_hpvm_track_mem(&matA.front(), A_sz);
  llvm_hpvm_track_mem(&matBT.front(), B_sz);
  llvm_hpvm_track_mem(&matC.front(), C_sz);
  // Copy A and B^T into device memory
  pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);

  for (size_t i = 0; i < matC.size(); i++)
    matC[i] = 0.0f;

  pb_SwitchToTimer(&timers, pb_TimerID_NONE);

  // Use standard sgemm interface
  basicSgemm('N', 'T', matArow, matBcol, matAcol, 1.0f, &matA.front(), A_sz,
             matArow, &matBT.front(), B_sz, matBcol, 0.0f, &matC.front(), C_sz,
             matArow);

  pb_SwitchToTimer(&timers, pb_TimerID_COPY);
  llvm_hpvm_request_mem(&matC.front(), C_sz);

  pb_SwitchToTimer(&timers, hpvm_TimerID_MEM_UNTRACK);
  llvm_hpvm_untrack_mem(&matA.front());
  llvm_hpvm_untrack_mem(&matBT.front());
  llvm_hpvm_untrack_mem(&matC.front());
  pb_SwitchToTimer(&timers, pb_TimerID_NONE);

  pb_PrintTimerSet(&timers);
  __hpvm__cleanup();

  if (params->outFile) {

    /* Write C to file */
    // pb_SwitchToTimer(&timers, pb_TimerID_IO);
    writeColMajorMatrixFile(params->outFile, matArow, matBcol, matC);
  }

  double GPUtime = pb_GetElapsedTime(&(timers.timers[pb_TimerID_KERNEL]));
  std::cout << "GFLOPs = " << 2. * matArow * matBcol * matAcol / GPUtime / 1e9
            << std::endl;
  pb_FreeParameters(params);

  return 0;
}
