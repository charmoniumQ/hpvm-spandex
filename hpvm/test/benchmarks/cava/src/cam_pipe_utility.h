#ifndef _CAM_PIPE_UTILITY_H_
#define _CAM_PIPE_UTILITY_H_

#include "defs.h"
#include "utility.h"

uint8_t *read_image_from_binary(char *file_path, size_t *row_size,
                                size_t *col_size);
void write_image_to_binary(char *file_path, uint8_t *image, size_t row_size,
                           size_t col_size);
float *transpose_mat(float *inmat, int width, int height);
void convert_hwc_to_chw(uint8_t *input, int row_size, int col_size,
                        uint8_t **result);
void convert_chw_to_hwc(uint8_t *input, int row_size, int col_size,
                        uint8_t **result);

#endif
