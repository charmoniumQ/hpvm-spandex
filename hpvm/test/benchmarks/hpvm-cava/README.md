# HPVM-CAVA
Harvard Camera Pipeline with HPVM intrinsics

## Camera Pipeline

The camera pipeline is a simple five-stage image signal processor (ISP) which processes raw images (i.e., sensor inputs) into an image that can feed into a vision backend (e.g., a CNN).

## How to Build and Test

After building HPVM, the following steps are required to build and run the camera pipeline:

1. Build with `make TARGET=seq` for CPU and `make TARGET=gpu` for gpu.
2. Run with `./cava-hpvm-<Target> example-tulip-small/raw_tulip-small.bin example-tulip-small/tulip-small`. 
    * `<Target>` can be either `seq` or `gpu` depending on what target is used to build.
    * This processes the raw image `example-tulip-small/raw_tulip-small.bin`. Note that raw images are different from bitmaps, so you might need to obtain them using special software.
    * This generates: `tulip-small.bin` and `tulip-small-<stage>.bin` where `<stage>` represents the stage of the pipeline, in the directory `example-tulip-small`.
3. Convert the binary outputs to a PNG with `./convert.sh example-tulip-small`.
    * The convert script uses some scripts in the `script` directory. These need to be compiled first using `cd ./scripts; make`.
    * **In order to compile the convert scripts, the path to numpy should be set correctly in the makefile.**
4. View the resulting PNG at `example-tulip-small/tulip-small.png`. (As well as all the intermediary images for each stage `tulip-small-<stage>.png`).
