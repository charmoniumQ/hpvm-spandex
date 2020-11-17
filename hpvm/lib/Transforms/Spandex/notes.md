# Questions

- LLVM: Is `opt -load *.so -basic-aa -steens-aa -spandex -dfg2llvm-cpu -clearDFG` right?
- HPVM: Will alias analysis work on an HPVM DFG?

# Assumptions

- All accesses must be to an address p + c where p is statically-traceable to an argument or the return of a malloc-call and c is a static constant.
- If p is different from p', then p+c is a different block than p'+c'.
- All pointers returned by malloc are block-aligned.
- Functions that are not HPVM nodes, malloc, or free cannot take or return pointers.
  - When a function gets called, I would lose track of where the pointers came from, so I could not tell if there are conflicting accesses.
  - However, if you want to use a function, you can still `inline` it.
- Straight-line code.
  - Loops are permissible if they are unrolled at compile-time.
- All accesses are 1 word.
- No "manual" synchronization.
- All
