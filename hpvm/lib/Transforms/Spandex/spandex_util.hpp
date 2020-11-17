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
	bool owner_pred_available;
	unsigned int cache_size;
	unsigned short block_size;
	Core core;
};

class BoiledDownFunction;

class MemoryAccess {
public:
	const llvm::Instruction& instruction;
	const AccessKind kind;
	const Address address;
	const BoiledDownFunction& bdf;
	const unsigned order;
	Req req_type;
	WordMask word_mask;
private:
	MemoryAccess(const llvm::Instruction& _instruction, AccessKind _kind, const Address& _address, const BoiledDownFunction& _bdf, unsigned _order)
		: instruction{_instruction}
		, kind{_kind}
		, address{_address}
		, bdf{_bdf}
		, order{_order}
		, req_type{+Req::unassigned}
	{ }

public:
	static ResultStr<MemoryAccess>
	create(const llvm::DataLayout& data_layout, const llvm::Instruction& instruction, const HardwareParams& hw_params, const BoiledDownFunction& bdf, unsigned order) {
		const auto& [pointer, kind] = TRY(get_pointer_target(instruction));
		const auto& address = TRY(Address::create(data_layout, *pointer, hw_params.block_size));
		return Ok(MemoryAccess{instruction, kind, address, bdf, order});
	}
	float criticality_weight() const;
	bool operator==(const MemoryAccess& other) const {
		return this == &other;
	}
	bool operator!=(const MemoryAccess& other) const {
		return !(*this == other);
	}
};

template <typename Iterator, typename Range>
bool iterator_in_range(const Iterator it, const Range range, bool not_end = false, bool consty = true) {
	return consty
		? range.cbegin() <= it && (not_end ? it < range.cend() : it <= range.cend())
		: range .begin() <= it && (not_end ? it < range .end() : it <= range .end());
}

class BoiledDownFunction {
private:
	std::vector<MemoryAccess> accesses;
	std::unordered_map<Address, std::vector<Ref<const MemoryAccess>>> accesses_to_loc;
	std::unordered_map<Block, std::vector<Ref<const MemoryAccess>>> accesses_to_block;
	const HardwareParams& hw_params;
	const llvm::Function& function;
	BoiledDownFunction(const HardwareParams& _hw_params, const llvm::Function& _function)
		: hw_params{_hw_params}
		, function{_function}
	{ }
public:
	static ResultStr<BoiledDownFunction>
	create(const llvm::Function& function, const llvm::DataLayout& data_layout, const HardwareParams& hw_params) {
		BoiledDownFunction bdf {hw_params, function};
		if (&function.front() != &function.back()) {
			return Err("Spandex pass supports straight-line code for now. Function is not straight-line:\n" + llvm_to_str_ndbg(function));
		} else {
			const llvm::BasicBlock& basic_block = function.getEntryBlock();
			for (const llvm::Instruction& instruction : basic_block) {
				if (is_memory_access(instruction)) {
					{
						const MemoryAccess access = TRY(MemoryAccess::create(data_layout, instruction, hw_params, bdf, bdf.accesses.size()));
						if (access.kind == +AccessKind::rmw || access.kind == +AccessKind::cas) {
							Err(str{"Spandex pass does not support atomics yet."});
						}
						bdf.accesses.push_back(access);
					}

					const auto& access_ref = bdf.accesses.back();
					bdf.accesses_to_loc  [access_ref.address      ].push_back(access_ref);
					bdf.accesses_to_block[access_ref.address.block].push_back(access_ref);
				}
			}
			return Ok(bdf);
		}
	}
	void emulate_cache(std::unordered_set<Address>& cache, const MemoryAccess& begin, const MemoryAccess& end) const {
		// TODO: this algorithm overestimates the cache, because it doesn't consider aliasing between arguments
		// TODO: improve performance of this algorithm with a Binary Indexed Tree
		auto begin_it = std::find_if(accesses.cbegin(), accesses.cend(), curried_address_equality<MemoryAccess>(begin));
		auto end_it   = std::find_if(accesses.cbegin(), accesses.cend(), curried_address_equality<MemoryAccess>(end  ));
		for (auto it = begin_it; it != end_it; ++it) {
			// Assume access is to word
			assert(it->address.aligned(WORD_SIZE));
			for (unsigned short byte = 0; byte < WORD_SIZE; ++byte) {
				cache.insert(it->address + byte);
			}
		}
	}
	const std::vector<Ref<const MemoryAccess>>& all_loc_conflicts(const Address& address) const {
		return const_cast<BoiledDownFunction&>(*this).accesses_to_loc[address];
	}
	const std::vector<Ref<const MemoryAccess>>& all_block_conflicts(const Block& block) const {
		return const_cast<BoiledDownFunction&>(*this).accesses_to_block[block];
	}
	/*
	WordMask intra_sync_load_reuse(const const_iterator& X) const {
		assert(iterator_in_range(X, accesses));
		WordMask mask;
		std::unordered_set<Address> cache;
		for (auto it = X; it != accesses.cend(); ++it) {
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
	*/
	const std::vector<MemoryAccess>& get_all_accesses() const { return accesses; }
	std::vector<MemoryAccess>& get_all_accesses() { return accesses; }
	const llvm::Function& get_function() const { return function; }
	const HardwareParams& get_hw_params() const { return hw_params; }
	Core get_core() const { return hw_params.core; }
	bool operator==(const BoiledDownFunction& other) const {
		return this == &other;
	}
	bool operator!=(const BoiledDownFunction& other) const {
		return !(*this == other);
	}
};

