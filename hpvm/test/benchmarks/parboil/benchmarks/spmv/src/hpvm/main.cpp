/***************************************************************************
 *cr
 *cr            (C) Copyright 2010 The Board of Trustees of the
 *cr                        University of Illinois
 *cr                         All Rights Reserved
 *cr
 ***************************************************************************/

//#include <CL/cl.h>
//#include <CL/cl_ext.h>
#include <hpvm.h>
#include <parboil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "convert_dataset.h"
#include "file.h"
#include "gpu_info.h"

#define WARP_BITS 5

static int generate_vector(float *x_vector, int dim) {
  srand(54321);
  int i;
  for (i = 0; i < dim; i++) {
    x_vector[i] = (rand() / (float)RAND_MAX);
  }
  return 0;
}

typedef struct __attribute__((__packed__)) {
  float *dst_vector;
  size_t bytes_dst_vector;
  float *d_data;
  size_t bytes_d_data;
  int *d_index;
  size_t bytes_d_index;
  int *d_perm;
  size_t bytes_d_perm;
  float *x_vec;
  size_t bytes_x_vec;
  int dim;
  int *jds_ptr_int;
  size_t bytes_jds_ptr_int;
  int *sh_zcnt_int;
  size_t bytes_sh_zcnt_int;
  size_t dim_X1, dim_X2;
} RootIn;

void spmv_jds(float *dst_vector, size_t bytes_dst_vector, float *d_data,
              size_t bytes_d_data, int *d_index, size_t bytes_d_index,
              int *d_perm, size_t bytes_d_perm, float *x_vec,
              size_t bytes_x_vec, int dim, int *jds_ptr_int,
              size_t bytes_jds_ptr_int, int *sh_zcnt_int,
              size_t bytes_sh_zcnt_int) {
  __hpvm__hint(hpvm::DEVICE);
  __hpvm__attributes(7, dst_vector, d_data, d_index, d_perm, x_vec, jds_ptr_int,
                     sh_zcnt_int, 1, dst_vector);

  void *thisNode = __hpvm__getNode();
  void *parentNode = __hpvm__getParentNode(thisNode);
  int lx = __hpvm__getNodeInstanceID_x(thisNode);
  int gx = __hpvm__getNodeInstanceID_x(parentNode);
  int gridx = __hpvm__getNumNodeInstances_x(thisNode);

  int ix = gx * gridx + lx;
  int warp_id = ix >> WARP_BITS;

  if (ix < dim) {
    float sum = 0.0f;
    int bound = sh_zcnt_int[warp_id];
    // prefetch 0
    int j = jds_ptr_int[0] + ix;
    float d = d_data[j];
    int i = d_index[j];
    float t = x_vec[i];

    if (bound > 1) // bound >=2
    {
      // prefetch 1
      j = jds_ptr_int[1] + ix;
      i = d_index[j];
      int in;
      float dn;
      float tn;
      for (int k = 2; k < bound; k++) {
        // prefetch k-1
        dn = d_data[j];
        // prefetch k
        j = jds_ptr_int[k] + ix;
        in = d_index[j];
        // prefetch k-1
        tn = x_vec[i];

        // compute k-2
        sum += d * t;
        // sweep to k
        i = in;
        // sweep to k-1
        d = dn;
        t = tn;
      }

      // fetch last
      dn = d_data[j];
      tn = x_vec[i];

      // compute last-1
      sum += d * t;
      // sweep to last
      d = dn;
      t = tn;
    }
    // compute last
    sum += d * t; // 3 3

    // write out data
    dst_vector[d_perm[ix]] = sum;
  }
}

void spmvLvl1(float *dst_vector, size_t bytes_dst_vector, float *d_data,
              size_t bytes_d_data, int *d_index, size_t bytes_d_index,
              int *d_perm, size_t bytes_d_perm, float *x_vec,
              size_t bytes_x_vec, int dim, int *jds_ptr_int,
              size_t bytes_jds_ptr_int, int *sh_zcnt_int,
              size_t bytes_sh_zcnt_int, size_t dim_X1) {
  __hpvm__hint(hpvm::DEVICE);
  __hpvm__attributes(7, dst_vector, d_data, d_index, d_perm, x_vec, jds_ptr_int,
                     sh_zcnt_int, 1, dst_vector);
  void *spmv_node = __hpvm__createNodeND(1, spmv_jds, dim_X1);
  __hpvm__bindIn(spmv_node, 0, 0, 0);
  __hpvm__bindIn(spmv_node, 1, 1, 0);
  __hpvm__bindIn(spmv_node, 2, 2, 0);
  __hpvm__bindIn(spmv_node, 3, 3, 0);
  __hpvm__bindIn(spmv_node, 4, 4, 0);
  __hpvm__bindIn(spmv_node, 5, 5, 0);
  __hpvm__bindIn(spmv_node, 6, 6, 0);
  __hpvm__bindIn(spmv_node, 7, 7, 0);
  __hpvm__bindIn(spmv_node, 8, 8, 0);
  __hpvm__bindIn(spmv_node, 9, 9, 0);
  __hpvm__bindIn(spmv_node, 10, 10, 0);
  __hpvm__bindIn(spmv_node, 11, 11, 0);
  __hpvm__bindIn(spmv_node, 12, 12, 0);
  __hpvm__bindIn(spmv_node, 13, 13, 0);
  __hpvm__bindIn(spmv_node, 14, 14, 0);
}

