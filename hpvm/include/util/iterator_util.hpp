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