float MemoryAccess::criticality_weight() const {
		if (bdf.get_core().target == hpvm::CPU_TARGET
		    && (kind == +AccessKind::load || kind == +AccessKind::cas || kind == +AccessKind::rmw)) {
			return 3.0;
		} else if (bdf.get_core().target == hpvm::GPU_TARGET
		           && (kind == +AccessKind::load || kind == +AccessKind::cas || kind == +AccessKind::rmw)) {
			return 2.0;
		} else {
			return 1.0;
		}
	}

typedef std::pair<Ref<BoiledDownFunction>, unsigned> BdfDfgNode;

struct BdfDfgEquality {
	bool operator()(const BdfDfgNode& a, const BdfDfgNode& b) const {
		return a.first.get() == b.first.get() && a.second == b.second;
	}
};

struct BdfDfgHasher {
	std::size_t operator()(const BdfDfgNode& a) const noexcept {
		return reinterpret_cast<size_t>(&a.first) ^ a.second;
	}
};

typedef Digraph<BdfDfgNode, BdfDfgHasher, BdfDfgEquality> BdfDfg;

BfsIt<BdfDfgNode, BdfDfg>
get_static_trace(const BdfDfg& dfg, const MemoryAccess& X) {
	const auto& argument = ptr2ref<llvm::Argument>(llvm::dyn_cast<llvm::Argument>(&X.address.block.segment.base));
	return BfsIt<BdfDfgNode, BdfDfg>{
		dfg,
		BdfDfgNode(std::cref(X.bdf), argument.getArgNo())
	};
}

bool reuse_possible(
					const BdfDfg& dfg,
					const MemoryAccess& begin,
					const MemoryAccess& end
					) {
	std::unordered_set<Address> cache;
	for (auto [bdf, arg_no] : get_static_trace(dfg, begin)) {
		if (bdf.get().get_core() == begin.bdf.get_core()) {
			bdf.get().emulate_cache(cache, begin, end);
		}
	}
	return cache.size() < begin.bdf.get_hw_params().cache_size * 0.75;
}

/*
Algorithm 5: Is ownership beneficial?
*/
static bool ownership_beneficial(const BdfDfg& dfg, const MemoryAccess& X) {
	char phase = 4;
	float X_score = 0.0;

	// Everything in the same BDF is not sync seperated and same core
	// Therefore the algoirhtm simply sets Y_prev to the last conflicting access in the same BDF
	const MemoryAccess* Y_prev = &(X.bdf.all_loc_conflicts(X.address).end() - 1)->get();

	for (auto [bdf, arg_no] : get_static_trace(dfg, X)) {
		const auto& function = bdf.get().get_function();
		assert(arg_no < function.arg_size());
		const auto& equiv_argument = ptr2ref<llvm::Argument>(function.arg_begin() + arg_no);
		// equiv_address in bdf is aliased to X.address in X.bdf
		const Address equiv_address = X.address.rebase(equiv_argument);
		bool sync_sept = true;
		for (const MemoryAccess& Y : bdf.get().all_loc_conflicts(equiv_address)) {
			if (Y_prev->bdf.get_core() != Y.bdf.get_core() || sync_sept) {
				phase--;
				if (phase < 0 || (Y_prev->bdf.get_core() == Y.bdf.get_core() && reuse_possible(dfg, X, Y))) {
					break;
				}
				float Y_val = phase * Y.criticality_weight();
				if (Y_prev->bdf.get_core() == Y.bdf.get_core()) {
					X_score += Y_val;
				} else {
					X_score -= Y_val;
				}
			}
			if (sync_sept) {
				// sync_sept should only be true fir the first iteration of the new bdf
				sync_sept = false;
			}
			Y_prev = &Y;
		}
	}
	return X_score > 0;
}