void spmvLvl2(float *dst_vector, size_t bytes_dst_vector, float *d_data,
              size_t bytes_d_data, int *d_index, size_t bytes_d_index,
              int *d_perm, size_t bytes_d_perm, float *x_vec,
              size_t bytes_x_vec, int dim, int *jds_ptr_int,
              size_t bytes_jds_ptr_int, int *sh_zcnt_int,
              size_t bytes_sh_zcnt_int, size_t dim_X1, size_t dim_X2) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(7, dst_vector, d_data, d_index, d_perm, x_vec, jds_ptr_int,
                     sh_zcnt_int, 1, dst_vector);
  void *spmv_node = __hpvm__createNodeND(1, spmvLvl1, dim_X2);
  __hpvm__bindIn(spmv_node, 0, 0, 0);
  __hpvm__bindIn(spmv_node, 1, 1, 0);
  __hpvm__bindIn(spmv_node, 2, 2, 0);
  __hpvm__bindIn(spmv_node, 3, 3, 0);
  __hpvm__bindIn(spmv_node, 4, 4, 0);
  __hpvm__bindIn(spmv_node, 5, 5, 0);
  __hpvm__bindIn(spmv_node, 6, 6, 0);
  __hpvm__bindIn(spmv_node, 7, 7, 0);
  __hpvm__bindIn(spmv_node, 8, 8, 0);
  __hpvm__bindIn(spmv_node, 9, 9, 0);
  __hpvm__bindIn(spmv_node, 10, 10, 0);
  __hpvm__bindIn(spmv_node, 11, 11, 0);
  __hpvm__bindIn(spmv_node, 12, 12, 0);
  __hpvm__bindIn(spmv_node, 13, 13, 0);
  __hpvm__bindIn(spmv_node, 14, 14, 0);
  __hpvm__bindIn(spmv_node, 15, 15, 0);
}

void spmvLvl3(float *dst_vector, size_t bytes_dst_vector, float *d_data,
              size_t bytes_d_data, int *d_index, size_t bytes_d_index,
              int *d_perm, size_t bytes_d_perm, float *x_vec,
              size_t bytes_x_vec, int dim, int *jds_ptr_int,
              size_t bytes_jds_ptr_int, int *sh_zcnt_int,
              size_t bytes_sh_zcnt_int, size_t dim_X1, size_t dim_X2) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(7, dst_vector, d_data, d_index, d_perm, x_vec, jds_ptr_int,
                     sh_zcnt_int, 1, dst_vector);
  void *spmv_node = __hpvm__createNodeND(1, spmvLvl2, dim_X2);
  __hpvm__bindIn(spmv_node, 0, 0, 0);
  __hpvm__bindIn(spmv_node, 1, 1, 0);
  __hpvm__bindIn(spmv_node, 2, 2, 0);
  __hpvm__bindIn(spmv_node, 3, 3, 0);
  __hpvm__bindIn(spmv_node, 4, 4, 0);
  __hpvm__bindIn(spmv_node, 5, 5, 0);
  __hpvm__bindIn(spmv_node, 6, 6, 0);
  __hpvm__bindIn(spmv_node, 7, 7, 0);
  __hpvm__bindIn(spmv_node, 8, 8, 0);
  __hpvm__bindIn(spmv_node, 9, 9, 0);
  __hpvm__bindIn(spmv_node, 10, 10, 0);
  __hpvm__bindIn(spmv_node, 11, 11, 0);
  __hpvm__bindIn(spmv_node, 12, 12, 0);
  __hpvm__bindIn(spmv_node, 13, 13, 0);
  __hpvm__bindIn(spmv_node, 14, 14, 0);
  __hpvm__bindIn(spmv_node, 15, 15, 0);
  __hpvm__bindIn(spmv_node, 16, 16, 0);
}

