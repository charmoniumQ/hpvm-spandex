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

#include "opencv2/ocl/ocl.hpp"
#include "opencv2/opencv.hpp"
#include <cassert>
#include <hpvm.h>
#include <iostream>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define NUM_RUNS 100
#define DEPTH 3
#define HEIGHT 640
#define WIDTH 480

std::string input_window = "GPU Pipeline - Input Video";
std::string output_window = "GPU Pipeline - Edge Mapping";

#ifdef MIDDLE
#define POSX_IN 640
#define POSY_IN 0
#define POSX_OUT 640
#define POSY_OUT 540

#elif RIGHT
#define POSX_IN 1280
#define POSY_IN 0
#define POSX_OUT 1280
#define POSY_OUT 540

#else // LEFT
#define POSX_IN 0
#define POSY_IN 0
#define POSX_OUT 0
#define POSY_OUT 540
#endif

//#define NUM_FRAMES 20

// Definitions of sizes for edge detection kernels

#define MIN_BR 0.0f
#define MAX_BR 1.0f

// Code needs to be changed for this to vary
#define SZB 3

#define REDUCTION_TILE_SZ 1024

#define _MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define _MAX(X, Y) ((X) > (Y) ? (X) : (Y))

extern "C" {

struct __attribute__((__packed__)) OutStruct {
  size_t ret;
};

struct __attribute__((__packed__)) InStruct {
  float *I;
  size_t bytesI;
  float *Is;
  size_t bytesIs;
  float *L;
  size_t bytesL;
  float *S;
  size_t bytesS;
  float *G;
  size_t bytesG;
  float *maxG;
  size_t bytesMaxG;
  float *E;
  size_t bytesE;
  float *Gs;
  size_t bytesGs;
  float *B;
  size_t bytesB;
  float *Sx;
  size_t bytesSx;
  float *Sy;
  size_t bytesSy;
  long m;
  long n;
  long block_x;
  long grid_x;
};

void packData(struct InStruct *args, float *I, size_t bytesI, float *Is,
              size_t bytesIs, float *L, size_t bytesL, float *S, size_t bytesS,
              float *G, size_t bytesG, float *maxG, size_t bytesMaxG, float *E,
              size_t bytesE, float *Gs, size_t bytesGs, float *B, size_t bytesB,
              float *Sx, size_t bytesSx, float *Sy, size_t bytesSy, long m,
              long n, long block_x, long grid_x) {
  args->I = I;
  args->bytesI = bytesI;
  args->Is = Is;
  args->bytesIs = bytesIs;
  args->L = L;
  args->bytesL = bytesL;
  args->S = S;
  args->bytesS = bytesS;
  args->G = G;
  args->bytesG = bytesG;
  args->maxG = maxG;
  args->bytesMaxG = bytesMaxG;
  args->E = E;
  args->bytesE = bytesE;
  args->Gs = Gs;
  args->bytesGs = bytesGs;
  args->B = B;
  args->bytesB = bytesB;
  args->Sx = Sx;
  args->bytesSx = bytesSx;
  args->Sy = Sy;
  args->bytesSy = bytesSy;
  args->m = m;
  args->n = n;
  args->block_x = block_x;
  args->grid_x = grid_x;
}

/*
 * Gaussian smoothing of image I of size m x n
 * I : input image
 * Gs : gaussian filter
 * Is: output (smoothed image)
 * m, n : dimensions
 *
 * Need 2D grid, a thread per pixel
 * No use of separable algorithm because we need to do this in one kernel
 * No use of shared memory because
 * - we don't handle it in the CPU pass
 */

#define GAUSSIAN_SIZE 7
#define GAUSSIAN_RADIUS (GAUSSIAN_SIZE / 2)
void gaussianSmoothing(float *I, size_t bytesI, float *Gs, size_t bytesGs,
                       float *Is, size_t bytesIs, long m, long n) {

  __hpvm__hint(hpvm::DEVICE);
  __hpvm__attributes(2, I, Gs, 1, Is);

  void *thisNode = __hpvm__getNode();
  long gx = __hpvm__getNodeInstanceID_x(thisNode);
  long gy = __hpvm__getNodeInstanceID_y(thisNode);

  int gloc = gx + gy * n;

  float smoothedVal = 0;
  float gval;
  int loadOffset;

  if ((gx < n) && (gy < m)) {
    for (int i = -GAUSSIAN_RADIUS; i <= GAUSSIAN_RADIUS; i++)
      for (int j = -GAUSSIAN_RADIUS; j <= GAUSSIAN_RADIUS; j++) {

        loadOffset = gloc + i * n + j;

        if ((gy + i) < 0) // top contour
          loadOffset = gx + j;
        else if ((gy + i) > m - 1) // bottom contour
          loadOffset = (m - 1) * n + gx + j;
        else
          loadOffset = gloc + i * n + j; // within image vertically

        // Adjust so we are within image horizonally
        if ((gx + j) < 0) // left contour
          loadOffset -= (gx + j);
        else if ((gx + j) > n - 1) // right contour
          loadOffset = loadOffset - gx - j + n - 1;

        gval = I[loadOffset];
        smoothedVal +=
            gval *
            Gs[(GAUSSIAN_RADIUS + i) * GAUSSIAN_SIZE + GAUSSIAN_RADIUS + j];
      }

    Is[gloc] = smoothedVal;
  }
  __hpvm__return(2, bytesIs, bytesIs);
}

void WrapperGaussianSmoothing(float *I, size_t bytesI, float *Gs,
                              size_t bytesGs, float *Is, size_t bytesIs, long m,
                              long n) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(2, I, Gs, 1, Is);
  void *GSNode = __hpvm__createNodeND(2, gaussianSmoothing, m, n);
  __hpvm__bindIn(GSNode, 0, 0, 0); // Bind I
  __hpvm__bindIn(GSNode, 1, 1, 0); // Bind bytesI
  __hpvm__bindIn(GSNode, 2, 2, 0); // Bind Gs
  __hpvm__bindIn(GSNode, 3, 3, 0); // Bind bytesGs
  __hpvm__bindIn(GSNode, 4, 4, 0); // Bind Is
  __hpvm__bindIn(GSNode, 5, 5, 0); // Bind bytesIs
  __hpvm__bindIn(GSNode, 6, 6, 0); // Bind m
  __hpvm__bindIn(GSNode, 7, 7, 0); // Bind n

  __hpvm__bindOut(GSNode, 0, 0, 0); // bind output bytesIs
  __hpvm__bindOut(GSNode, 1, 1, 0); // bind output bytesIs
}

