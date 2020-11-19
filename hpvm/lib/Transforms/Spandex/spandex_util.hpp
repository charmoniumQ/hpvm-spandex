#include <iomanip>
#define DEBUG_TYPE "Spandex"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm_util.hpp"

#define NO_COMMIT(block) block

BETTER_ENUM(SpandexRequestType, char, O_data, O, S, V, WTfwd, WTfwd_data, Vo, WTo, WTo_data, unassigned)

using Req = SpandexRequestType;

constexpr unsigned short BYTE_SIZE = 8;
constexpr unsigned short LINE_SIZE = 64 * BYTE_SIZE;
constexpr unsigned short WORD_SIZE = 8 * BYTE_SIZE;
using WordMask = std::bitset<LINE_SIZE / WORD_SIZE>;

class HardwareParams {
public:
	HardwareParams() = delete;
	bool producer;
	bool owner_pred_available;
	unsigned int cache_size;
	unsigned short block_size;
	Core core;
};

class BoiledDownFunction;

class MemoryAccess {
public:
	MemoryAccess() = delete;
	const llvm::Instruction& instruction;
	AccessKind kind;
	Address address;
	const BoiledDownFunction& bdf;
	unsigned order;
	Req req_type;
	WordMask word_mask;
private:
	MemoryAccess(const llvm::Instruction& _instruction, AccessKind _kind, Address _address, const BoiledDownFunction& _bdf, unsigned _order)
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
		auto [pointer, kind] = TRY(get_pointer_target(instruction));
		Address address = TRY(Address::create(data_layout, pointer.get(), hw_params.block_size));
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
	std::unordered_map<Address, std::vector<Ref<MemoryAccess>>> accesses_to_loc;
	std::unordered_map<Block, std::vector<Ref<MemoryAccess>>> accesses_to_block;
	const HardwareParams& hw_params;
	const llvm::Function& function;
	BoiledDownFunction(const HardwareParams& _hw_params, const llvm::Function& _function)
		: hw_params{_hw_params}
		, function{_function}
	{ }
public:
	BoiledDownFunction() = delete;
	BoiledDownFunction& operator=(const BoiledDownFunction&) = delete;
	BoiledDownFunction(const BoiledDownFunction&) = delete;
	BoiledDownFunction& operator=(const BoiledDownFunction&&) = delete;
	BoiledDownFunction(BoiledDownFunction&& other)
		: accesses{std::move(other.accesses)}
		, accesses_to_loc{std::move(other.accesses_to_loc)}
		, accesses_to_block{std::move(other.accesses_to_block)}
		, hw_params{other.hw_params}
		, function{other.function}
	{ }
	
