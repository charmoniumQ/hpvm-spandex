/***************************************************************************
 *cr
 *cr            (C) Copyright 2010 The Board of Trustees of the
 *cr                        University of Illinois
 *cr                         All Rights Reserved
 *cr
 ***************************************************************************/

/*############################################################################*/

#include <parboil.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <hpvm.h>

#include "lbm_macros.h"
#include "layout_config.h"
#include "lbm.h"
#include "main.h"

#define AS_UINT(x) (*((unsigned *)&(x)))

/*############################################################################*/

static LBM_Grid srcGrid, dstGrid;

/*############################################################################*/

struct pb_TimerSet timers;

/*############################################################################*/

void MAIN_parseCommandLine(int nArgs, char *arg[], MAIN_Param *param,
                           struct pb_Parameters *params) {
  struct stat fileStat;

  if (nArgs < 2) {
    printf("syntax: lbm <time steps>\n");
    exit(1);
  }

  param->nTimeSteps = atoi(arg[1]);

  if (params->inpFiles[0] != NULL) {
    param->obstacleFilename = params->inpFiles[0];

    if (stat(param->obstacleFilename, &fileStat) != 0) {
      printf("MAIN_parseCommandLine: cannot stat obstacle file '%s'\n",
             param->obstacleFilename);
      exit(1);
    }
    if (fileStat.st_size != SIZE_X * SIZE_Y * SIZE_Z + (SIZE_Y + 1) * SIZE_Z) {
      printf("MAIN_parseCommandLine:\n"
             "\tsize of file '%s' is %i bytes\n"
             "\texpected size is %i bytes\n",
             param->obstacleFilename, (int)fileStat.st_size,
             SIZE_X * SIZE_Y * SIZE_Z + (SIZE_Y + 1) * SIZE_Z);
      exit(1);
    }
  } else
    param->obstacleFilename = NULL;

  param->resultFilename = params->outFile;
}

/*############################################################################*/

void MAIN_printInfo(const MAIN_Param *param) {
  printf("MAIN_printInfo:\n"
         "\tgrid size      : %i x %i x %i = %.2f * 10^6 Cells\n"
         "\tnTimeSteps     : %i\n"
         "\tresult file    : %s\n"
         "\taction         : %s\n"
         "\tsimulation type: %s\n"
         "\tobstacle file  : %s\n\n",
         SIZE_X, SIZE_Y, SIZE_Z, 1e-6 * SIZE_X * SIZE_Y * SIZE_Z,
         param->nTimeSteps, param->resultFilename, "store", "lid-driven cavity",
         (param->obstacleFilename == NULL) ? "<none>"
                                           : param->obstacleFilename);
}

/*############################################################################*/

typedef struct __attribute__((__packed__)) {
  float *srcG;
  size_t bytes_srcG;
  float *dstG;
  size_t bytes_dstG;
  size_t dim_X1, dim_X2, dim_Y2;
} RootIn;

