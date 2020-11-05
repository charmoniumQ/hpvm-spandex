#define DEBUG_TYPE "Spandex"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm_util.hpp"

BETTER_ENUM(SpandexRequestType, char, O_data, O, S, V, WTfwd, WTfwd_data, Vo, WTo, WTo_data, unassigned)

using Req = SpandexRequestType;

constexpr unsigned short LINE_SIZE = 64;
constexpr unsigned short WORD_SIZE = 8;
using WordMask = std::bitset<LINE_SIZE / WORD_SIZE>;

class HardwareParams {
public:
	bool producer;
	hpvm::Target target;
	bool owner_pred_available;
	unsigned int cache_size;
	unsigned short block_size;
};

class MemoryAccess {
public:
	const llvm::Instruction& instruction;
	const AccessKind kind;
	const Address address;
	Req req_type;
	WordMask mask;
	hpvm::Target target;
private:
	MemoryAccess(const llvm::Instruction& _instruction, AccessKind _kind, const Address& _address, hpvm::Target _target)
		: instruction{_instruction}
		, kind{_kind}
		, address{_address}
		, req_type{Req::unassigned}
		, target{_target}
	{ }

public:
	static ResultStr<MemoryAccess>
	create(const llvm::DataLayout& data_layout, const llvm::Instruction& instruction, const HardwareParams& hw_params) {
		const auto& [pointer, kind] = TRY(get_pointer_target(instruction));
		const auto& address = TRY(Address::create(data_layout, *pointer, hw_params.block_size));
		return Ok(MemoryAccess{instruction, kind, address, hw_params.target});
	}
	float criticality_weight() {
		if (target == hpvm::CPU_TARGET
		    && (kind == +AccessKind::load || kind == +AccessKind::cas || kind == +AccessKind::rmw)) {
			return 3.0;
		} else if (target == hpvm::GPU_TARGET
		           && (kind == +AccessKind::load || kind == +AccessKind::cas || kind == +AccessKind::rmw)) {
			return 2.0;
		} else {
			return 1.0;
		}
	}
};

class AccessList {
private:
	std::vector<std::reference_wrapper<MemoryAccess>> accesses;
public:
	typedef std::vector<std::reference_wrapper<MemoryAccess>>::const_iterator const_iterator;
	AccessList() { }
	void push_back(MemoryAccess& access) { accesses.push_back(access); }

};

template <typename Iterator, typename Range>
bool iterator_in_range(const Iterator it, const Range range, bool not_end = false, bool consty = true) {
	return consty
		? range.cbegin() <= it && (not_end ? it < range.cend() : it <= range.cend())
		: range .begin() <= it && (not_end ? it < range .end() : it <= range .end());
}

class BoiledDownFunction {
private:
	std::vector<MemoryAccess> accesses_in_order;
	std::unordered_map<Address, std::vector<Ref<const MemoryAccess>>> accesses_to_loc;
	std::unordered_map<Block, std::vector<Ref<const MemoryAccess>>> accesses_to_block;
	const HardwareParams& hw_params;
	BoiledDownFunction(const HardwareParams& _hw_params)
		: hw_params{_hw_params}
	{ }
public:
	static BoiledDownFunction create(const HardwareParams& hw_params) {
		return BoiledDownFunction{hw_params};
	}

