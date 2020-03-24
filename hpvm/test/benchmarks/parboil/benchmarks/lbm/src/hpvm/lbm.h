/***************************************************************************
 *cr
 *cr            (C) Copyright 2010 The Board of Trustees of the
 *cr                        University of Illinois
 *cr                         All Rights Reserved
 *cr
 ***************************************************************************/

/*############################################################################*/

#ifndef _LBM_H_
#define _LBM_H_

/*############################################################################*/

void LBM_allocateGrid(float **ptr);
void LBM_freeGrid(float **ptr);
void LBM_initializeGrid(LBM_Grid grid);
void LBM_initializeSpecialCellsForLDC(LBM_Grid grid);
void LBM_loadObstacleFile(LBM_Grid grid, const char *filename);
void LBM_swapGrids(LBM_Grid *grid1, LBM_Grid *grid2);
void LBM_showGridStatistics(LBM_Grid Grid);
void LBM_storeVelocityField(LBM_Grid grid, const char *filename,
                            const BOOL binary);

/*############################################################################*/

#endif /* _LBM_H_ */