void performStreamCollide_kernel(float *srcG, size_t bytes_srcG, float *dstG,
                                 size_t bytes_dstG) {
  __hpvm__hint(hpvm::DEVICE);
  __hpvm__attributes(2, srcG, dstG, 1, dstG);

  void *thisNode = __hpvm__getNode();
  void *parentNode = __hpvm__getParentNode(thisNode);

  srcG += MARGIN;
  dstG += MARGIN;

  int lx = __hpvm__getNodeInstanceID_x(thisNode);
  int gx = __hpvm__getNodeInstanceID_x(parentNode);
  int gy = __hpvm__getNodeInstanceID_y(parentNode);

  // Using some predefined macros here.  Consider this the declaration
  //  and initialization of the variables SWEEP_X, SWEEP_Y and SWEEP_Z

  SWEEP_VAR
  SWEEP_X = lx; // get_local_id(0)
  SWEEP_Y = gx; // get_group_id(0)
  SWEEP_Z = gy; // get_group_id(1)

  float temp_swp, tempC, tempN, tempS, tempE, tempW, tempT, tempB;
  float tempNE, tempNW, tempSE, tempSW, tempNT, tempNB, tempST;
  float tempSB, tempET, tempEB, tempWT, tempWB;

  // Load all of the input fields
  // This is a gather operation of the SCATTER preprocessor variable
  // is undefined in layout_config.h, or a "local" read otherwise
  tempC = SRC_C(srcG);

  tempN = SRC_N(srcG);
  tempS = SRC_S(srcG);
  tempE = SRC_E(srcG);
  tempW = SRC_W(srcG);
  tempT = SRC_T(srcG);
  tempB = SRC_B(srcG);

  tempNE = SRC_NE(srcG);
  tempNW = SRC_NW(srcG);
  tempSE = SRC_SE(srcG);
  tempSW = SRC_SW(srcG);
  tempNT = SRC_NT(srcG);
  tempNB = SRC_NB(srcG);
  tempST = SRC_ST(srcG);
  tempSB = SRC_SB(srcG);
  tempET = SRC_ET(srcG);
  tempEB = SRC_EB(srcG);
  tempWT = SRC_WT(srcG);
  tempWB = SRC_WB(srcG);

  // Test whether the cell is fluid or obstacle
  if (AS_UINT(LOCAL(srcG, FLAGS)) & (OBSTACLE)) {

    // Swizzle the inputs: reflect any fluid coming into this cell
    // back to where it came from
    temp_swp = tempN;
    tempN = tempS;
    tempS = temp_swp;
    temp_swp = tempE;
    tempE = tempW;
    tempW = temp_swp;
    temp_swp = tempT;
    tempT = tempB;
    tempB = temp_swp;
    temp_swp = tempNE;
    tempNE = tempSW;
    tempSW = temp_swp;
    temp_swp = tempNW;
    tempNW = tempSE;
    tempSE = temp_swp;
    temp_swp = tempNT;
    tempNT = tempSB;
    tempSB = temp_swp;
    temp_swp = tempNB;
    tempNB = tempST;
    tempST = temp_swp;
    temp_swp = tempET;
    tempET = tempWB;
    tempWB = temp_swp;
    temp_swp = tempEB;
    tempEB = tempWT;
    tempWT = temp_swp;
  } else {

    // The math meat of LBM: ignore for optimization
    float ux, uy, uz, rho, u2;
    float temp1, temp2, temp_base;
    rho = tempC + tempN + tempS + tempE + tempW + tempT + tempB + tempNE +
          tempNW + tempSE + tempSW + tempNT + tempNB + tempST + tempSB +
          tempET + tempEB + tempWT + tempWB;

    ux = +tempE - tempW + tempNE - tempNW + tempSE - tempSW + tempET + tempEB -
         tempWT - tempWB;

    uy = +tempN - tempS + tempNE + tempNW - tempSE - tempSW + tempNT + tempNB -
         tempST - tempSB;

    uz = +tempT - tempB + tempNT - tempNB + tempST - tempSB + tempET - tempEB +
         tempWT - tempWB;

    ux /= rho;
    uy /= rho;
    uz /= rho;

    if (AS_UINT(LOCAL(srcG, FLAGS)) & (ACCEL)) {

      ux = 0.005f;
      uy = 0.002f;
      uz = 0.000f;
    }

    u2 = 1.5f * (ux * ux + uy * uy + uz * uz) - 1.0f;
    temp_base = OMEGA * rho;
    temp1 = DFL1 * temp_base;

    // Put the output values for this cell in the shared memory
    temp_base = OMEGA * rho;
    temp1 = DFL1 * temp_base;
    temp2 = 1.0f - OMEGA;
    tempC = temp2 * tempC + temp1 * (-u2);
    temp1 = DFL2 * temp_base;
    tempN = temp2 * tempN + temp1 * (uy * (4.5f * uy + 3.0f) - u2);
    tempS = temp2 * tempS + temp1 * (uy * (4.5f * uy - 3.0f) - u2);
    tempT = temp2 * tempT + temp1 * (uz * (4.5f * uz + 3.0f) - u2);
    tempB = temp2 * tempB + temp1 * (uz * (4.5f * uz - 3.0f) - u2);
    tempE = temp2 * tempE + temp1 * (ux * (4.5f * ux + 3.0f) - u2);
    tempW = temp2 * tempW + temp1 * (ux * (4.5f * ux - 3.0f) - u2);
    temp1 = DFL3 * temp_base;
    tempNT =
        temp2 * tempNT + temp1 * ((+uy + uz) * (4.5f * (+uy + uz) + 3.0f) - u2);
    tempNB =
        temp2 * tempNB + temp1 * ((+uy - uz) * (4.5f * (+uy - uz) + 3.0f) - u2);
    tempST =
        temp2 * tempST + temp1 * ((-uy + uz) * (4.5f * (-uy + uz) + 3.0f) - u2);
    tempSB =
        temp2 * tempSB + temp1 * ((-uy - uz) * (4.5f * (-uy - uz) + 3.0f) - u2);
    tempNE =
        temp2 * tempNE + temp1 * ((+ux + uy) * (4.5f * (+ux + uy) + 3.0f) - u2);
    tempSE =
        temp2 * tempSE + temp1 * ((+ux - uy) * (4.5f * (+ux - uy) + 3.0f) - u2);
    tempET =
        temp2 * tempET + temp1 * ((+ux + uz) * (4.5f * (+ux + uz) + 3.0f) - u2);
    tempEB =
        temp2 * tempEB + temp1 * ((+ux - uz) * (4.5f * (+ux - uz) + 3.0f) - u2);
    tempNW =
        temp2 * tempNW + temp1 * ((-ux + uy) * (4.5f * (-ux + uy) + 3.0f) - u2);
    tempSW =
        temp2 * tempSW + temp1 * ((-ux - uy) * (4.5f * (-ux - uy) + 3.0f) - u2);
    tempWT =
        temp2 * tempWT + temp1 * ((-ux + uz) * (4.5f * (-ux + uz) + 3.0f) - u2);
    tempWB =
        temp2 * tempWB + temp1 * ((-ux - uz) * (4.5f * (-ux - uz) + 3.0f) - u2);
  }

  // Write the results computed above
  // This is a scatter operation of the SCATTER preprocessor variable
  // is defined in layout_config.h, or a "local" write otherwise
  DST_C(dstG) = tempC;

  DST_N(dstG) = tempN;
  DST_S(dstG) = tempS;
  DST_E(dstG) = tempE;
  DST_W(dstG) = tempW;
  DST_T(dstG) = tempT;
  DST_B(dstG) = tempB;

  DST_NE(dstG) = tempNE;
  DST_NW(dstG) = tempNW;
  DST_SE(dstG) = tempSE;
  DST_SW(dstG) = tempSW;
  DST_NT(dstG) = tempNT;
  DST_NB(dstG) = tempNB;
  DST_ST(dstG) = tempST;
  DST_SB(dstG) = tempSB;
  DST_ET(dstG) = tempET;
  DST_EB(dstG) = tempEB;
  DST_WT(dstG) = tempWT;
  DST_WB(dstG) = tempWB;
}

