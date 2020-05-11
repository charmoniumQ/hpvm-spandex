#include <iostream>
#include <stdlib.h>
#include <cassert>
#include <hpvm.h>

// void nop(size_t dummy) {
// 	__hpvm__hint(hpvm::CPU_TARGET);
// 	__hpvm__return(1, dummy);
// }

void dbl_sclr(int* in, size_t inSize, int* out, size_t outSize) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);
	long i = __hpvm__getNodeInstanceID_x(__hpvm__getNode());
	out[i] = in[i] + in[i];
	__hpvm__return(1, outSize);
}

void dbl_vect(int* in, size_t inSize, int* out, size_t outSize, size_t length) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);
	
	void* dbl_sclr_n = __hpvm__createNodeND(1, dbl_sclr, length);
	__hpvm__bindIn(dbl_sclr_n, 0 /*in     */, 0 /*in     */, HPVM_NONSTREAMING);
	__hpvm__bindIn(dbl_sclr_n, 1 /*inSize */, 1 /*inSize */, HPVM_NONSTREAMING);
	__hpvm__bindIn(dbl_sclr_n, 2 /*out    */, 2 /*out    */, HPVM_NONSTREAMING);
	__hpvm__bindIn(dbl_sclr_n, 3 /*outSize*/, 3 /*outSize*/, HPVM_NONSTREAMING);
	__hpvm__bindOut(dbl_sclr_n, 0 /*outSize*/, 0 /*outSize*/, HPVM_NONSTREAMING);
}

void sqr_sclr(int* in, size_t inSize, int* out, size_t outSize) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);
	long i = __hpvm__getNodeInstanceID_x(__hpvm__getNode());
	out[i] = in[i] * in[i];
	__hpvm__return(1, outSize);
}

void sqr_vect(int* in, size_t inSize, int* out, size_t outSize, size_t length) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);
	
	void* sqr_sclr_n = __hpvm__createNodeND(1, sqr_sclr, length);
	__hpvm__bindIn(sqr_sclr_n, 0 /*in     */, 0 /*in     */, HPVM_NONSTREAMING);
	__hpvm__bindIn(sqr_sclr_n, 1 /*inSize */, 1 /*inSize */, HPVM_NONSTREAMING);
	__hpvm__bindIn(sqr_sclr_n, 2 /*out    */, 2 /*out    */, HPVM_NONSTREAMING);
	__hpvm__bindIn(sqr_sclr_n, 3 /*outSize*/, 3 /*outSize*/, HPVM_NONSTREAMING);
	__hpvm__bindOut(sqr_sclr_n, 0 /*outSize*/, 0 /*outSize*/, HPVM_NONSTREAMING);
}

void dbl_sqr_vect(int* in, size_t inSize, int* tmp, size_t tmpSize, int* out, size_t outSize, size_t length) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(2, in, tmp, 2, out, tmp);

	void* dbl_vect_n = __hpvm__createNodeND(0, dbl_vect);
	void* sqr_vect_n = __hpvm__createNodeND(0, sqr_vect);

	// void* nop1_n = __hpvm__createNodeND(0, nop);
	// __hpvm__bindIn(nop1_n, 1 /*inSize    */, 0 /*dummy */, HPVM_NONSTREAMING);
	// __hpvm__edge(nop1_n, sqr_vect_n, HPVM_ONE_TO_ONE, 0 /*dummy*/, 1 /*inSize*/, HPVM_NONSTREAMING);
	__hpvm__bindIn(sqr_vect_n, 1 /*inSize    */, 1 /*dummy  */, HPVM_NONSTREAMING);

	__hpvm__bindIn(sqr_vect_n, 0 /*in        */, 0 /*in     */, HPVM_NONSTREAMING);
	__hpvm__bindIn(sqr_vect_n, 2 /*tmp       */, 2 /*out    */, HPVM_NONSTREAMING);
	__hpvm__bindIn(sqr_vect_n, 3 /*tmpSize   */, 3 /*outSize*/, HPVM_NONSTREAMING);
	__hpvm__bindIn(sqr_vect_n, 6 /*length    */, 4 /*length */, HPVM_NONSTREAMING);

	__hpvm__edge(sqr_vect_n, dbl_vect_n, HPVM_ONE_TO_ONE, 0 /*outSize*/, 1 /*inSize*/, HPVM_NONSTREAMING);

	__hpvm__bindIn(dbl_vect_n, 2 /*tmp       */, 0 /*in     */, HPVM_NONSTREAMING);
	__hpvm__bindIn(dbl_vect_n, 4 /*out       */, 2 /*out    */, HPVM_NONSTREAMING);
	__hpvm__bindIn(dbl_vect_n, 5 /*outSize   */, 3 /*outSize*/, HPVM_NONSTREAMING);
	__hpvm__bindIn(dbl_vect_n, 6 /*length    */, 4 /*length */, HPVM_NONSTREAMING);
	__hpvm__bindOut(dbl_vect_n, 0 /*outSize*/, 0 /*outSize*/, HPVM_NONSTREAMING);
}

struct __attribute__((__packed__)) OutStruct {
	size_t outSize;
};

struct __attribute__((__packed__)) InStruct {
	int* in; size_t inSize;
	int* tmp; size_t tmpSize;
	int* out; size_t outSize;
	size_t length;
	OutStruct outStr;
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
		length,
		{1},
	};
	void *dbl_sqr_vect_n = __hpvm__launch(HPVM_NONSTREAMING, dbl_sqr_vect, reinterpret_cast<void*>(&args));
	__hpvm__wait(dbl_sqr_vect_n);

	// Is this necessary?
	llvm_hpvm_request_mem(out, length * sizeof(int));

	for (size_t i = 0; i < length; ++i) {
		std::cout << i << ": " << out[i] << std::endl;
		assert(out[i] == 2*i*i);
	}

	llvm_hpvm_untrack_mem(in);
	llvm_hpvm_untrack_mem(tmp);
	llvm_hpvm_untrack_mem(out);

	__hpvm__cleanup();

	return 0;
}
