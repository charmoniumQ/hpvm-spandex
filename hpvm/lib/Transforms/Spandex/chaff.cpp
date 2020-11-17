#if false

using StaticTraceIt = LazyTransform<BfsIt<Ref<llvm::DFNode>>, BoiledDownFunction>;

StaticTraceIt get_static_trace(
	const Digraph<Ref<llvm::DFNode>>& dfg,
	const llvm::DFNode& root,
	const llvm::DataLayout& data_layout,
	const HardwareParams& gpu_params,
	const HardwareParams& cpu_params
) {
	return LazyTransform<BfsIt<Ref<llvm::DFNode>>, BoiledDownFunction>{
		BfsIt<Ref<llvm::DFNode>>{dfg, root},
		BfsIt<Ref<llvm::DFNode>>{},
		[&](const llvm::DFNode& node) -> BoiledDownFunction {
			if (node.isRoot()) {
				return BoiledDownFunction::create(cpu_params);
			} else {
				const auto& function = ptr2ref<llvm::Function>(node.getFuncPointer());
				return BoiledDownFunction::create(function, data_layout, node.getTargetHint() == hpvm::GPU_TARGET ? gpu_params : cpu_params).unwrap();
			}
		}
	};
}

using IntraSyncConflictsIt = typename std::vector<Ref<const MemoryAccess>>::const_iterator;
using SubsequentConflictsIt = LazyTransform<StaticTraceIt, IterRange<IntraSyncConflictsIt>>;

SubsequentConflictsIt subsequent_conflicts(StaticTraceIt after_sync, const MemoryAccess& ma) {
	const BoiledDownFunction& current_bdf = *after_sync;
	return
		LazyTransform<StaticTraceIt, IterRange<IntraSyncConflictsIt>>{
			after_sync + 1,
			after_sync.end(),
			[&](const BoiledDownFunction& bdf) -> IterRange<IntraSyncConflictsIt> {
				if (&bdf == &current_bdf) {
					const std::vector<Ref<const MemoryAccess>>& block_accesses = bdf.all_loc_conflicts(ma.address);
					IntraSyncConflictsIt rest_of_block_accesses = std::find_if(block_accesses.cbegin(), block_accesses.cend(), [&](const MemoryAccess& ma2) {
						return &ma == &ma2;
					});
					return IterRange<IntraSyncConflictsIt>(rest_of_block_accesses + 1, block_accesses.cend());
				} else {
					return IterRange<IntraSyncConflictsIt>(bdf.all_loc_conflicts(ma.address).cbegin(), bdf.all_loc_conflicts(ma.address).cend());
				}
			}
		};
}

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
	std::vector<IterRange<InputIterator>> its;
	typename std::vector<IterRange<InputIterator>>::iterator it_it;
	void ensure_invariant() {
		while (it_it != its.end() && it_it->beginf() == it_it->endf()) {
			++it_it;
		}
	}
	void assert_invariant() {
		assert((it_it != its.end()) == (it_it->beginf() == it_it->endf()));
	}
public:
	typedef typename InputIterator::reference reference;
	ConcatN(std::vector<IterRange<InputIterator>> _its)
		: its{_its}, it_it{its.begin()}
	{
		ensure_invariant();
	}
	ConcatN &operator++() {
		// only works in acyclic graph
		assert(!empty() && "Incremented a completed iterator");
		++it_it->beginf();
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
		return *it_it->beginf();
	}
	reference operator*() {
		assert(!empty() && "Accessed a completed iterator");
		return *it_it->beginf();
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
*/

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
*/

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



	IterRange<std::vector<const MemoryAccess&>::const_iterator> subsequent_conflicts_within_bdf() {
		assert(bdf);

		// Get all conflicts for this address
		std::vector<Ref<const MemoryAccess>> accesses_to_loc = bdf.all_loc_conflicts(address);

		// Start where this one is
		auto start = std::find(accessess_to_loc.cbegin(), accesses_to_loc.cend(), *this);

		return IterRange<std::vector<const MemoryAccess&>::const_iterator>{start, accesses_to_loc.cend()};
	}

	IterRange<std::vector<const MemoryAccess&>::const_iterator> subsequent_block_conflicts_within_bdf() {
		assert(bdf);

		// Get all conflicts for this address
		std::vector<Ref<const MemoryAccess>> accesses_to_block = bdf.all_block_conflicts(address);

		// Start where this one is
		auto start = std::find(accessess_to_block.cbegin(), accesses_to_block.cend(), *this);

		return IterRange<std::vector<const MemoryAccess&>::const_iterator>{start, accesses_to_block.cend()};
	}

#endif
