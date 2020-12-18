#pragma once
#include <string>
#pragma GCC diagnostic ignored "-Wpedantic"
#include "result.hpp"

#define DEBUGF(expr) std::cerr << __FILE__ ":" << __LINE__ << ": " #expr ": " << expr << std::endl;

using str = std::string;

template <typename T>
using Ref = std::reference_wrapper<const T>;

template <typename T>
using MutRef = std::reference_wrapper<T>;

template <typename T>
T& ptr2ref(T* t) {
	assert(t);
	return *t;
}

template <typename T>
const T& ptr2ref(const T* t) {
	assert(t);
	return *t;
}

template <typename T>
struct remove_reference_wrapper {
	typedef T type;
};
template <typename T>
struct remove_reference_wrapper<std::reference_wrapper<T>> {
	typedef T type;
};

template <typename T>
struct is_reference_wrapper {
	typedef T false_type;
};
template <typename T>
struct is_reference_wrapper<std::reference_wrapper<T>> {
	typedef T true_type;
};

template <typename T>
bool address_equality(const T& a, const T& b) { return &a == &b; }

template <typename T>
struct curried_address_equality {
	curried_address_equality(const T& _a) : a{_a} { }
	bool operator()(const T& b) {
		return &a == &b;
	}
private:
	const T& a;
};
