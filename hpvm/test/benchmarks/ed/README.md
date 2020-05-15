# Edge Detection Pipeline

HPVM benchmark performing edge detection on a stream of input images.

## Dependencies

Edge detection pipeline depends on `OpenCV` 2.x or 3.x.

Edge detection pipeline is not tested with other versions of OpenCV.

## How to Build and Test

```
make TARGET={seq, gpu}
./pipeline-{seq, gpu} datasets/formula1_scaled.mp4
```
