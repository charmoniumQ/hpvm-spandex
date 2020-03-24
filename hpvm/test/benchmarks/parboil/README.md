## Compiling and Running Parboil Benchmarks
Several tests from the [parboil suite](http://impact.crhc.illinois.edu/parboil/parboil.aspx) have been ported to HPVM.
To run one of these tests, navigate to its directory under `benchmarks/`.
Tests may be built for the cpu or gpu with hpvm.
```
# sgemm example
cd benchmarks/sgemm
# HPVM cpu
make TARGET=seq VERSION=hpvm
make run TARGET=seq VERSION=hpvm
# HPVM gpu
make TARGET=gpu VERSION=hpvm
make run TARGET=gpu VERSION=hpvm
```

# Current Benchmark Compatability

| Benchmark | Version | Supported on CPU | Supported on GPU |
| :-------- | :------ | :--------------: | :--------------: |
| sgemm     | hpvm    | ✔                | ✔                |
| stencil   | hpvm    | ✔                | ✔                |
| spmv      | hpvm    | ✔                | ✘                |
| lbm       | hpvm    | ✔                | ✘                |
