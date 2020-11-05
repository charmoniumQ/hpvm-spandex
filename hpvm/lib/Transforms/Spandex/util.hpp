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

template <typename T> struct remove_reference_wrapper { typedef T type; };
template <typename T> struct remove_reference_wrapper<std::reference_wrapper<T>> { typedef T type; };

template <typename InputIterator> class ConcatN
  // Iterator traits - typedefs and types required to be STL compliant
	: public std::iterator<
		std::input_iterator_tag,
		typename InputIterator::value_type,
		typename InputIterator::difference_type,
		typename InputIterator::pointer,
		typename InputIterator::reference
	>
{
private:
	std::vector<std::pair<InputIterator, InputIterator>> its;
	typename std::vector<std::pair<InputIterator, InputIterator>>::iterator it_it;
	void ensure_invariant() {
		while (it_it != its.end() && it_it->first == it_it->second) {
			++it_it;
		}
	}
	void assert_invariant() {
		assert((it_it != its.end()) == (it_it->first == it_it->second));
	}
public:
	typedef typename InputIterator::reference reference;
	ConcatN(std::vector<std::pair<InputIterator, InputIterator>> _its)
		: its{_its}, it_it{its.begin()}
	{
		ensure_invariant();
	}
	ConcatN &operator++() {
		// only works in acyclic graph
		assert(!empty() && "Incremented a completed iterator");
		++it_it->first;
		ensure_invariant();
		return *this;
	}
	bool empty() const {
		assert_invariant();
		return it_it != its.end();
	}

	/*
	  Above methods guarantee the invariant (when assertions are checked).
	  Below methods call above methods.
	 */

	const reference operator*() const {
		assert(!empty() && "Accessed a completed iterator");
		return *it_it->first;
	}
	reference operator*() {
		assert(!empty() && "Accessed a completed iterator");
		return *it_it->first;
	}
	reference operator->() { return **this; }
	const reference operator->() const { return **this; }
	ConcatN operator++(int) {
		ConcatN other = *this;
		++*this;
		return other;
	}
	ConcatN<InputIterator> operator+(size_t n) {
		ConcatN<InputIterator> other = *this;
		for (size_t i = 0; i < n; ++i) {
			++other;
		}
		return other;
	}
	bool operator==(const ConcatN &other) const {
		if (empty() || other.empty()) {
			return empty() && other.empty();
		} else {
			return it_it == other.it_it && its == other.its;
	  }
	}
	bool operator!=(const ConcatN &other) const { return !(*this == other); }
	ConcatN<InputIterator> end() const {
		std::vector<std::pair<InputIterator, InputIterator>> end_its;
		std::transform(its.cbegin(), its.cend(), end_its.begin(), [](const std::pair<InputIterator, InputIterator>& pair) {
			return std::make_pair<InputIterator, InputIterator>(pair.second, pair.second);
		});
		return ConcatN<InputIterator>{end_its};
	}
};