/*
Algorithm 6: Is shared-state beneficial?
*/
static bool shared_state_beneficial(const BdfDfg& dfg, const MemoryAccess& X) {
	if (X.bdf.get_core().target == hpvm::GPU_TARGET) {
		return false;
	}

	// Everything in the same BDF is not sync seperated and same core
	// Therefore the algoirhtm simply sets Y_prev to the last conflicting access in the same BDF
	const MemoryAccess* Y_prev = &(X.bdf.all_block_conflicts(X.address.block).end() - 1)->get();

	for (auto [bdf, arg_no] : get_static_trace(dfg, X)) {
		const auto& function = bdf.get().get_function();
		assert(arg_no < function.arg_size());
		const auto& equiv_argument = ptr2ref<llvm::Argument>(function.arg_begin() + arg_no);
		// equiv_address in bdf is aliased to X.address in X.bdf
		const Address equiv_address = X.address.rebase(equiv_argument);
		bool sync_sept = true;
		for (const MemoryAccess& Y : bdf.get().all_block_conflicts(equiv_address.block)) {
			if (Y_prev->bdf.get_core() != Y.bdf.get_core() || sync_sept) {
				if (Y.kind == +AccessKind::load && X.bdf.get_core() == Y.bdf.get_core()) {
					return true;
				}
				if (Y.kind == +AccessKind::store && X.bdf.get_core() != Y.bdf.get_core()) {
					return false;
				}
			}
			Y_prev = &Y;
			if (sync_sept) {
				// sync_sept should only be true fir the firs titeration of the new bdf
				sync_sept = false;
			}
		}
	}
	return false;
}

/*
Algorithm 7: Is owner-prediction beneficial?
*/
static bool owner_pred_beneficial(const BdfDfg& dfg, const MemoryAccess& X) {
	char phase = 4;
	float X_score = 0.0;
	const auto* conflicts = &X.bdf.all_loc_conflicts(X.address);
	auto X_prev = std::find_if(conflicts->cbegin(), conflicts->cend(), curried_address_equality<MemoryAccess>(X));
	auto Y = X_prev;
	bool first_time = true;

	for (auto [bdf, arg_no] : get_static_trace(reverse<BdfDfgNode, BdfDfg>(dfg), X)) {
		const auto& function = bdf.get().get_function();
		assert(arg_no < function.arg_size());
		const auto& equiv_argument = ptr2ref<llvm::Argument>(function.arg_begin() + arg_no);
		// equiv_address in bdf is aliased to X.address in X.bdf
		const Address equiv_address = X.address.rebase(equiv_argument);

		if (!first_time) {
			conflicts = &bdf.get().all_loc_conflicts(equiv_address);
			Y = conflicts->cend();
			first_time = false;
		}

		while (Y != conflicts->cbegin()) {
			const auto Y_prev = Y - 1;
			if (X.bdf.get_core() == Y->get().bdf.get_core() && X.kind == Y->get().kind) {
				phase--;
				if (phase < 0) {
					goto outer_break;
				}
				float Y_val = phase;
				if (Y_prev->get().bdf.get_core() == X_prev->get().bdf.get_core()) {
					X_score += Y_val;
				} else {
					X_score -= Y_val;
				}
				Y = Y_prev;
			}
		}
	}

 outer_break:
	return X_score > 0;
}

static WordMask requested_words_only(const MemoryAccess& X) {
	WordMask mask;
	mask.set(X.address.block_offset / WORD_SIZE);
	return mask;
}

static WordMask intra_synch_load_reuse(const MemoryAccess& X) {
	WordMask mask;
	// TODO: do this
	abort();
	return mask;
}