	static ResultStr<BoiledDownFunction>
	create(const llvm::Function& function, const llvm::DataLayout& data_layout, const HardwareParams& hw_params) {
		BoiledDownFunction bdf{hw_params};
		if (&function.front() != &function.back()) {
			return Err("Spandex pass supports straight-line code for now. Function is not straight-line:\n" + llvm_to_str_ndbg(function));
		} else {
			const llvm::BasicBlock& basic_block = function.getEntryBlock();
			for (const llvm::Instruction& instruction : basic_block) {
				if (is_memory_access(instruction)) {
					bdf.accesses_in_order.push_back(TRY(MemoryAccess::create(data_layout, instruction, hw_params)));
					const auto& access = bdf.accesses_in_order.back();
					if (access.kind == +AccessKind::rmw || access.kind == +AccessKind::cas) {
						Err(str{"Spandex pass does not support atomics yet."});
					}
					bdf.accesses_to_loc  [access.address      ].push_back(access);
					bdf.accesses_to_block[access.address.block].push_back(access);
				}
			}
			return Ok(bdf);
		}
	}
	typedef std::vector<MemoryAccess>::const_iterator const_iterator;
	size_t n_unique_bytes(const const_iterator& begin, const const_iterator& end) const {
		// TODO: improve performance of this algorithm with a Binary Indexed Tree
		std::unordered_set<Address> cache;
		assert(iterator_in_range(begin, accesses_in_order));
		assert(iterator_in_range(end, accesses_in_order));
		for (auto it = begin; it != end; ++it) {
			// Assume access is to word
			assert(it->address.aligned(WORD_SIZE));
			for (unsigned short byte = 0; byte < WORD_SIZE; ++byte) {
				cache.insert(it->address + byte);
			}
		}
		return cache.size();
	}
	bool reuse_possible(const const_iterator& begin, const const_iterator& end) const {
		return n_unique_bytes(begin, end) < 0.75 * hw_params.cache_size;
	}
	const std::vector<Ref<const MemoryAccess>>& all_loc_conflicts(const Address& address) const {
		return const_cast<BoiledDownFunction&>(*this).accesses_to_loc[address];
	}
	const std::vector<Ref<const MemoryAccess>>& all_block_conflicts(const Block& block) const {
		return const_cast<BoiledDownFunction&>(*this).accesses_to_block[block];
	}
	bool sync_sept(const const_iterator& X, const const_iterator& Y) const {
		// Assuming no "manual" synchronization
		return false;
	}
	WordMask intra_sync_load_reuse(const const_iterator& X) const {
		assert(iterator_in_range(X, accesses_in_order));
		WordMask mask;
		std::unordered_set<Address> cache;
		for (auto it = X; it != accesses_in_order.cend(); ++it) {
			// Assume access is to word
			assert(it->address.aligned(WORD_SIZE));
			for (unsigned short byte = 0; byte < WORD_SIZE; ++byte) {
				cache.insert(it->address + byte);
			}
			if (cache.size() > 0.75 * hw_params.cache_size) {
				break;
			}
			if (it->kind == +AccessKind::load && it->address.block == X->address.block){
				mask.set(it->address.block_offset / WORD_SIZE);
			}
		}
		return mask;
	}
};

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
using SubsequentConflictsIt = LazyTransform<StaticTraceIt, std::pair<IntraSyncConflictsIt, IntraSyncConflictsIt>>;

SubsequentConflictsIt subsequent_conflicts(StaticTraceIt after_sync, const MemoryAccess& ma, const Address& address) {
	const BoiledDownFunction& current_bdf = *after_sync;
	return
		LazyTransform<StaticTraceIt, std::pair<IntraSyncConflictsIt, IntraSyncConflictsIt>>{
			after_sync + 1,
			after_sync.end(),
			[&](const BoiledDownFunction& bdf) -> std::pair<IntraSyncConflictsIt, IntraSyncConflictsIt> {
				if (&bdf == &current_bdf) {
					const std::vector<Ref<const MemoryAccess>>& block_accesses = bdf.all_loc_conflicts(ma.address);
					IntraSyncConflictsIt rest_of_block_accesses = std::find_if(block_accesses.cbegin(), block_accesses.cend(), [&](const MemoryAccess& ma2) {
						return &ma == &ma2;
					});
					return std::make_pair<IntraSyncConflictsIt, IntraSyncConflictsIt>(rest_of_block_accesses + 1, block_accesses.cend());
				} else {
					return std::make_pair<IntraSyncConflictsIt, IntraSyncConflictsIt>(bdf.all_loc_conflicts(address).cbegin(), bdf.all_loc_conflicts(address).cend());
				}
			}
		};
}

/*
SubsequentConflictsIt subsequent_block_conflicts(StaticTraceIt static_trace_after_block, std::vector<Ref<const MemoryAccess>>::const_iterator rest_of_block, const Address& address) {
	return LazyTransform<StaticTraceIt, const std::vector<Ref<const MemoryAccess>>&>{
		rest_of_static_trace,
		static_trace.end(),
		[&](const BoiledDownFunction& bdf) { return bdf.all_block_conflicts(block); }
	};
}
*/

