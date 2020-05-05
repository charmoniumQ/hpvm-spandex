#include <iostream>
#include <stdlib.h>
#include <cassert>
#include <hpvm.h>

void double_scalar_fn(int* in, size_t inSize, int* out, size_t outSize) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);
	long i = __hpvm__getNodeInstanceID_x(__hpvm__getNode());
	out[i] = in[i] + in[i];
}

void double_vector_fn(int* in, size_t inSize, int* out, size_t outSize, size_t length) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);
	
	void* scalar = __hpvm__createNodeND(1, double_scalar_fn, length);
	__hpvm__bindIn(scalar, 0 /*in     */, 0 /*in     */, HPVM_NONSTREAMING);
	__hpvm__bindIn(scalar, 1 /*inSize */, 1 /*inSize */, HPVM_NONSTREAMING);
	__hpvm__bindIn(scalar, 2 /*out    */, 2 /*out    */, HPVM_NONSTREAMING);
	__hpvm__bindIn(scalar, 3 /*outSize*/, 3 /*outSize*/, HPVM_NONSTREAMING);
}

void square_scalar_fn(int* in, size_t inSize, int* out, size_t outSize) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);
	long i = __hpvm__getNodeInstanceID_x(__hpvm__getNode());
	out[i] = in[i] * in[i];
}

void square_vector_fn(int* in, size_t inSize, int* out, size_t outSize, size_t length) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);
	
	void* scalar = __hpvm__createNodeND(1, square_scalar_fn, length);
	__hpvm__bindIn(scalar, 0 /*in     */, 0 /*in     */, HPVM_NONSTREAMING);
	__hpvm__bindIn(scalar, 1 /*inSize */, 1 /*inSize */, HPVM_NONSTREAMING);
	__hpvm__bindIn(scalar, 2 /*out    */, 2 /*out    */, HPVM_NONSTREAMING);
	__hpvm__bindIn(scalar, 3 /*outSize*/, 3 /*outSize*/, HPVM_NONSTREAMING);
}

void square_and_double_vector_fn(int* in, size_t inSize, int* tmp, size_t tmpSize, int* out, size_t outSize, size_t length, size_t length_dup) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);

	void* double_vector = __hpvm__createNodeND(0, double_vector_fn);
	void* square_vector = __hpvm__createNodeND(0, square_vector_fn);

	__hpvm__bindIn(square_vector, 0 /*in        */, 0 /*in     */, HPVM_NONSTREAMING);
	__hpvm__bindIn(square_vector, 1 /*inSize    */, 1 /*inSize */, HPVM_NONSTREAMING);
	__hpvm__bindIn(square_vector, 2 /*tmp       */, 2 /*out    */, HPVM_NONSTREAMING);
	__hpvm__bindIn(square_vector, 3 /*tmpSize   */, 3 /*outSize*/, HPVM_NONSTREAMING);
	__hpvm__bindIn(square_vector, 6 /*length    */, 4 /*length */, HPVM_NONSTREAMING);

	__hpvm__bindIn(double_vector, 2 /*tmp       */, 0 /*in     */, HPVM_NONSTREAMING);
	__hpvm__bindIn(double_vector, 3 /*tmpSize   */, 1 /*inSize */, HPVM_NONSTREAMING);
	__hpvm__bindIn(double_vector, 4 /*out       */, 2 /*out    */, HPVM_NONSTREAMING);
	__hpvm__bindIn(double_vector, 5 /*outSize   */, 3 /*outSize*/, HPVM_NONSTREAMING);
	__hpvm__bindIn(double_vector, 7 /*length_dup*/, 4 /*length */, HPVM_NONSTREAMING);
}

struct __attribute__((__packed__)) InStruct {
	int* in;
	size_t inSize;
	int* tmp;
	size_t tmpSize;
	int* out;
	size_t outSize;
	size_t length;
	size_t length_dup;
};

int main(int argc, char *argv[]) {
	__hpvm__init();

	const size_t length = 10;
	int in[length];
	int tmp[length];
	int out[length];
	for (size_t i = 0; i < length; ++i) {
		in[i] = i;
		out[i] = 0;
		tmp[i] = 0;
	}

	llvm_hpvm_track_mem(in, length * sizeof(int));
	llvm_hpvm_track_mem(tmp, length * sizeof(int));
	llvm_hpvm_track_mem(out, length * sizeof(int));

	struct InStruct args {
		in, length * sizeof(int),
		tmp, length * sizeof(int),
		out, length * sizeof(int),
		length, length
	};
	void *DFG = __hpvm__launch(HPVM_NONSTREAMING, square_and_double_vector_fn, reinterpret_cast<void*>(&args));
	__hpvm__wait(DFG);

	// Is this necessary?
	llvm_hpvm_request_mem(out, length * sizeof(int));

	for (size_t i = 0; i < length; ++i) {
		assert(out[i] == i*i*2);
	}

	llvm_hpvm_untrack_mem(in);
	llvm_hpvm_untrack_mem(tmp);
	llvm_hpvm_untrack_mem(out);

	__hpvm__wait(DFG);

	__hpvm__cleanup();

	return 0;
}