/* Compute a non-linear laplacian estimate of input image I of size m x n */
/*
 * Is   : blurred imput image
 * m, n : dimensions
 * B    : structural element for dilation - erosion ([0 1 0; 1 1 1; 0 1 0])
 * L    : output (laplacian of the image)
 * Need 2D grid, a thread per pixel
 */
void laplacianEstimate(float *Is, size_t bytesIs, float *B, size_t bytesB,
                       float *L, size_t bytesL, long m, long n) {

  __hpvm__hint(hpvm::DEVICE);
  __hpvm__attributes(2, Is, B, 1, L);
  // 3x3 image area
  float imageArea[SZB * SZB];

  void *thisNode = __hpvm__getNode();
  long gx = __hpvm__getNodeInstanceID_x(thisNode);
  long gy = __hpvm__getNodeInstanceID_y(thisNode);
  int i, j;

  if ((gx < n) && (gy < m)) {
    // Data copy for dilation filter
    imageArea[1 * SZB + 1] = Is[gy * n + gx];

    if (gx == 0) {
      imageArea[0 * SZB + 0] = imageArea[1 * SZB + 0] = imageArea[2 * SZB + 0] =
          MIN_BR;
    } else {
      imageArea[1 * SZB + 0] = Is[gy * n + gx - 1];
      imageArea[0 * SZB + 0] = (gy > 0) ? Is[(gy - 1) * n + gx - 1] : MIN_BR;
      imageArea[2 * SZB + 0] =
          (gy < m - 1) ? Is[(gy + 1) * n + gx - 1] : MIN_BR;
    }

    if (gx == n - 1) {
      imageArea[0 * SZB + 2] = imageArea[1 * SZB + 2] = imageArea[2 * SZB + 2] =
          MIN_BR;
    } else {
      imageArea[1 * SZB + 2] = Is[gy * n + gx + 1];
      imageArea[0 * SZB + 2] = (gy > 0) ? Is[(gy - 1) * n + gx + 1] : MIN_BR;
      imageArea[2 * SZB + 2] =
          (gy < m - 1) ? Is[(gy + 1) * n + gx + 1] : MIN_BR;
    }

    imageArea[0 * SZB + 1] = (gy > 0) ? Is[(gy - 1) * n + gx] : MIN_BR;
    imageArea[2 * SZB + 1] = (gy < m - 1) ? Is[(gy + 1) * n + gx] : MIN_BR;

    // Compute pixel of dilated image
    float dilatedPixel = MIN_BR;
    for (i = 0; i < SZB; i++)
      for (j = 0; j < SZB; j++)
        dilatedPixel =
            _MAX(dilatedPixel, imageArea[i * SZB + j] * B[i * SZB + j]);

    // Data copy for erotion filter - only change the boundary conditions
    if (gx == 0) {
      imageArea[0 * SZB + 0] = imageArea[1 * SZB + 0] = imageArea[2 * SZB + 0] =
          MAX_BR;
    } else {
      if (gy == 0)
        imageArea[0 * SZB + 0] = MAX_BR;
      if (gy == m - 1)
        imageArea[2 * SZB + 0] = MAX_BR;
    }

    if (gx == n - 1) {
      imageArea[0 * SZB + 2] = imageArea[1 * SZB + 2] = imageArea[2 * SZB + 2] =
          MAX_BR;
    } else {
      if (gy == 0)
        imageArea[0 * SZB + 2] = MAX_BR;
      if (gy == m - 1)
        imageArea[2 * SZB + 2] = MAX_BR;
    }

    if (gy == 0)
      imageArea[0 * SZB + 1] = MAX_BR;
    if (gy == m - 1)
      imageArea[2 * SZB + 1] = MAX_BR;

    // Compute pixel of eroded image
    float erodedPixel = MAX_BR;
    for (i = 0; i < SZB; i++)
      for (j = 0; j < SZB; j++)
        erodedPixel =
            _MIN(erodedPixel, imageArea[i * SZB + j] * B[i * SZB + j]);

    float laplacian = dilatedPixel + erodedPixel - 2 * imageArea[1 * SZB + 1];
    L[gy * n + gx] = laplacian;
  }
  __hpvm__return(1, bytesL);
}