void lbmLvl1(float *srcG, size_t bytes_srcG, float *dstG, size_t bytes_dstG,
             size_t dim_X1) {
  __hpvm__hint(hpvm::DEVICE);
  __hpvm__attributes(2, srcG, dstG, 1, dstG);
  void *lbm_node =
      __hpvm__createNodeND(2, performStreamCollide_kernel, dim_X1, (size_t)1);
  __hpvm__bindIn(lbm_node, 0, 0, 0);
  __hpvm__bindIn(lbm_node, 1, 1, 0);
  __hpvm__bindIn(lbm_node, 2, 2, 0);
  __hpvm__bindIn(lbm_node, 3, 3, 0);
}

void lbmLvl2(float *srcG, size_t bytes_srcG, float *dstG, size_t bytes_dstG,
             size_t dim_X1, size_t dim_X2, size_t dim_Y2) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(2, srcG, dstG, 1, dstG);
  void *lbm_node = __hpvm__createNodeND(2, lbmLvl1, dim_X2, dim_Y2);
  __hpvm__bindIn(lbm_node, 0, 0, 0);
  __hpvm__bindIn(lbm_node, 1, 1, 0);
  __hpvm__bindIn(lbm_node, 2, 2, 0);
  __hpvm__bindIn(lbm_node, 3, 3, 0);
  __hpvm__bindIn(lbm_node, 4, 4, 0);
}

void lbmLvl3(float *srcG, size_t bytes_srcG, float *dstG, size_t bytes_dstG,
             size_t dim_X1, size_t dim_X2, size_t dim_Y2) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(2, srcG, dstG, 1, dstG);
  void *lbm_node = __hpvm__createNodeND(0, lbmLvl2);
  __hpvm__bindIn(lbm_node, 0, 0, 0);
  __hpvm__bindIn(lbm_node, 1, 1, 0);
  __hpvm__bindIn(lbm_node, 2, 2, 0);
  __hpvm__bindIn(lbm_node, 3, 3, 0);
  __hpvm__bindIn(lbm_node, 4, 4, 0);
  __hpvm__bindIn(lbm_node, 5, 5, 0);
  __hpvm__bindIn(lbm_node, 6, 6, 0);
}

