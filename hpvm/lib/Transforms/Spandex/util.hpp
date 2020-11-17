#pragma once
#include <string>
#include <regex>
#include <cxxabi.h>
#include "enum.h"
#pragma GCC diagnostic ignored "-Wpedantic"
#include "result.h"

using str = std::string;

template <typename T>
using ResultStr = Result<T, str>;

#undef TRY
#define TRY(result_expr)                                           \
    ({                                                             \
        auto res = result_expr;                                    \
        if (!res.isOk()) {                                         \
            return Err<str>(str{__FILE__ ":"} + std::to_string(__LINE__) + str{"\n    " #result_expr "\n"} + res.unwrapErr()); \
        }                                                          \
        typedef details::ResultOkType<decltype(res)>::type T;      \
        res.storage().get<T>();                                    \
    })


template <typename T>
T unwrap(ResultStr<T> result) {
	if (result.isOk()) {
		return result.unwrap();
	} else {
		std::cerr << result.unwrapErr() << std::endl;
		abort();
	}
}

str demangle(const std::string &input) {
  int status = -1;
  std::string real_input =
      input.substr(input.size() - 7) == std::string{"_cloned"}
          ? input.substr(0, input.size() - 7)
          : input;

  char *output =
      abi::__cxa_demangle(real_input.c_str(), nullptr, nullptr, &status);
  if (status == 0) {
    std::string real_output =
        std::regex_replace(output, std::regex("\\(.*\\)"), "");
    return real_output;
  } else {
    // dbgs() << "abi::__cxa_demangle returned " << status << " on " <<
    // real_input
    //        << "\n";
    return real_input;
  }
}

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

template <typename InputIterator> class IterRange {
public:
	IterRange(InputIterator __begin, InputIterator __end)
		: _begin{__begin}
		, _end{__end}
	{ }
	InputIterator& beginf() {
		return _begin;
	}
	InputIterator& endf() {
		return _end;
	}
	InputIterator begin() const {
		return _begin;
	}
	InputIterator end() const {
		return _end;
	}
private:
	InputIterator _begin;
	InputIterator _end;
};

template <typename InputIterator, typename T> class LazyTransform
  // Iterator traits - typedefs and types required to be STL compliant
	: public std::iterator<
		std::input_iterator_tag,
		T,
		typename InputIterator::difference_type,
		void,
		void
	>
{
public:
	typedef typename InputIterator::value_type input_value_type;
	typedef T output_value_type;
private:
	InputIterator it;
	InputIterator end_it;
	std::function<T(input_value_type)> fn;
public:
	LazyTransform(InputIterator _it, InputIterator _end_it, std::function<T(input_value_type)> _fn)
		: it{_it}, end_it{_end_it}, fn{_fn}
	{ }
	bool empty() const {
		return it == end_it;
	}
	LazyTransform<InputIterator, T> end() const {
		return LazyTransform<InputIterator, T>{end_it, end_it, fn};
	}
	LazyTransform<InputIterator, T> begin() const {
		return LazyTransform<InputIterator, T>{it, end_it, fn};
	}
	LazyTransform &operator++() {
		// only works in acyclic graph
		assert(!empty() && "Incremented a completed iterator");
		++it;
		return *this;
	}

	output_value_type operator*() const {
		assert(!empty() && "Accessed a completed iterator");
		return fn(*it);
	}
	output_value_type operator->() const { return **this; }
	LazyTransform operator++(int) {
		LazyTransform other = *this;
		++*this;
		return other;
	}
	LazyTransform<InputIterator, T> operator+(size_t n) {
		LazyTransform<InputIterator, T> other = *this;
		for (size_t i = 0; i < n; ++i) {
			++other;
		}
		return other;
	}
	bool operator==(const LazyTransform &other) const {
		if (empty() || other.empty()) {
			return empty() && other.empty();
		} else {
			return it == other.it && end_it == other.end_it;
		}
  }
  bool operator!=(const LazyTransform &other) const { return !(*this == other); }
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
