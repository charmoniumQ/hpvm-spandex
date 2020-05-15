#include <iostream>
#include <algorithm>
#include <stdlib.h>
#include <stdio.h>
#include <cassert>
#include <hpvm.h>

// void nop(size_t dummy) {
// 	__hpvm__hint(hpvm::CPU_TARGET);
// 	__hpvm__return(1, dummy);
// }

typedef union {
	unsigned long long data;
} data;

void dbl_sclr(data* in, size_t inSize, data* out, size_t outSize) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);
	long i = __hpvm__getNodeInstanceID_x(__hpvm__getNode());
	out[i].data = in[i].data + in[i].data;
	printf("s_ 0 store %#lx\n", (unsigned long) &out[i]);
	__hpvm__return(1, outSize);
}

void dbl_vect(data* in, size_t inSize, data* out, size_t outSize, size_t length) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);
	
	void* dbl_sclr_n = __hpvm__createNodeND(1, dbl_sclr, length);
	__hpvm__bindIn(dbl_sclr_n, 0 /*in     */, 0 /*in     */, HPVM_NONSTREAMING);
	__hpvm__bindIn(dbl_sclr_n, 1 /*inSize */, 1 /*inSize */, HPVM_NONSTREAMING);
	__hpvm__bindIn(dbl_sclr_n, 2 /*out    */, 2 /*out    */, HPVM_NONSTREAMING);
	__hpvm__bindIn(dbl_sclr_n, 3 /*outSize*/, 3 /*outSize*/, HPVM_NONSTREAMING);
	__hpvm__bindOut(dbl_sclr_n, 0 /*outSize*/, 0 /*outSize*/, HPVM_NONSTREAMING);
}

void sqr_sclr(data* in, size_t inSize, data* out, size_t outSize) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);
	long i = __hpvm__getNodeInstanceID_x(__hpvm__getNode());
	out[i].data = in[i].data * in[i].data;
	printf("s_ 1 load %#lx\n", (unsigned long) &in[i]);
	__hpvm__return(1, outSize);
}

void sqr_vect(data* in, size_t inSize, data* out, size_t outSize, size_t length) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, in, 1, out);
	
	void* sqr_sclr_n = __hpvm__createNodeND(1, sqr_sclr, length);
	__hpvm__bindIn(sqr_sclr_n, 0 /*in     */, 0 /*in     */, HPVM_NONSTREAMING);
	__hpvm__bindIn(sqr_sclr_n, 1 /*inSize */, 1 /*inSize */, HPVM_NONSTREAMING);
	__hpvm__bindIn(sqr_sclr_n, 2 /*out    */, 2 /*out    */, HPVM_NONSTREAMING);
	__hpvm__bindIn(sqr_sclr_n, 3 /*outSize*/, 3 /*outSize*/, HPVM_NONSTREAMING);
	__hpvm__bindOut(sqr_sclr_n, 0 /*outSize*/, 0 /*outSize*/, HPVM_NONSTREAMING);
}

void dbl_sqr_vect(data* in, size_t inSize, data* tmp, size_t tmpSize, data* out, size_t outSize, size_t length) {
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
	data* in; size_t inSize;
	data* tmp; size_t tmpSize;
	data* out; size_t outSize;
	size_t length;
	OutStruct outStr;
};

int main(int argc, char *argv[]) {
	__hpvm__init();

	const size_t length = 1024*4;
	data in[length];
	data tmp[length];
	data out[length];
	for (size_t i = 0; i < length; ++i) {
		in[i].data = i;
		out[i].data = 0;
		tmp[i].data = 0;
	}

	llvm_hpvm_track_mem(in, length * sizeof(data));
	llvm_hpvm_track_mem(tmp, length * sizeof(data));
	llvm_hpvm_track_mem(out, length * sizeof(data));

	struct InStruct args {
		in, length * sizeof(data),
		tmp, length * sizeof(data),
		out, length * sizeof(data),
		length,
		{1},
	};
	void *dbl_sqr_vect_n = __hpvm__launch(HPVM_NONSTREAMING, dbl_sqr_vect, reinterpret_cast<void*>(&args));
	__hpvm__wait(dbl_sqr_vect_n);

	// Is this necessary?
	llvm_hpvm_request_mem(out, length * sizeof(data));

	for (size_t i = 0; i < std::max(length, 20ul); ++i) {
		std::cout << i << ": " << out[i].data << std::endl;
		assert(out[i].data == 2*i*i);
	}

	llvm_hpvm_untrack_mem(in);
	llvm_hpvm_untrack_mem(tmp);
	llvm_hpvm_untrack_mem(out);

	__hpvm__cleanup();

	return 0;
}