__attribute__((noinline)) void MAIN_performStreamCollide(LBM_Grid src,
                                                         LBM_Grid dst) {

  long dimBlock[3] = {SIZE_X, 1, 1};
  long dimGrid[3] = {SIZE_X * SIZE_Y, SIZE_Z, 1};
  size_t size = TOTAL_PADDED_CELLS * N_CELL_ENTRIES * sizeof(float);

  void *root_in = malloc(sizeof(RootIn));
  RootIn root_in_local = {src - MARGIN, size,   dst - MARGIN, size,
                          SIZE_X,       SIZE_Y, SIZE_Z};
  *(RootIn *)root_in = root_in_local;
  void *lbmDFG = __hpvm__launch(0, lbmLvl3, root_in);

  __hpvm__wait(lbmDFG);
}

void MAIN_initialize(const MAIN_Param *param) {

  pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);
  // Setup datastructures
  LBM_allocateGrid((float **)&srcGrid);
  LBM_allocateGrid((float **)&dstGrid);
  LBM_initializeGrid(srcGrid);
  LBM_initializeGrid(dstGrid);

  pb_SwitchToTimer(&timers, pb_TimerID_IO);
  if (param->obstacleFilename != NULL) {
    LBM_loadObstacleFile(srcGrid, param->obstacleFilename);
    LBM_loadObstacleFile(dstGrid, param->obstacleFilename);
  }

  pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);
  LBM_initializeSpecialCellsForLDC(srcGrid);
  LBM_initializeSpecialCellsForLDC(dstGrid);

  LBM_showGridStatistics(srcGrid);

  // LBM_freeGrid( (float**) &srcGrid );
  // LBM_freeGrid( (float**) &dstGrid );
}

/*############################################################################*/

void MAIN_finalize(const MAIN_Param *param) {

  // Setup TEMP datastructures

  LBM_showGridStatistics(srcGrid);

  LBM_storeVelocityField(srcGrid, param->resultFilename, TRUE);

  LBM_freeGrid((float **)&srcGrid);
  LBM_freeGrid((float **)&dstGrid);
}

int main(int nArgs, char *arg[]) {
  MAIN_Param param;
  int t;

  struct pb_Parameters *params;
  params = pb_ReadParameters(&nArgs, arg);

  // Setup TEMP datastructures
  MAIN_parseCommandLine(nArgs, arg, &param, params);
  MAIN_printInfo(&param);

  MAIN_initialize(&param);

  pb_InitializeTimerSet(&timers);
  __hpvm__init();

  size_t size = TOTAL_PADDED_CELLS * N_CELL_ENTRIES * sizeof(float);
  pb_SwitchToTimer(&timers, hpvm_TimerID_MEM_TRACK);
  llvm_hpvm_track_mem(srcGrid - MARGIN, size);
  llvm_hpvm_track_mem(dstGrid - MARGIN, size);

  pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);
  for (t = 1; t <= param.nTimeSteps; t++) {
    pb_SwitchToTimer(&timers, pb_TimerID_NONE);
    MAIN_performStreamCollide(srcGrid, dstGrid);

    pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);
    LBM_swapGrids(&srcGrid, &dstGrid);

    /*if( (t & 63) == 0 ) {*/
    /*printf( "timestep: %i\n", t );*/
#if 0
            CUDA_LBM_getDeviceGrid((float**)&CUDA_srcGrid, (float**)&TEMP_srcGrid);
            LBM_showGridStatistics( *TEMP_srcGrid );
#endif
    /*}*/
  }

  pb_SwitchToTimer(&timers, pb_TimerID_COPY);
  llvm_hpvm_request_mem(srcGrid - MARGIN, size);

  pb_SwitchToTimer(&timers, hpvm_TimerID_MEM_UNTRACK);
  llvm_hpvm_untrack_mem(srcGrid - MARGIN);
  llvm_hpvm_untrack_mem(dstGrid - MARGIN);

  pb_SwitchToTimer(&timers, pb_TimerID_NONE);
  pb_PrintTimerSet(&timers);
  __hpvm__cleanup();

  /*pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);*/
  MAIN_finalize(&param);

  /*pb_SwitchToTimer(&timers, pb_TimerID_COMPUTE);*/

  pb_FreeParameters(params);
  return 0;
}
