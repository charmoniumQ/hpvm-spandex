//===- IntrinsicsHPVM.td - Defines HPVM intrinsics ---------*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines all of the HPVM-specific intrinsics.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "hpvm" in {
  /* All intrinsics start with "llvm.hpvm."
   * As we do not want the compiler to mess with these intrinsics, we assume
   * worst memory behavior for all these intrinsics.
   */

  /* Initialization intrinsic -
   * i8* llvm.hpvm.setup(function*);
   */
  def int_hpvm_init : Intrinsic<[], [], []>;

  /* Launch intrinsic - with streaming argument
   * i8* llvm.hpvm.launch(i8*, ArgList*, i1);
   */
  def int_hpvm_launch : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty,
                                  llvm_ptr_ty, llvm_i1_ty], []>;

  /* Push intrinsic - push data on streaming pipeline
   * void llvm.hpvm.push(i8*, ArgList*);
   */
  def int_hpvm_push : Intrinsic<[], [llvm_ptr_ty, llvm_ptr_ty], []>;

  /* Pop intrinsic - pop data from streaming pipeline
   * i8* llvm.hpvm.pop(i8*);
   */
  def int_hpvm_pop : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty], []>;

  /* Cleanup intrinsic -
   * void llvm.hpvm.cleanup(i8*);
   */
  def int_hpvm_cleanup : Intrinsic<[], [], []>;

  /* Wait intrinsic -
   * void llvm.hpvm.wait(graphID*);
   */
  def int_hpvm_wait : Intrinsic<[], [llvm_ptr_ty], []>;

  /* Track memory intrinsic -
   * void llvm.hpvm.trackMemory(i8*, i64);
   */
  def int_hpvm_trackMemory : Intrinsic<[], [llvm_ptr_ty, llvm_i64_ty], []>;

  /* Track memory intrinsic -
   * void llvm.hpvm.untrackMemory(i8*);
   */
  def int_hpvm_untrackMemory : Intrinsic<[], [llvm_ptr_ty], []>;

  /* Request memory intrinsic -
   * void llvm.hpvm.requestMemory(i8*, i64);
   */
  def int_hpvm_requestMemory : Intrinsic<[], [llvm_ptr_ty, llvm_i64_ty], []>;

  /* Create Node intrinsic -
   * i8* llvm.hpvm.createNode(function*);
   */
  def int_hpvm_createNode : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty], []>;

  /* Create Node 1D array intrinsic -
   * i8* llvm.hpvm.createNode1D(function*, i64);
   */
  def int_hpvm_createNode1D : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty,
                                        llvm_i64_ty], []>;

  /* Create Node 2D array intrinsic -
   * i8* llvm.hpvm.createNode2D(function*, i64, i64);
   */
  def int_hpvm_createNode2D : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty,
                                        llvm_i64_ty, llvm_i64_ty], []>;

  /* Create Node 3D array intrinsic -
   * i8* llvm.hpvm.createNode2D(function*, i64, i64, i64);
   */
  def int_hpvm_createNode3D : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty,
                                        llvm_i64_ty, llvm_i64_ty, llvm_i64_ty],
                                        []>;

  /* Create dataflow edge intrinsic -
   * i8* llvm.hpvm.createEdge(i8*, i8*, i1, i32, i32, i1);
   */
  def int_hpvm_createEdge : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty, llvm_ptr_ty,
                                      llvm_i1_ty, llvm_i32_ty, llvm_i32_ty,
                                      llvm_i1_ty],
                                      []>;

  /* Create bind input intrinsic -
   * void llvm.hpvm.bind.input(i8*, i32, i32);
   */
  def int_hpvm_bind_input : Intrinsic<[], [llvm_ptr_ty, llvm_i32_ty,
                                      llvm_i32_ty, llvm_i1_ty], []>;

  /* Create bind output intrinsic -
   * void llvm.hpvm.bind.output(i8*, i32, i32);
   */
  def int_hpvm_bind_output : Intrinsic<[], [llvm_ptr_ty, llvm_i32_ty,
                                       llvm_i32_ty, llvm_i1_ty], []>;

  /* Find associated dataflow node intrinsic -
   * i8* llvm.hpvm.getNode();
   */
  def int_hpvm_getNode : Intrinsic<[llvm_ptr_ty], [], [IntrNoMem]>;

  /* Find parent dataflow node intrinsic -
   * i8* llvm.hpvm.getParentNode(i8*);
   */
  def int_hpvm_getParentNode : Intrinsic<[llvm_ptr_ty], [llvm_ptr_ty], [IntrNoMem]>;

  /* Find the number of dimensions of a dataflow node intrinsic -
   * i32 llvm.hpvm.getNumDims(i8*);
   */
  def int_hpvm_getNumDims : Intrinsic<[llvm_i32_ty], [llvm_ptr_ty], [IntrNoMem]>;

  /* Find the unique indentifier of a dataflow node (with respect to his parent
   * node) in the specified dimension intrinsic -
   */

  /* i64 llvm.hpvm.getNodeInstanceID.[xyz](i8*);
   */
  def int_hpvm_getNodeInstanceID_x : Intrinsic<[llvm_i64_ty], [llvm_ptr_ty],
                                               [IntrNoMem]>;

  def int_hpvm_getNodeInstanceID_y : Intrinsic<[llvm_i64_ty], [llvm_ptr_ty],
                                               [IntrNoMem]>;

  def int_hpvm_getNodeInstanceID_z : Intrinsic<[llvm_i64_ty], [llvm_ptr_ty],
                                               [IntrNoMem]>;

  /* Find the number of instances of a dataflow node in the specified dimension
   * intrinsic -
   */

  /* i64 llvm.hpvm.getNumNodeInstances.[xyz](i8*);
   */
  def int_hpvm_getNumNodeInstances_x : Intrinsic<[llvm_i64_ty], [llvm_ptr_ty],
                                                 [IntrNoMem]>;

  def int_hpvm_getNumNodeInstances_y : Intrinsic<[llvm_i64_ty], [llvm_ptr_ty],
                                                 [IntrNoMem]>;

  def int_hpvm_getNumNodeInstances_z : Intrinsic<[llvm_i64_ty], [llvm_ptr_ty],
                                                 [IntrNoMem]>;

  /* Local Barrier
   * void llvm.hpvm.barrier();
   */
  def int_hpvm_barrier : Intrinsic<[], [], []>;

  /* Memory allocation inside the graph
   * i8* llvm.hpvm.malloc();
   */
  def int_hpvm_malloc : Intrinsic<[llvm_ptr_ty], [llvm_i64_ty], []>;

  /* Find the vector length supported by target architecture
   * intrinsic -
   * i32 llvm.hpvm.getVectorLength();
   */
  def int_hpvm_getVectorLength : Intrinsic<[llvm_i32_ty], [], []>;

  /* ============ Atomic intrinsics ============= */
  // Atomic arithmetic operations

  /* i32 llvm.hpvm.atomic.add(i32*, i32)*/
  def int_hpvm_atomic_add: Intrinsic<[llvm_i32_ty], [llvm_ptr_ty, llvm_i32_ty],
                                    []>;

  /* i32 llvm.hpvm.atomic.sub(i32*, i32)*/
  def int_hpvm_atomic_sub: Intrinsic<[llvm_i32_ty], [llvm_ptr_ty, llvm_i32_ty],
                                    []>;

  /* i32 llvm.hpvm.atomic.xchg(i32*, i32)*/
  def int_hpvm_atomic_xchg: Intrinsic<[llvm_i32_ty], [llvm_ptr_ty, llvm_i32_ty],
                                    []>;

  /* i32 llvm.hpvm.atomic.min(i32*, i32)*/
  def int_hpvm_atomic_min: Intrinsic<[llvm_i32_ty], [llvm_ptr_ty, llvm_i32_ty],
                                    []>;

  /* i32 llvm.hpvm.atomic.maxi32*, i32)*/
  def int_hpvm_atomic_max: Intrinsic<[llvm_i32_ty], [llvm_ptr_ty, llvm_i32_ty],
                                    []>;

  // Atomic bitwise operations

  /* i32 llvm.hpvm.atomic.and(i32*, i32)*/
  def int_hpvm_atomic_and: Intrinsic<[llvm_i32_ty], [llvm_ptr_ty, llvm_i32_ty],
                                    []>;

  /* i32 llvm.hpvm.atomic.or(i32*, i32)*/
  def int_hpvm_atomic_or: Intrinsic<[llvm_i32_ty], [llvm_ptr_ty, llvm_i32_ty],
                                    []>;

  /* i32 llvm.hpvm.atomic.xor(i32*, i32)*/
  def int_hpvm_atomic_xor: Intrinsic<[llvm_i32_ty], [llvm_ptr_ty, llvm_i32_ty],
                                    []>;

}