void WrapperlaplacianEstimate(float *Is, size_t bytesIs, float *B,
                              size_t bytesB, float *L, size_t bytesL, long m,
                              long n) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(2, Is, B, 1, L);
  void *LNode = __hpvm__createNodeND(2, laplacianEstimate, m, n);
  __hpvm__bindIn(LNode, 0, 0, 0); // Bind Is
  __hpvm__bindIn(LNode, 1, 1, 0); // Bind bytesIs
  __hpvm__bindIn(LNode, 2, 2, 0); // Bind B
  __hpvm__bindIn(LNode, 3, 3, 0); // Bind bytesB
  __hpvm__bindIn(LNode, 4, 4, 0); // Bind L
  __hpvm__bindIn(LNode, 5, 5, 0); // Bind bytesL
  __hpvm__bindIn(LNode, 6, 6, 0); // Bind m
  __hpvm__bindIn(LNode, 7, 7, 0); // Bind n

  __hpvm__bindOut(LNode, 0, 0, 0); // bind output bytesL
}

/* Compute the zero crossings of input image L of size m x n */
/*
 * L    : imput image (computed Laplacian)
 * m, n : dimensions
 * B    : structural element for dilation - erosion ([0 1 0; 1 1 1; 0 1 0])
 * S    : output (sign of the image)
 * Need 2D grid, a thread per pixel
 */
void computeZeroCrossings(float *L, size_t bytesL, float *B, size_t bytesB,
                          float *S, size_t bytesS, long m, long n) {
  __hpvm__hint(hpvm::DEVICE);
  //__hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(2, L, B, 1, S);

  // 3x3 image area
  float imageArea[SZB][SZB];

  void *thisNode = __hpvm__getNode();
  long gx = __hpvm__getNodeInstanceID_x(thisNode);
  long gy = __hpvm__getNodeInstanceID_y(thisNode);
  int i, j;

  if ((gx < n) && (gy < m)) {
    // Data copy for dilation filter
    imageArea[1][1] = L[gy * n + gx] > MIN_BR ? MAX_BR : MIN_BR;

    if (gx == 0) { // left most line
      imageArea[0][0] = imageArea[1][0] = imageArea[2][0] = MIN_BR;
    } else {
      imageArea[1][0] = L[gy * n + gx - 1] > MIN_BR ? MAX_BR : MIN_BR;
      imageArea[0][0] =
          (gy > 0) ? (L[(gy - 1) * n + gx - 1] > MIN_BR ? MAX_BR : MIN_BR)
                   : MIN_BR;
      imageArea[2][0] =
          (gy < m - 1) ? (L[(gy + 1) * n + gx - 1] > MIN_BR ? MAX_BR : MIN_BR)
                       : MIN_BR;
    }

    if (gx == n - 1) {
      imageArea[0][2] = imageArea[1][2] = imageArea[2][2] = MIN_BR;
    } else {
      imageArea[1][2] = L[gy * n + gx + 1] > MIN_BR ? MAX_BR : MIN_BR;
      imageArea[0][2] =
          (gy > 0) ? (L[(gy - 1) * n + gx + 1] > MIN_BR ? MAX_BR : MIN_BR)
                   : MIN_BR;
      imageArea[2][2] =
          (gy < m - 1) ? (L[(gy + 1) * n + gx + 1] > MIN_BR ? MAX_BR : MIN_BR)
                       : MIN_BR;
    }

    imageArea[0][1] =
        (gy > 0) ? (L[(gy - 1) * n + gx] > MIN_BR ? MAX_BR : MIN_BR) : MIN_BR;
    imageArea[2][1] = (gy < m - 1)
                          ? (L[(gy + 1) * n + gx] > MIN_BR ? MAX_BR : MIN_BR)
                          : MIN_BR;

    // Compute pixel of dilated image
    float dilatedPixel = MIN_BR;
    for (i = 0; i < SZB; i++)
      for (j = 0; j < SZB; j++)
        dilatedPixel = _MAX(dilatedPixel, imageArea[i][j] * B[i * SZB + j]);

    // Data copy for erotion filter - only change the boundary conditions
    if (gx == 0) {
      imageArea[0][0] = imageArea[1][0] = imageArea[2][0] = MAX_BR;
    } else {
      if (gy == 0)
        imageArea[0][0] = MAX_BR;
      if (gy == m - 1)
        imageArea[2][0] = MAX_BR;
    }

    if (gx == n - 1) {
      imageArea[0][2] = imageArea[1][2] = imageArea[2][2] = MAX_BR;
    } else {
      if (gy == 0)
        imageArea[0][2] = MAX_BR;
      if (gy == m - 1)
        imageArea[2][2] = MAX_BR;
    }

    if (gy == 0)
      imageArea[0][1] = MAX_BR;
    if (gy == m - 1)
      imageArea[2][1] = MAX_BR;

    // Compute pixel of eroded image
    float erodedPixel = MAX_BR;
    for (i = 0; i < SZB; i++)
      for (j = 0; j < SZB; j++)
        erodedPixel = _MIN(erodedPixel, imageArea[i][j] * B[i * SZB + j]);

    float pixelSign = dilatedPixel - erodedPixel;
    S[gy * n + gx] = pixelSign;
  }
  __hpvm__return(1, bytesS);
}

