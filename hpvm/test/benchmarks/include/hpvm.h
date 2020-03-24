/***************************************************************************
 *cr
 *cr            (C) Copyright 2010 The Board of Trustees of the
 *cr                        University of Illinois
 *cr                         All Rights Reserved
 *cr
 ***************************************************************************/

#ifndef DEVICE
#define DEVICE GPU_TARGET
#endif

#include "../../../include/SupportHPVM/HPVMHint.h"
#ifdef __cplusplus
#include <cstddef>
#else
#include <stddef.h>
#endif

#ifndef __cplusplus
#define noexcept
#endif

#ifdef __cplusplus
extern "C" {
void __hpvm__hint(hpvm::Target) noexcept;
#else
void __hpvm__hint(enum Target) noexcept;
#endif

void *__hpvm__createNodeND(unsigned, ...) noexcept;
void __hpvm__return(unsigned, ...) noexcept;

void __hpvm__attributes(unsigned, ...) noexcept;
void __hpvm__init() noexcept;
void __hpvm__cleanup() noexcept;

void __hpvm__bindIn(void *, unsigned, unsigned, unsigned) noexcept;
void __hpvm__bindOut(void *, unsigned, unsigned, unsigned) noexcept;
void *__hpvm__edge(void *, void *, unsigned, unsigned, unsigned,
                   unsigned) noexcept;

void __hpvm__push(void *, void *) noexcept;
void *__hpvm__pop(void *) noexcept;
void *__hpvm__launch(unsigned, ...) noexcept;
void __hpvm__wait(void *) noexcept;

void *__hpvm__getNode() noexcept;
void *__hpvm__getParentNode(void *) noexcept;
void __hpvm__barrier() noexcept;
void *__hpvm__malloc(long) noexcept;
long __hpvm__getNodeInstanceID_x(void *) noexcept;
long __hpvm__getNodeInstanceID_y(void *) noexcept;
long __hpvm__getNodeInstanceID_z(void *) noexcept;
long __hpvm__getNumNodeInstances_x(void *) noexcept;
long __hpvm__getNumNodeInstances_y(void *) noexcept;
long __hpvm__getNumNodeInstances_z(void *) noexcept;

// Atomic
// signed int
int __hpvm__atomic_add(int *, int) noexcept;
int __hpvm__atomic_sub(int *, int) noexcept;
int __hpvm__atomic_xchg(int *, int) noexcept;
int __hpvm__atomic_inc(int *) noexcept;
int __hpvm__atomic_dec(int *) noexcept;
int __hpvm__atomic_min(int *, int) noexcept;
int __hpvm__atomic_max(int *, int) noexcept;
int __hpvm__atomic_and(int *, int) noexcept;
int __hpvm__atomic_or(int *, int) noexcept;
int __hpvm__atomic_xor(int *, int) noexcept;

void llvm_hpvm_track_mem(void *, size_t) noexcept;
void llvm_hpvm_untrack_mem(void *) noexcept;
void llvm_hpvm_request_mem(void *, size_t) noexcept;

#ifdef __cplusplus
}
#endif
