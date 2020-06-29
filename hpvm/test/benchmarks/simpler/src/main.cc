#include <iostream>
#include <stdlib.h>
#include <cassert>
#include <hpvm.h>

struct __attribute__((__packed__)) OutStruct {
	size_t outSize;
};

struct __attribute__((__packed__)) InStruct {
	int* tmp; size_t tmpSize;
	OutStruct outStr;
};

const size_t RtmpSize = 20;

void B_c(int* tmp, [[maybe unused]] size_t tmpSize) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(0, 1, tmp);
	for (size_t i = 0; i < RtmpSize; ++i) {
		tmp[i] = i;
	}
	__hpvm__return(1, tmpSize);
}

void B(int* tmp, size_t tmpSize) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, tmp, 1, tmp);

	void* B_n = __hpvm__createNodeND(0, B_c);

	__hpvm__bindIn(B_n, 0, 0, HPVM_NONSTREAMING);
	__hpvm__bindIn(B_n, 1, 1, HPVM_NONSTREAMING);
	__hpvm__bindOut(B_n, 0, 0, HPVM_NONSTREAMING);
}

void C(int* tmp, [[maybe unsued]] size_t tmpSize) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, tmp, 0);
	int sum = 0;
	for (size_t i = 0; i < RtmpSize; ++i) {
		sum += tmp[i];
	}
	__hpvm__return(1, tmpSize + sum);
}

void A(int* tmp, size_t tmpSize) {
	__hpvm__hint(hpvm::CPU_TARGET);
	__hpvm__attributes(1, tmp, 1, tmp);

	void* B_n = __hpvm__createNodeND(0, B);
	void* C_n = __hpvm__createNodeND(0, C);

	__hpvm__bindIn(B_n, 0, 0, HPVM_NONSTREAMING);
	__hpvm__bindIn(B_n, 1, 1, HPVM_NONSTREAMING);
	__hpvm__bindIn(C_n, 0, 0, HPVM_NONSTREAMING);
	__hpvm__edge(B_n, C_n, HPVM_ONE_TO_ONE, 0, 1, HPVM_NONSTREAMING);
	__hpvm__bindOut(C_n, 0, 0, HPVM_NONSTREAMING);
}

int main(int argc, char *argv[]) {
	__hpvm__init();

	int tmp[RtmpSize];
	llvm_hpvm_track_mem(tmp, RtmpSize * sizeof(int));

	struct InStruct args {
		tmp, RtmpSize * sizeof(int),
		{1},
	};
	void *A_n = __hpvm__launch(HPVM_NONSTREAMING, A, reinterpret_cast<void*>(&args));
	__hpvm__wait(A_n);

	// Is this necessary?
	llvm_hpvm_request_mem(tmp, RtmpSize * sizeof(int));

	llvm_hpvm_untrack_mem(tmp);

	__hpvm__cleanup();

	return 0;
}