void WrapperComputeZeroCrossings(float *L, size_t bytesL, float *B,
                                 size_t bytesB, float *S, size_t bytesS, long m,
                                 long n) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(2, L, B, 1, S);
  void *ZCNode = __hpvm__createNodeND(2, computeZeroCrossings, m, n);
  __hpvm__bindIn(ZCNode, 0, 0, 0); // Bind L
  __hpvm__bindIn(ZCNode, 1, 1, 0); // Bind bytesL
  __hpvm__bindIn(ZCNode, 2, 2, 0); // Bind B
  __hpvm__bindIn(ZCNode, 3, 3, 0); // Bind bytesB
  __hpvm__bindIn(ZCNode, 4, 4, 0); // Bind S
  __hpvm__bindIn(ZCNode, 5, 5, 0); // Bind bytesS
  __hpvm__bindIn(ZCNode, 6, 6, 0); // Bind m
  __hpvm__bindIn(ZCNode, 7, 7, 0); // Bind n

  __hpvm__bindOut(ZCNode, 0, 0, 0); // bind output bytesS
}

/*
 * Gradient computation using Sobel filters
 * Is   : input (smoothed image)
 * Sx, Sy: Sobel operators
 * - Sx = [-1  0  1 ; -2 0 2 ; -1 0 1 ]
 * - Sy = [-1 -2 -1 ;  0 0 0 ;  1 2 1 ]
 * m, n : dimensions
 * G: output, gradient magnitude : sqrt(Gx^2+Gy^2)
 * Need 2D grid, a thread per pixel
 * No use of separable algorithm because we need to do this in one kernel
 * No use of shared memory because
 * - we don't handle it in the CPU pass
 */

#define SOBEL_SIZE 3
#define SOBEL_RADIUS (SOBEL_SIZE / 2)

void computeGradient(float *Is, size_t bytesIs, float *Sx, size_t bytesSx,
                     float *Sy, size_t bytesSy, float *G, size_t bytesG, long m,
                     long n) {

  __hpvm__hint(hpvm::DEVICE);
  __hpvm__attributes(3, Is, Sx, Sy, 1, G);

  void *thisNode = __hpvm__getNode();
  long gx = __hpvm__getNodeInstanceID_x(thisNode);
  long gy = __hpvm__getNodeInstanceID_y(thisNode);

  int gloc = gx + gy * n;

  float Gx = 0;
  float Gy = 0;
  float gval;
  int loadOffset;

  if ((gx < n) && (gy < m)) {
    for (int i = -SOBEL_RADIUS; i <= SOBEL_RADIUS; i++)
      for (int j = -SOBEL_RADIUS; j <= SOBEL_RADIUS; j++) {

        loadOffset = gloc + i * n + j;

        if ((gy + i) < 0) // top contour
          loadOffset = gx + j;
        else if ((gy + i) > m - 1) // bottom contour
          loadOffset = (m - 1) * n + gx + j;
        else
          loadOffset = gloc + i * n + j; // within image vertically

        // Adjust so we are within image horizonally
        if ((gx + j) < 0) // left contour
          loadOffset -= (gx + j);
        else if ((gx + j) > n - 1) // right contour
          loadOffset = loadOffset - gx - j + n - 1;

        gval = Is[loadOffset];
        Gx += gval * Sx[(SOBEL_RADIUS + i) * SOBEL_SIZE + SOBEL_RADIUS + j];
        Gy += gval * Sy[(SOBEL_RADIUS + i) * SOBEL_SIZE + SOBEL_RADIUS + j];
      }

    G[gloc] = sqrt(Gx * Gx + Gy * Gy);
  }
  __hpvm__return(1, bytesG);
}

