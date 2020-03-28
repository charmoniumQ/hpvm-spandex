#include <cassert>
#include <hpvm.h>

struct __attribute__((__packed__)) InStruct {
	int a;
	int b;
};

struct __attribute__((__packed__)) OutStruct {
	int sum;
};

void sum_leaf_fn(int a, int b) {
	__hpvm__hint(hpvm::DEVICE);
	__hpvm__return(1, a + b);
}

void sum_internal_fn(int a, int b) {
	__hpvm__hint(hpvm::CPU_TARGET);
	void* sum_leaf = __hpvm__createNodeND(0, sum_leaf_fn);
	__hpvm__bindIn(sum_leaf, 0, 0, HPVM_NONSTREAMING);
	__hpvm__bindIn(sum_leaf, 1, 1, HPVM_NONSTREAMING);
	__hpvm__bindOut(sum_leaf, 0, 0, HPVM_NONSTREAMING);	
}

void sum_root_fn(int a, int b) {
	__hpvm__hint(hpvm::CPU_TARGET);
	void* sum_internal = __hpvm__createNodeND(0, sum_internal_fn);
	__hpvm__bindIn(sum_internal, 0, 0, HPVM_STREAMING);
	__hpvm__bindIn(sum_internal, 1, 1, HPVM_STREAMING);
	__hpvm__bindOut(sum_internal, 0, 0, HPVM_STREAMING);
}

int main(int argc, char *argv[]) {
	__hpvm__init();

	auto args = new InStruct{34, 12};

	void *sum_root = __hpvm__launch(HPVM_STREAMING, sum_root_fn, (void *)args);

	__hpvm__push(sum_root, (void *)args);

	auto *ret = reinterpret_cast<struct OutStruct*>(__hpvm__pop(sum_root));

	assert(ret->sum == args->a + args->b);

	__hpvm__wait(sum_root);

	__hpvm__cleanup();

	return 0;
}