/*
template <typename InputIterator1, typename InputIterator2> class ConcatTwo
  // Iterator traits - typedefs and types required to be STL compliant
	: public std::iterator<
		std::input_iterator_tag,
		typename InputIterator1::value_type,
		typename InputIterator1::difference_type,
		typename InputIterator1::pointer,
		typename InputIterator1::reference
	>
{
private:
	InputIterator1 it1;
	InputIterator1 end1;
	InputIterator2 it2;
	InputIterator2 end2;
	bool on_second;
	void ensure_invariant() {
		on_second = it1 == end1;
	}
	void assert_invariant() {
		assert(on_second == (it1 == end1));
	}
public:
	static_assert(std::is_same<typename InputIterator1::reference, typename InputIterator2::reference>::value);
	typedef typename InputIterator1::reference reference;
	ConcatTwo(InputIterator1 _it1, InputIterator1 _end1, InputIterator1 _it2, InputIterator2 _end2)
		: it1{_it1}, end1{_end1}, it2{_it2}, end2{_end2}, on_second{false}
	{
		ensure_invariant();
	}
	ConcatTwo &operator++() {
		// only works in acyclic graph
		assert(!empty() && "Incremented a completed iterator");
		if (!on_second) {
			++it1;
			ensure_invariant();
		} else {
			++it2;
		}
		return *this;
	}
	bool empty() const {
		assert_invariant();
		return it2 == end2;
	}

	/*
	  Above methods guarantee the invariant (when assertions are checked).
	  Below methods call above methods.
*//*

	typename std::add_const<reference>::type operator*() const {
		assert(!empty() && "Accessed a completed iterator");
		if (!on_second) {
			return *it1;
		} else {
			return *it2;
		}
	}
	reference operator*() {
		assert(!empty() && "Accessed a completed iterator");
		if (!on_second) {
			return *it1;
		} else {
			return *it2;
		}
	}
	reference operator->() { return **this; }
	typename std::add_const<reference>::type operator->() const { return **this; }
	ConcatTwo operator++(int) {
		ConcatTwo other = *this;
		++*this;
		return other;
	}
	ConcatTwo<InputIterator1, InputIterator2> operator+(size_t n) {
		ConcatTwo<InputIterator1, InputIterator2> other = *this;
		for (size_t i = 0; i < n; ++i) {
			++other;
		}
		return other;
	}
	bool operator==(const ConcatTwo &other) const {
		if (empty() || other.empty()) {
			return empty() && other.empty();
		} else {
			return it1 == other.it1 && end2 == other.end2 && it1 == other.it1 && end2 == other.end2;
		}
  }
  bool operator!=(const ConcatTwo &other) const { return !(*this == other); }
	ConcatTwo<InputIterator1, InputIterator2> end() const {
		return ConcatTwo<InputIterator1, InputIterator2>{end1, end1, end2, end2};
	}
};
*/

/*
template <typename InputIterator> class Prepend
// Iterator traits - typedefs and types required to be STL compliant
	: public std::iterator<
		std::input_iterator_tag,
		typename InputIterator::value_type,
		typename InputIterator::difference_type,
		typename InputIterator::pointer,
		typename InputIterator::reference
	>
{
public:
	typedef typename InputIterator::reference reference;
	typedef typename InputIterator::value_type value_type;
private:
	value_type elem;
	bool first;
	InputIterator it;
	InputIterator end_it;
public:
	Prepend(value_type elem, InputIterator _it, InputIterator _end_it)
		: elem{elem}, first{true}, it{_it}, end_it{_end_it}
	{ }
	Prepend &operator++() {
		// only works in acyclic graph
		assert(!empty() && "Incremented a completed iterator");
		if (first) {
			first = false;
		} else {
			end_it++;
		}
		return *this;
	}
	bool empty() const {
		return it == end_it;
	}

	/*
	  Above methods guarantee the invariant (when assertions are checked).
	  Below methods call above methods.
*//*

	typename std::add_const<reference>::type operator*() const {
		assert(!empty() && "Accessed a completed iterator");
		if (first) {
			return elem;
		} else {
			return *it;
		}
	}
	reference operator*() {
		assert(!empty() && "Accessed a completed iterator");
		if (first) {
			return elem;
		} else {
			return *it;
		}
	}
	reference operator->() { return **this; }
	typename std::add_const<reference>::type operator->() const { return **this; }
	Prepend operator++(int) {
		Prepend<InputIterator> other = *this;
		++*this;
		return other;
	}
	Prepend operator+(size_t n) {
		Prepend<InputIterator> other = *this;
		for (size_t i = 0; i < n; ++i) {
			++other;
		}
		return other;
	}
	bool operator==(const Prepend &other) const {
		if (empty() || other.empty()) {
			return empty() && other.empty();
		} else {
			return it == other.it;
		}
  }
  bool operator!=(const Prepend &other) const { return !(*this == other); }
	Prepend<InputIterator> end() const {
		return Prepend<InputIterator>{elem, end_it, end_it};
	}
};
*/

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