void WrapperComputeGradient(float *Is, size_t bytesIs, float *Sx,
                            size_t bytesSx, float *Sy, size_t bytesSy, float *G,
                            size_t bytesG, long m, long n) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(3, Is, Sx, Sy, 1, G);
  void *CGNode = __hpvm__createNodeND(2, computeGradient, m, n);
  __hpvm__bindIn(CGNode, 0, 0, 0); // Bind Is
  __hpvm__bindIn(CGNode, 1, 1, 0); // Bind bytesIs
  __hpvm__bindIn(CGNode, 2, 2, 0); // Bind Sx
  __hpvm__bindIn(CGNode, 3, 3, 0); // Bind bytesSx
  __hpvm__bindIn(CGNode, 4, 4, 0); // Bind Sy
  __hpvm__bindIn(CGNode, 5, 5, 0); // Bind bytesSy
  __hpvm__bindIn(CGNode, 6, 6, 0); // Bind G
  __hpvm__bindIn(CGNode, 7, 7, 0); // Bind bytesG
  __hpvm__bindIn(CGNode, 8, 8, 0); // Bind m
  __hpvm__bindIn(CGNode, 9, 9, 0); // Bind n

  __hpvm__bindOut(CGNode, 0, 0, 0); // bind output bytesG
}

/*
 * Reduction
 * G : input
 * maxG: output
 * m, n: input size
 * Needs a single thread block
 */
void computeMaxGradientLeaf(float *G, size_t bytesG, float *maxG,
                            size_t bytesMaxG, long m, long n) {

  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(1, G, 1, maxG);

  void *thisNode = __hpvm__getNode();

  long lx = __hpvm__getNodeInstanceID_x(thisNode);     // threadIdx.x
  long dimx = __hpvm__getNumNodeInstances_x(thisNode); // blockDim.x

  // Assume a single thread block
  // Thread block iterates over all elements
  for (int i = lx + dimx; i < m * n; i += dimx) {
    if (G[lx] < G[i])
      G[lx] = G[i];
  }

  // First thread iterates over all elements of the thread block
  long bounds = dimx < m * n ? dimx : m * n;
  if (lx == 0) {
    for (int i = 1; i < bounds; i++)
      if (G[lx] < G[i])
        G[lx] = G[i];

    *maxG = G[lx];
  }

  __hpvm__return(1, bytesMaxG);
}

void computeMaxGradientTB(float *G, size_t bytesG, float *maxG,
                          size_t bytesMaxG, long m, long n, long block_x) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(2, G, maxG, 1, maxG);
  void *CMGLeafNode = __hpvm__createNodeND(1, computeMaxGradientLeaf, block_x);
  __hpvm__bindIn(CMGLeafNode, 0, 0, 0); // Bind G
  __hpvm__bindIn(CMGLeafNode, 1, 1, 0); // Bind bytesG
  __hpvm__bindIn(CMGLeafNode, 2, 2, 0); // Bind maxG
  __hpvm__bindIn(CMGLeafNode, 3, 3, 0); // Bind bytesMaxG
  __hpvm__bindIn(CMGLeafNode, 4, 4, 0); // Bind m
  __hpvm__bindIn(CMGLeafNode, 5, 5, 0); // Bind n

  __hpvm__bindOut(CMGLeafNode, 0, 0, 0); // bind output bytesMaxG
}

void WrapperComputeMaxGradient(float *G, size_t bytesG, float *maxG,
                               size_t bytesMaxG, long m, long n, long block_x,
                               long grid_x) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(2, G, maxG, 1, maxG);
  void *CMGTBNode = __hpvm__createNodeND(1, computeMaxGradientTB, grid_x);
  __hpvm__bindIn(CMGTBNode, 0, 0, 0); // Bind G
  __hpvm__bindIn(CMGTBNode, 1, 1, 0); // Bind bytesG
  __hpvm__bindIn(CMGTBNode, 2, 2, 0); // Bind maxG
  __hpvm__bindIn(CMGTBNode, 3, 3, 0); // Bind bytesMaxG
  __hpvm__bindIn(CMGTBNode, 4, 4, 0); // Bind m
  __hpvm__bindIn(CMGTBNode, 5, 5, 0); // Bind n
  __hpvm__bindIn(CMGTBNode, 6, 6, 0); // Bind block_x

  __hpvm__bindOut(CMGTBNode, 0, 0, 0); // bind output bytesMaxG
}

/* Reject the zero crossings where the gradient is below a threshold */
/*
 * S    : input (computed zero crossings)
 * m, n : dimensions
 * G: gradient of (smoothed) image
 * E    : output (edges of the image)
 * Need 2D grid, a thread per pixel
 */

#define THETA 0.1
void rejectZeroCrossings(float *S, size_t bytesS, float *G, size_t bytesG,
                         float *maxG, size_t bytesMaxG, float *E, size_t bytesE,
                         long m, long n) {
  __hpvm__hint(hpvm::DEVICE);
  __hpvm__attributes(3, S, G, maxG, 1, E);

  void *thisNode = __hpvm__getNode();
  int gx = __hpvm__getNodeInstanceID_x(thisNode);
  int gy = __hpvm__getNodeInstanceID_y(thisNode);

  float mG = *maxG;
  if ((gx < n) && (gy < m)) {
    E[gy * n + gx] =
        ((S[gy * n + gx] > 0.0) && (G[gy * n + gx] > THETA * mG)) ? 1.0 : 0.0;
  }
  __hpvm__return(1, bytesE);
}