int main(int argc, char **argv) {
  struct pb_TimerSet timers;
  struct pb_Parameters *parameters;

  printf("OpenCL accelerated sparse matrix vector multiplication****\n");
  printf("Li-Wen Chang <lchang20@illinois.edu> and Shengzhao "
         "Wu<wu14@illinois.edu>\n");
  parameters = pb_ReadParameters(&argc, argv);

  if ((parameters->inpFiles[0] == NULL) || (parameters->inpFiles[1] == NULL)) {
    fprintf(stderr, "Expecting one two filenames\n");
    exit(-1);
  }

  /*pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);*/

  // parameters declaration
  int len;
  int depth;
  int dim;
  int pad = 32;
  int nzcnt_len;

  // host memory allocation
  // matrix
  float *h_data;
  int *h_indices;
  int *h_ptr;
  int *h_perm;
  int *h_nzcnt;

  // vector
  float *h_Ax_vector;
  float *h_x_vector;

  // load matrix from files
  /*pb_SwitchToTimer(&timers, pb_TimerID_IO);*/
  // inputData(parameters->inpFiles[0], &len, &depth, &dim,&nzcnt_len,&pad,
  //    &h_data, &h_indices, &h_ptr,
  //    &h_perm, &h_nzcnt);
  int col_count;

  coo_to_jds(parameters->inpFiles[0], // bcsstk32.mtx, fidapm05.mtx, jgl009.mtx
             1,                       // row padding
             pad,                     // warp size
             1,                       // pack size
             1,                       // is mirrored?
             0,                       // binary matrix
             1,                       // debug level [0:2]
             &h_data, &h_ptr, &h_nzcnt, &h_indices, &h_perm, &col_count, &dim,
             &len, &nzcnt_len, &depth);

  h_Ax_vector = (float *)malloc(sizeof(float) * dim);
  h_x_vector = (float *)malloc(sizeof(float) * dim);
  input_vec(parameters->inpFiles[1], h_x_vector, dim);

  pb_InitializeTimerSet(&timers);
  __hpvm__init();

  pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);
  memset(h_Ax_vector, 0, dim * sizeof(float));

  size_t grid;
  size_t block;

  compute_active_thread(&block, &grid, nzcnt_len, pad, 3, 0, 8);

  pb_SwitchToTimer(&timers, hpvm_TimerID_MEM_TRACK);
  llvm_hpvm_track_mem(h_Ax_vector, dim * sizeof(float));
  llvm_hpvm_track_mem(h_data, len * sizeof(float));
  llvm_hpvm_track_mem(h_indices, len * sizeof(int));
  llvm_hpvm_track_mem(h_perm, dim * sizeof(int));
  llvm_hpvm_track_mem(h_x_vector, dim * sizeof(float));
  llvm_hpvm_track_mem(h_ptr, depth * sizeof(int));
  llvm_hpvm_track_mem(h_nzcnt, nzcnt_len * sizeof(int));

  // main execution
  pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);

  int i;
  for (i = 0; i < 50; i++) {
    pb_SwitchToTimer(&timers, pb_TimerID_NONE);

    void *root_in = malloc(sizeof(RootIn));
    RootIn root_in_local = {h_Ax_vector,
                            dim * sizeof(float),
                            h_data,
                            len * sizeof(float),
                            h_indices,
                            len * sizeof(int),
                            h_perm,
                            dim * sizeof(int),
                            h_x_vector,
                            dim * sizeof(float),
                            dim,
                            h_ptr,
                            depth * sizeof(int),
                            h_nzcnt,
                            nzcnt_len * sizeof(int),
                            block,
                            (grid / block)};
    *(RootIn *)root_in = root_in_local;
    void *spmvDFG = __hpvm__launch(0, spmvLvl3, root_in);

    __hpvm__wait(spmvDFG);
    pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);

    /******************************* Issues *******************************
     * 1. Using OpenCL to compute grid and block dimensions
     *    (getting device info)
     *    We need to check the GPU version (major number) where this kernel
     *    executes to compare against opencl_nvidia version
     * 2. Type of cl_mem buffer for d_x_vector is created with size of float,
          but copied in through size of int.
          Due to type of h_x_vector, I chose to use float
     *    (Minor)
     * 3. Kernel initially used constant memory for last two arguments - removed
     */
  }

  // HtoD memory copy
  pb_SwitchToTimer(&timers, pb_TimerID_COPY);
  llvm_hpvm_request_mem(h_Ax_vector, dim * sizeof(float));

  pb_SwitchToTimer(&timers, hpvm_TimerID_MEM_UNTRACK);

  llvm_hpvm_untrack_mem(h_Ax_vector);
  llvm_hpvm_untrack_mem(h_data);
  llvm_hpvm_untrack_mem(h_indices);
  llvm_hpvm_untrack_mem(h_perm);
  llvm_hpvm_untrack_mem(h_x_vector);
  llvm_hpvm_untrack_mem(h_ptr);
  llvm_hpvm_untrack_mem(h_nzcnt);
  pb_SwitchToTimer(&timers, pb_TimerID_NONE);

  pb_PrintTimerSet(&timers);
  __hpvm__cleanup();

  if (parameters->outFile) {
    /*pb_SwitchToTimer(&timers, pb_TimerID_IO);*/
    outputData(parameters->outFile, h_Ax_vector, dim);
  }

  free(h_data);
  free(h_indices);
  free(h_ptr);
  free(h_perm);
  free(h_nzcnt);
  free(h_Ax_vector);
  free(h_x_vector);

  pb_FreeParameters(parameters);

  return 0;
}