	BoiledDownFunction(const llvm::Function& function, const llvm::DataLayout& data_layout, const HardwareParams& hw_params)
		: BoiledDownFunction{hw_params, function}
	{
		if (&function.front() != &function.back()) {
			//return Err("Spandex pass supports straight-line code for now. Function is not straight-line:\n" + llvm_to_str_ndbg(function));
			std::cerr << "Spandex pass supports straight-line code for now. Function is not straight-line:\n" + llvm_to_str_ndbg(function);
			abort();
		} else {
			const llvm::BasicBlock& basic_block = function.getEntryBlock();
			for (const llvm::Instruction& instruction : basic_block) {
				if (is_memory_access(instruction)) {
					ResultStr<MemoryAccess> access = MemoryAccess::create(data_layout, instruction, hw_params, *this, this->accesses.size());
					if (access.isErr()) {
						std::cerr << access.unwrapErr() << std::endl;
						abort();
					} else if (access.unwrap().kind == +AccessKind::rmw || access.unwrap().kind == +AccessKind::cas) {
						std::cerr << "Spandex pass does not support atomics yet.\n";
						abort();
						//return Err(str{"Spandex pass does not support atomics yet."});
					} else {
						this->accesses.push_back(access.unwrap());
					}
				}
			}
			for (const MemoryAccess& access : this->accesses) {
				this->accesses_to_loc  [access.address      ].emplace_back(access);
				this->accesses_to_block[access.address.block].emplace_back(access);
			}
			//return Ok(std::move(bdf));
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
	const std::vector<Ref<MemoryAccess>>& all_loc_conflicts(const Address& address) const {
		return const_cast<BoiledDownFunction&>(*this).accesses_to_loc[address];
	}
	const std::vector<Ref<MemoryAccess>>& all_block_conflicts(const Block& block) const {
		return const_cast<BoiledDownFunction&>(*this).accesses_to_block[block];
	}
	WordMask intra_synch_load_reuse(const MemoryAccess& X) const {
		WordMask mask;
		std::unordered_set<Address> cache;
		intra_synch_load_reuse(X, mask, cache);
		return mask;
	}
	void intra_synch_load_reuse(const MemoryAccess& X, WordMask& mask, std::unordered_set<Address>& cache) const {
		assert(&X.bdf == this);
		auto accesses_to_block = all_block_conflicts(X.address.block);
		auto it = std::find_if(accesses_to_block.cbegin(), accesses_to_block.cend(), curried_address_equality<MemoryAccess>(X));
		assert(it != accesses_to_block.cend());
		for (; it != accesses_to_block.cend(); ++it) {
			// Assume access is to word
			assert(it->get().address.aligned(WORD_SIZE));
			cache.insert(it->get().address);
			if (cache.size() > 0.75 * hw_params.cache_size) {
				break;
			}
			if (it->get().kind == +AccessKind::load && it->get().address.block == X.address.block) {
				size_t word_index = it->get().address.block_offset / WORD_SIZE;
				assert(word_index < mask.size());
				mask.set(word_index);
			}
		}
	}
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
static bool owner_pred_beneficial(const BdfDfg&, const BdfDfg& reverse_dfg, const MemoryAccess& X) {
	char phase = 4;
	float X_score = 0.0;
	const auto* conflicts = &X.bdf.all_loc_conflicts(X.address);
	auto X_prev = std::find_if(conflicts->cbegin(), conflicts->cend(), curried_address_equality<MemoryAccess>(X));
	auto Y = X_prev;
	bool first_time = true;

	for (auto [bdf, arg_no] : get_static_trace(reverse_dfg, X)) {
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
			auto Y_prev = Y - 1;
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
	size_t word_index = X.address.block_offset / WORD_SIZE;
	assert(word_index < mask.size());
	mask.set(word_index);
	return mask;
}

static Req assign_request_type(const BdfDfg& dfg, const BdfDfg& reverse_dfg, const MemoryAccess& X) {
	switch (X.kind) {
	case +AccessKind::load: {
		/*
		  Algorithm 1: Select load request type.
		*/
		if (ownership_beneficial(dfg, X)) {
			return Req::O_data;
		} else if (shared_state_beneficial(dfg, X)) {
			return Req::S;
		} else if (owner_pred_beneficial(dfg, reverse_dfg, X)) {
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
		} else if (owner_pred_beneficial(dfg, reverse_dfg, X)) {
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
		} else if (owner_pred_beneficial(dfg, reverse_dfg, X)) {
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

WordMask full_block_mask() {
	WordMask mask;
	mask.set();
	return mask;
}

WordMask inter_synch_load_reuse(const BdfDfg& dfg, const MemoryAccess& X) {
	const llvm::Value& orig_base = X.address.block.segment.base;

	std::unordered_set<Address> cache;
	WordMask mask;

	for (auto [bdf, arg_no] : get_static_trace(dfg, X)) {
		const auto& function = bdf.get().get_function();
		assert(arg_no < function.arg_size());
		const llvm::Argument& equiv_base = ptr2ref<llvm::Argument>(function.arg_begin() + arg_no);
		const Address equiv_address = X.address.rebase(equiv_base);
		for (const Ref<MemoryAccess>& access : bdf.get().all_block_conflicts(equiv_address.block)) {
			const Address transposed_address =
				(&access.get().address.block.segment.base == &equiv_base)
				? access.get().address.rebase(orig_base)
				: access.get().address
				;

			cache.insert(transposed_address);
			assert(transposed_address.aligned(WORD_SIZE));
			if (cache.size() > 0.75 * bdf.get().get_hw_params().cache_size) {
				break;
			}
			if (access.get().kind == +AccessKind::load && transposed_address.block == X.address.block) {
				mask.set(transposed_address.block_offset / WORD_SIZE);
			}
		}
	}

	return mask;
}

/*
Algorithm 4: Request-granularity selection.
*/
static std::pair<WordMask, Req> select_granularity(const BdfDfg& dfg, const MemoryAccess& X) {
	if (X.req_type == +Req::V) {
		return std::pair<WordMask, Req>(X.bdf.intra_synch_load_reuse(X), X.req_type);
	} else if (X.req_type == +Req::S) {
		return std::pair<WordMask, Req>(full_block_mask(), X.req_type);
	} else if (X.req_type == +Req::WTo || X.req_type == +Req::WTfwd || X.req_type == +Req::WTo_data || X.req_type == +Req::WTfwd_data) {
		return std::pair<WordMask, Req>(requested_words_only(X), X.req_type);
	} else if (X.req_type == +Req::O || X.req_type == +Req::O_data) {
		WordMask word_mask = inter_synch_load_reuse(dfg, X);
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
	std::list<BoiledDownFunction> bdfs;
	llvm::DataLayout data_layout {&module};
	HardwareParams hp {
					   .producer = false,
					   .owner_pred_available = false,
					   .cache_size = 1024*1024 * BYTE_SIZE,
					   .block_size = 64 * BYTE_SIZE,
					   .core = {hpvm::CPU_TARGET, 0},
	};
	BdfDfg bdf_mem_comm_dfg = map_graph<Port, BdfDfgNode, Digraph<Port>, BdfDfg>(mem_comm_dfg, [&](const Port& port) {
		const auto& function = ptr2ref<llvm::Function>(port.N.getFuncPointer());
		bdfs.emplace_back(function, data_layout, hp);
		unsigned int pos = port.pos;
		return BdfDfgNode{bdfs.back(), std::move(pos)};
	});
	BdfDfg reverse_dfg = reverse<BdfDfgNode, BdfDfg>(bdf_mem_comm_dfg);
	for (auto& bdf : bdfs) {
		for (auto& ma : bdf.get_all_accesses()) {
			if (llvm::isa<llvm::Argument>(ma.address.block.segment.base)) {
				ma.req_type = assign_request_type(bdf_mem_comm_dfg, reverse_dfg, ma);
				auto word_mask_x_req_type = select_granularity(bdf_mem_comm_dfg, ma);
				ma.word_mask = word_mask_x_req_type.first;
				ma.req_type = word_mask_x_req_type.second;
				std::cout
					<< std::setw(60) << std::left << llvm_to_str(ma.instruction)
					<< std::setw(5) << std::left << ma.req_type._to_string()
					<< "[" << ma.word_mask.to_string() << "] "
					<< "for "<< ma.address << " "
					<< std::endl;
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