void WrapperRejectZeroCrossings(float *S, size_t bytesS, float *G,
                                size_t bytesG, float *maxG, size_t bytesMaxG,
                                float *E, size_t bytesE, long m, long n) {
  __hpvm__hint(hpvm::CPU_TARGET);
  __hpvm__attributes(3, S, G, maxG, 1, E);
  void *RZCNode = __hpvm__createNodeND(2, rejectZeroCrossings, m, n);
  __hpvm__bindIn(RZCNode, 0, 0, 0); // Bind S
  __hpvm__bindIn(RZCNode, 1, 1, 0); // Bind bytesS
  __hpvm__bindIn(RZCNode, 2, 2, 0); // Bind G
  __hpvm__bindIn(RZCNode, 3, 3, 0); // Bind bytesG
  __hpvm__bindIn(RZCNode, 4, 4, 0); // Bind maxG
  __hpvm__bindIn(RZCNode, 5, 5, 0); // Bind bytesMaxG
  __hpvm__bindIn(RZCNode, 6, 6, 0); // Bind E
  __hpvm__bindIn(RZCNode, 7, 7, 0); // Bind bytesE
  __hpvm__bindIn(RZCNode, 8, 8, 0); // Bind m
  __hpvm__bindIn(RZCNode, 9, 9, 0); // Bind n

  __hpvm__bindOut(RZCNode, 0, 0, 0); // bind output bytesE
}

// Pipelined Root node
void edgeDetection(float *I, size_t bytesI,       // 0
                   float *Is, size_t bytesIs,     // 2
                   float *L, size_t bytesL,       // 4
                   float *S, size_t bytesS,       // 6
                   float *G, size_t bytesG,       // 8
                   float *maxG, size_t bytesMaxG, // 10
                   float *E, size_t bytesE,       // 12
                   float *Gs, size_t bytesGs,     // 14
                   float *B, size_t bytesB,       // 16
                   float *Sx, size_t bytesSx,     // 18
                   float *Sy, size_t bytesSy,     // 20
                   long m,                        // 22
                   long n,                        // 23
                   long block_x,                  // 24
                   long grid_x                    // 25
) {
  __hpvm__attributes(5, I, Gs, B, Sx, Sy, 6, Is, L, S, G, maxG, E);
  __hpvm__hint(hpvm::CPU_TARGET);
  void *GSNode = __hpvm__createNodeND(0, WrapperGaussianSmoothing);
  void *LNode = __hpvm__createNodeND(0, WrapperlaplacianEstimate);
  void *CZCNode = __hpvm__createNodeND(0, WrapperComputeZeroCrossings);
  void *CGNode = __hpvm__createNodeND(0, WrapperComputeGradient);
  void *CMGNode = __hpvm__createNodeND(0, WrapperComputeMaxGradient);
  void *RZCNode = __hpvm__createNodeND(0, WrapperRejectZeroCrossings);

  // Gaussian Inputs
  __hpvm__bindIn(GSNode, 0, 0, 1);  // Bind I
  __hpvm__bindIn(GSNode, 1, 1, 1);  // Bind bytesI
  __hpvm__bindIn(GSNode, 14, 2, 1); // Bind Gs
  __hpvm__bindIn(GSNode, 15, 3, 1); // Bind bytesGs
  __hpvm__bindIn(GSNode, 2, 4, 1);  // Bind Is
  __hpvm__bindIn(GSNode, 3, 5, 1);  // Bind bytesIs
  __hpvm__bindIn(GSNode, 22, 6, 1); // Bind m
  __hpvm__bindIn(GSNode, 23, 7, 1); // Bind n

  // Laplacian Inputs
  __hpvm__bindIn(LNode, 2, 0, 1);          // Bind Is
  __hpvm__edge(GSNode, LNode, 1, 0, 1, 1); // Get bytesIs
  __hpvm__bindIn(LNode, 16, 2, 1);         // Bind B
  __hpvm__bindIn(LNode, 17, 3, 1);         // Bind bytesB
  __hpvm__bindIn(LNode, 4, 4, 1);          // Bind L
  __hpvm__bindIn(LNode, 5, 5, 1);          // Bind bytesL
  __hpvm__bindIn(LNode, 22, 6, 1);         // Bind m
  __hpvm__bindIn(LNode, 23, 7, 1);         // Bind n

  // Compute ZC Inputs
  __hpvm__bindIn(CZCNode, 4, 0, 1);         // Bind L
  __hpvm__edge(LNode, CZCNode, 1, 0, 1, 1); // Get bytesL
  __hpvm__bindIn(CZCNode, 16, 2, 1);        // Bind B
  __hpvm__bindIn(CZCNode, 17, 3, 1);        // Bind bytesB
  __hpvm__bindIn(CZCNode, 6, 4, 1);         // Bind S
  __hpvm__bindIn(CZCNode, 7, 5, 1);         // Bind bytesS
  __hpvm__bindIn(CZCNode, 22, 6, 1);        // Bind m
  __hpvm__bindIn(CZCNode, 23, 7, 1);        // Bind n

  // Gradient Inputs
  __hpvm__bindIn(CGNode, 2, 0, 1);          // Bind Is
  __hpvm__edge(GSNode, CGNode, 1, 1, 1, 1); // Get bytesIs
  __hpvm__bindIn(CGNode, 18, 2, 1);         // Bind Sx
  __hpvm__bindIn(CGNode, 19, 3, 1);         // Bind bytesSx
  __hpvm__bindIn(CGNode, 20, 4, 1);         // Bind Sy
  __hpvm__bindIn(CGNode, 21, 5, 1);         // Bind bytesSy
  __hpvm__bindIn(CGNode, 8, 6, 1);          // Bind G
  __hpvm__bindIn(CGNode, 9, 7, 1);          // Bind bytesG
  __hpvm__bindIn(CGNode, 22, 8, 1);         // Bind m
  __hpvm__bindIn(CGNode, 23, 9, 1);         // Bind n

  // Max Gradient Inputs
  __hpvm__bindIn(CMGNode, 8, 0, 1);          // Bind G
  __hpvm__edge(CGNode, CMGNode, 1, 0, 1, 1); // Get bytesG
  __hpvm__bindIn(CMGNode, 10, 2, 1);         // Bind maxG
  __hpvm__bindIn(CMGNode, 11, 3, 1);         // Bind bytesMaxG
  __hpvm__bindIn(CMGNode, 22, 4, 1);         // Bind m
  __hpvm__bindIn(CMGNode, 23, 5, 1);         // Bind n
  __hpvm__bindIn(CMGNode, 24, 6, 1);         // Bind block_x
  __hpvm__bindIn(CMGNode, 25, 7, 1);         // Bind grid_x

  // Reject ZC Inputs
  __hpvm__bindIn(RZCNode, 6, 0, 1);           // Bind S
  __hpvm__edge(CZCNode, RZCNode, 1, 0, 1, 1); // Get bytesS
  __hpvm__bindIn(RZCNode, 8, 2, 1);           // Bind G
  __hpvm__bindIn(RZCNode, 9, 3, 1);           // Bind bytesG
  __hpvm__bindIn(RZCNode, 10, 4, 1);          // Bind maxG
  __hpvm__edge(CMGNode, RZCNode, 1, 0, 5, 1); // Get bytesMaxG
  __hpvm__bindIn(RZCNode, 12, 6, 1);          // Bind E
  __hpvm__bindIn(RZCNode, 13, 7, 1);          // Bind bytesE
  __hpvm__bindIn(RZCNode, 22, 8, 1);          // Bind m
  __hpvm__bindIn(RZCNode, 23, 9, 1);          // Bind n

  __hpvm__bindOut(RZCNode, 0, 0, 1); // Bind output
}
}

