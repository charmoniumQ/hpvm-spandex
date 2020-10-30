# Questions

- LLVM: Is `opt -load *.so -basic-aa -steens-aa -spandex -dfg2llvm-cpu -clearDFG` right?
- HPVM: Will alias analysis work on an HPVM DFG?

# Assumptions

- All accesses must be to an address p + c where p is returned by malloc and c is a compile-time known constant.
- Functions that are not HPVM nodes, malloc, or free cannot take or return pointers.
  - When a function gets called, I would lose track of where the pointers came from, so I could not tell if there are conflicting accesses.
  - However, if you want to use a function, you can still `inline` it.
- Straight-line code
- All accesses are 1 word.
  - This is not strictly necessary for analysis, but I don't know how multi-word accesses are represented in LLVM yet.
