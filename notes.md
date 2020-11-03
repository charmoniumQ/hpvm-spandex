# Questions

- LLVM: Is `opt -load *.so -basic-aa -steens-aa -spandex -dfg2llvm-cpu -clearDFG` right?
- HPVM: Will alias analysis work on an HPVM DFG?

# Assumptions

- All accesses must be to an address p + c where p is statically-traceable to a pointer returned by malloc and c is a static constant.
- All pointers returned by malloc are block-aligned.
- Functions that are not HPVM nodes, malloc, or free cannot take or return pointers.
  - When a function gets called, I would lose track of where the pointers came from, so I could not tell if there are conflicting accesses.
  - However, if you want to use a function, you can still `inline` it.
- Straight-line code
- All accesses are 1 word.
- No "manual" synchronization.