static Req assign_request_type(const BdfDfg& dfg, const MemoryAccess& X) {
	switch (X.kind) {
	case +AccessKind::load: {
		/*
		  Algorithm 1: Select load request type.
		*/
		if (ownership_beneficial(dfg, X)) {
			return Req::O_data;
		} else if (shared_state_beneficial(dfg, X)) {
			return Req::S;
		} else if (owner_pred_beneficial(dfg, X)) {
			return Req::Vo;
		} else {
			return Req::V;
		}
	}
	case +AccessKind::store: {
		/*
		  Algorithm 2: Select store request type.
		*/
		if (ownership_beneficial(dfg, X)) {
			return Req::O;
		} else if (owner_pred_beneficial(dfg, X)) {
			return Req::WTo;
		} else {
			return Req::WTfwd;
		}
	}
	case +AccessKind::rmw:
	case +AccessKind::cas: {
		/*
		  Algorithm 3: Select RMW request type
		*/
		if (ownership_beneficial(dfg, X)) {
			return Req::O_data;
		} else if (owner_pred_beneficial(dfg, X)) {
			return Req::WTo_data;
		} else {
			return Req::WTfwd_data;
		}
	}
	default:
		errs() << "Unknown AccessKind " << X.kind._to_string() << "\n";
		abort();
	}
}

/*
Algorithm 4: Request-granularity selection.
*/
static std::pair<WordMask, Req> select_granularity(const MemoryAccess& X) {
	if (X.req_type == +Req::V) {
		return std::pair<WordMask, Req>(intra_synch_load_reuse(X), X.req_type);
	} else if (X.req_type == +Req::S) {
		WordMask word_mask;
		word_mask.set();
		return std::pair<WordMask, Req>(word_mask, X.req_type);
	} else if (X.req_type == +Req::WTo || X.req_type == +Req::WTfwd || X.req_type == +Req::WTo_data || X.req_type == +Req::WTfwd_data) {
		return std::pair<WordMask, Req>(requested_words_only(X), X.req_type);
	} else if (X.req_type == +Req::O || X.req_type == +Req::O_data) {
		WordMask word_mask {intra_synch_load_reuse(X)};
		if (word_mask != requested_words_only(X)) {
			return std::pair<WordMask, Req>(word_mask, +Req::O_data);
		} else {
			return std::pair<WordMask, Req>(word_mask, X.req_type);
		}
	} else {
		errs() << "Unknown request type '" << X.req_type._to_string() << "'\n";
		abort();
	}
}

 static void spandex_annotate(const llvm::Module& module, const Digraph<Port>& mem_comm_dfg) {
	std::vector<BoiledDownFunction> bdfs;
	llvm::DataLayout data_layout {&module};
	HardwareParams hp;
	BdfDfg bdf_mem_comm_dfg = map_graph<Port, BdfDfgNode, Digraph<Port>, BdfDfg>(mem_comm_dfg, [&](const Port& port) {
		{
			const auto& function = ptr2ref<llvm::Function>(port.N.getFuncPointer());
			auto bdf_result = BoiledDownFunction::create(function, data_layout, hp);
			if (bdf_result.isOk()) {
				bdfs.push_back(bdf_result.unwrap());
			} else {
				std::cerr << bdf_result.unwrapErr() << std::endl;
				abort();
			}
		}
		unsigned int pos = port.pos;
		return BdfDfgNode{bdfs.back(), std::move(pos)};
	});
	for (auto& bdf : bdfs) {
		for (auto& ma : bdf.get_all_accesses()) {
			if (llvm::isa<llvm::Argument>(ma.address.block.segment.base)) {
				ma.req_type = assign_request_type(bdf_mem_comm_dfg, ma);
				auto word_mask_x_req_type = select_granularity(ma);
				ma.word_mask = word_mask_x_req_type.first;
				ma.req_type = word_mask_x_req_type.second;
			}
		}
	}
}

/*
static void assign_request_types(llvm::Module& module, llvm::Function& function, const llvm::Argument& argument, bool producer, hpvm::Target target, bool owner_pred_available, llvm::Pass& pass) {
	ScalarEvolution &SE =
		pass.getAnalysis<ScalarEvolutionWrapperPass>(function).getSE();
	// SE.getSmallConstantTripCount(const Loop*) is not const

	const LoopInfo &LI = pass.getAnalysis<LoopInfoWrapperPass>(function).getLoopInfo();

	AAResultsProxy& AA = pass.getAnalysis<AAResultsWrapperPass>(function).getBestAAResults();
	// AA.alias is not const.
}
*/