/*
Algorithm 5: Is ownership beneficial?
*/
static bool ownership_beneficial(const MemoryAccess& X) {
	return false;
}
	/*
static bool ownership_beneficial(const Digraph<std::reference_wrapper<const BoiledDownFunction>>& bdf_dfg, const BoiledDownFunction& root) {
	for (auto bdf_it = BfsIt{bdf_dfg, root}; it != BfsIt{}; ++it) {
		const auto& bdf = *bdf_it;
		for (auto X_it = bdf.accesses_in_order.begin(); X_it != bdf.accesses_in_order.end(); ++X_it) {
			// Is ownership beneficial for X?
			MemoryAccess& X = *X_it;
			unsigned char phase = 4;
			float X_score = 0.0;
			const auto& Y_prev = X;
			for (auto Y_it = X_it+1; Y_it != bdf.accesses_in_order.end(); ++Y_it) {
				
			}
			for (auto jt = it; jt != BfsIt{}; ++jt) {

			}
		}
	}
}
*/

/*
Algorithm 6: Is shared-state beneficial?
*/
static bool shared_state_beneficial(const MemoryAccess& X) {
	return false;
}

/*
Algorithm 7: Is owner-prediction beneficial?
*/
static bool owner_pred_beneficial(const MemoryAccess& X) {
	return false;
}

static WordMask requested_words_only(const MemoryAccess& X) {
	WordMask mask;
	mask.set(X.address.block_offset / WORD_SIZE);
	return mask;
}

static WordMask intra_synch_load_reuse(const MemoryAccess& X) {
	WordMask mask;
	// TODO: do this
	return mask;
}

/*
Algorithm 1: Select load request type.
*/
static void request_type_for_load(MemoryAccess& X) {
	if (ownership_beneficial(X)) {
		X.req_type = Req::O_data;
	} else if (shared_state_beneficial(X)) {
		X.req_type = Req::S;
	} else if (owner_pred_beneficial(X)) {
		X.req_type = Req::Vo;
	} else {
		X.req_type = Req::V;
	}
}

/*
Algorithm 2: Select store request type.
*/
static void request_type_for_store(MemoryAccess& X) {
	if (ownership_beneficial(X)) {
		X.req_type = Req::O;
	} else if (owner_pred_beneficial(X)) {
		X.req_type = Req::WTo;
	} else {
		X.req_type = Req::WTfwd;
	}
}

/*
Algorithm 3: Select RMW request type
*/
static void request_type_for_rmw(MemoryAccess& X) {
	if (ownership_beneficial(X)) {
		X.req_type = Req::O_data;
	} else if (owner_pred_beneficial(X)) {
		X.req_type = Req::WTo_data;
	} else {
		X.req_type = Req::WTfwd_data;
	}
}

/*
Algorithm 4: Request-granularity selection.
*/
static void granularity_selection(MemoryAccess& X) {
	if (X.req_type == +Req::V) {
		X.mask = intra_synch_load_reuse(X);
	} else if (X.req_type == +Req::S) {
		X.mask.set();
	} else if (X.req_type == +Req::WTo || X.req_type == +Req::WTfwd || X.req_type == +Req::WTo_data || X.req_type == +Req::WTfwd_data) {
		X.mask = requested_words_only(X);
	} else if (X.req_type == +Req::O || X.req_type == +Req::O_data) {
		X.mask = intra_synch_load_reuse(X);
		if (X.mask != requested_words_only(X)) {
			X.req_type = Req::O_data;
		}
	} else {
		errs() << "Unknown request type '" << X.req_type._to_string() << "'\n";
		abort();
	}
}

static void assign_request_types(llvm::Module& module, llvm::Function& function, const llvm::Argument& argument, bool producer, hpvm::Target target, bool owner_pred_available, llvm::Pass& pass) {
	/*
	ScalarEvolution &SE =
		pass.getAnalysis<ScalarEvolutionWrapperPass>(function).getSE();
	// SE.getSmallConstantTripCount(const Loop*) is not const

	const LoopInfo &LI = pass.getAnalysis<LoopInfoWrapperPass>(function).getLoopInfo();

	AAResultsProxy& AA = pass.getAnalysis<AAResultsWrapperPass>(function).getBestAAResults();
	// AA.alias is not const.
	*/
}
