#include <iostream>
#include <stdlib.h>
#include <cassert>
#include <hpvm.h>

void sum_leaf_fn(size_t size, int* a, int* b) {
	__hpvm__hint(hpvm::CPU_TARGET);
	auto c =
		b;
		// reinterpret_cast<int*>(malloc(size*sizeof(int)));
	for (size_t i = 0; i < size; ++i) {
		c[i] = a[i] + b[i];
	}
	__hpvm__return(1, c);
}

void sum_root_fn(size_t size, int* a, int* b) {
	__hpvm__hint(hpvm::CPU_TARGET);
	void* sum_internal = __hpvm__createNodeND(0, sum_leaf_fn);
	__hpvm__bindIn(sum_internal, 0, 0, HPVM_STREAMING);
	__hpvm__bindIn(sum_internal, 1, 1, HPVM_STREAMING);
	__hpvm__bindIn(sum_internal, 2, 2, HPVM_STREAMING);
	__hpvm__bindOut(sum_internal, 0, 0, HPVM_STREAMING);
}

struct __attribute__((__packed__)) InStruct {
	size_t size;
	int* a;
	int* b;
};


struct __attribute__((__packed__)) OutStruct {
	int* c;
};

int main(int argc, char *argv[]) {
	__hpvm__init();

	const size_t size = 10;
	int a[size];
	int b[size];
	for (size_t i = 0; i < size; ++i) {
		a[i] = i*i - i;
		b[i] = i;
	}

	llvm_hpvm_track_mem(a, size*sizeof(int));
	llvm_hpvm_track_mem(b, size*sizeof(int));
	struct InStruct args {size, a, b};
	void *sum_root = __hpvm__launch(HPVM_STREAMING, sum_root_fn, reinterpret_cast<void*>(&args));
	__hpvm__push(sum_root, reinterpret_cast<void*>(&args));
	int *c = reinterpret_cast<struct OutStruct*>(__hpvm__pop(sum_root))->c;
	llvm_hpvm_request_mem(c, size*sizeof(int));

	for (size_t i = 0; i < size; ++i) {
		std::cout << c[i] << std::endl;
		assert(c[i] == i*i);
	}

	llvm_hpvm_untrack_mem(a);
	llvm_hpvm_untrack_mem(b);

	__hpvm__wait(sum_root);

	__hpvm__cleanup();

	return 0;
}