using namespace cv;

void getNextFrame(VideoCapture &VC, Mat &F) {
  VC >> F;
  /// Convert the image to grayscale if image colored
  if (F.channels() == 3)
    cvtColor(F, F, CV_BGR2GRAY);

  F.convertTo(F, CV_32F, 1.0 / 255.0);
}

int main(int argc, char *argv[]) {

  if (argc < 2) {
    fprintf(stderr, "Expecting input image filename\n");
    exit(-1);
  }
  char *inFile = argv[1];
  fprintf(stderr, "Running pipeline on %s\n", inFile);

  size_t I_sz;
  long block_x, grid_x;

  std::cout << "Using OpenCV" << CV_VERSION << "\n";

  /* Read in data */
  std::cout << "Reading video file: " << inFile << "\n";
  VideoCapture cap(inFile);
  if (!cap.isOpened()) {
    std::cout << "Could not open video file"
              << "\n";
    return -1;
  }

  int NUM_FRAMES = cap.get(CV_CAP_PROP_FRAME_COUNT);
  NUM_FRAMES = 600;
  std::cout << "Number of frames = " << NUM_FRAMES << "\n";

  namedWindow(input_window, CV_WINDOW_AUTOSIZE);
  namedWindow(output_window, CV_WINDOW_AUTOSIZE);
  moveWindow(input_window, POSX_IN, POSY_IN);
  moveWindow(output_window, POSX_OUT, POSY_OUT);

  Mat src, Is, L, S, G, E;

  getNextFrame(cap, src);

  std::cout << "Image dimension = " << src.size() << "\n";
  if (!src.isContinuous()) {
    std::cout << "Expecting contiguous storage of image in memory!\n";
    exit(-1);
  }

  Is = Mat(src.size[0], src.size[1], CV_32F);
  L = Mat(src.size[0], src.size[1], CV_32F);
  S = Mat(src.size[0], src.size[1], CV_32F);
  G = Mat(src.size[0], src.size[1], CV_32F);
  E = Mat(src.size[0], src.size[1], CV_32F);

  // All these matrices need to have their data array contiguous in memory
  assert(src.isContinuous() && Is.isContinuous() && L.isContinuous() &&
         S.isContinuous() && G.isContinuous() && E.isContinuous());

  __hpvm__init();

  // copy A to device memory
  I_sz = src.size[0] * src.size[1] * sizeof(float);

  size_t bytesMaxG = sizeof(float);
  float *maxG = (float *)malloc(bytesMaxG);

  float B[] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
  size_t bytesB = 9 * sizeof(float);
  float Sx[] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
  size_t bytesSx = 9 * sizeof(float);
  float Sy[] = {-1, -2, -1, 0, 0, 0, 1, 2, 1};
  size_t bytesSy = 9 * sizeof(float);

  float Gs[] = {
      0.000036, 0.000363, 0.001446, 0.002291, 0.001446, 0.000363, 0.000036,
      0.000363, 0.003676, 0.014662, 0.023226, 0.014662, 0.003676, 0.000363,
      0.001446, 0.014662, 0.058488, 0.092651, 0.058488, 0.014662, 0.001446,
      0.002291, 0.023226, 0.092651, 0.146768, 0.092651, 0.023226, 0.002291,
      0.001446, 0.014662, 0.058488, 0.092651, 0.058488, 0.014662, 0.001446,
      0.000363, 0.003676, 0.014662, 0.023226, 0.014662, 0.003676, 0.000363,
      0.000036, 0.000363, 0.001446, 0.002291, 0.001446, 0.000363, 0.000036};
  size_t bytesGs = 7 * 7 * sizeof(float);

  block_x = 256;
  // grid_x should be equal to the number of SMs on GPU. FTX 680 has 8 SMs
  grid_x = 1;

  Mat in, out;
  resize(src, in, Size(HEIGHT, WIDTH));
  resize(E, out, Size(HEIGHT, WIDTH));
  imshow(input_window, in);
  imshow(output_window, out);
  //  waitKey(0);

  struct InStruct *args = (struct InStruct *)malloc(sizeof(InStruct));
  packData(args, (float *)src.data, I_sz, (float *)Is.data, I_sz,
           (float *)L.data, I_sz, (float *)S.data, I_sz, (float *)G.data, I_sz,
           maxG, bytesMaxG, (float *)E.data, I_sz, Gs, bytesGs, B, bytesB, Sx,
           bytesSx, Sy, bytesSy, src.size[0], src.size[1], block_x, grid_x);

  // Check if the total elements is a multiple of block size
  assert(src.size[0] * src.size[1] % block_x == 0);

  for (unsigned j = 0; j < NUM_RUNS; j++) {
    std::cout << "Run: " << j << "\n";
    void *DFG = __hpvm__launch(1, edgeDetection, (void *)args);

    cap = VideoCapture(inFile);
    getNextFrame(cap, src);

    if (NUM_FRAMES >= 2) {
      for (int i = 0; i < NUM_FRAMES; i++) {
        args->I = (float *)src.data;

        *maxG = 0.0;

        llvm_hpvm_track_mem(src.data, I_sz);
        llvm_hpvm_track_mem(Is.data, I_sz);
        llvm_hpvm_track_mem(L.data, I_sz);
        llvm_hpvm_track_mem(S.data, I_sz);
        llvm_hpvm_track_mem(G.data, I_sz);
        llvm_hpvm_track_mem(maxG, bytesMaxG);
        llvm_hpvm_track_mem(E.data, I_sz);
        llvm_hpvm_track_mem(Gs, bytesGs);
        llvm_hpvm_track_mem(B, bytesB);
        llvm_hpvm_track_mem(Sx, bytesSx);
        llvm_hpvm_track_mem(Sy, bytesSy);

        __hpvm__push(DFG, args);
        void *ret = __hpvm__pop(DFG);
        // This is reading the result of the streaming graph
        size_t framesize = ((OutStruct *)ret)->ret;

        llvm_hpvm_request_mem(maxG, bytesMaxG);
        llvm_hpvm_request_mem(E.data, I_sz);

        Mat in, out;
        resize(src, in, Size(HEIGHT, WIDTH));
        resize(E, out, Size(HEIGHT, WIDTH));
        imshow(output_window, out);
        imshow(input_window, in);
        waitKey(1);

        llvm_hpvm_untrack_mem(src.data);
        llvm_hpvm_untrack_mem(Is.data);
        llvm_hpvm_untrack_mem(L.data);
        llvm_hpvm_untrack_mem(S.data);
        llvm_hpvm_untrack_mem(G.data);
        llvm_hpvm_untrack_mem(maxG);
        llvm_hpvm_untrack_mem(E.data);
        llvm_hpvm_untrack_mem(Gs);
        llvm_hpvm_untrack_mem(B);
        llvm_hpvm_untrack_mem(Sx);
        llvm_hpvm_untrack_mem(Sy);

        getNextFrame(cap, src);
      }
    } else {
      __hpvm__push(DFG, args);
      __hpvm__pop(DFG);
    }
    __hpvm__wait(DFG);
  }
  __hpvm__cleanup();
  return 0;
}
