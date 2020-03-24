# Edge Detection Pipeline

HPVM benchmark performing edge detection on a stream of input images.

## Dependencies

Edge detection pipeline depends on `OpenCV==2.4`. To use OpenCV 3.4, go to line 13 in `src/main.cc`
and replace 

```
#include "opencv2/ocl/ocl.hpp"
```
with
```
#include "opencv2/core/ocl.hpp"
```

Edge detection pipeline is not tested with other versions of OpenCV.

## How to Build and Test

```
make TARGET={seq, gpu}
./pipeline-{seq, gpu} datasets/formula1_scaled.mp4
```
