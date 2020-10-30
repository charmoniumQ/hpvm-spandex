#define DEBUG_TYPE "Spandex"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm_util.hpp"

BETTER_ENUM(SpandexRequestType, char, O_data, O, S, V, WTfwd, WTfwd_data, Vo, WTo, WTo_data, unassigned)

using Req = SpandexRequestType;

constexpr unsigned short BYTE_SIZE = 8;
constexpr unsigned short LINE_SIZE = 64 * BYTE_SIZE;
constexpr unsigned short WORD_SIZE = 8 * BYTE_SIZE;
using WordMask = std::bitset<LINE_SIZE / WORD_SIZE>;

class HardwareParams {
public:
	bool producer;
	hpvm::Target target;
	bool owner_pred_available;
	size_t cache_size;
};

class MemoryAccess {
public:
	const llvm::Instruction& instruction;
	const AccessKind kind;
	const StaticMemoryLocation location;
	const size_t pos;
	Req req_type;
	WordMask mask;
private:
	MemoryAccess(const llvm::Instruction& _instruction, AccessKind _kind, const StaticMemoryLocation& _location, size_t _pos)
		: instruction{_instruction}
		, kind{_kind}
		, location{_location}
		, pos{_pos}
		, req_type{Req::unassigned}
	{ }

public:
	static ResultStr<MemoryAccess>
	create(const llvm::DataLayout& data_layout, const llvm::Instruction& instruction, size_t pos) {
		const auto& [pointer, kind] = TRY(get_pointer_target(instruction));
		const auto& location = TRY(StaticMemoryLocation::create(data_layout, *pointer));
		return Ok(MemoryAccess{instruction, kind, location, pos});
	}
};

class BoiledDownFunction {
private:
	std::vector<MemoryAccess> accesses_in_order;
	std::unordered_map<StaticMemoryLocation, std::vector<MemoryAccess>> accesses_to_loc;
	BoiledDownFunction() { }
public:
	static ResultStr<BoiledDownFunction>
	create(const llvm::Function& function, const llvm::DataLayout& data_layout) {
		BoiledDownFunction bdf;
		if (&function.front() != &function.back()) {
			return Err(str{"Spandex pass supports straight-line code for now. Function is not straight-line:\n"} + llvm_to_str_ndbg(function));
		} else {
			const llvm::BasicBlock& basic_block = function.getEntryBlock();
			for (const llvm::Instruction& instruction : basic_block) {
				if (is_memory_access(instruction)) {
					auto access = TRY(MemoryAccess::create(data_layout, instruction, bdf.accesses_in_order.size()));
					bdf.accesses_in_order.push_back(access);
					bdf.accesses_to_loc[access.location].push_back(access);
				}
			}
			return Ok(bdf);
		}
	}
	size_t n_unique_bytes(size_t i, size_t j) const {
		// TODO: improve performance of this algorithm with dynamic programming
		size_t counter = 0;
		std::unordered_set<StaticMemoryLocation> seen;
		for (auto it = accesses_in_order.cbegin() + i; it < accesses_in_order.cbegin() + j; ++it) {
			if (seen.count(it->location) == 0) {
				seen.insert(it->location);
				counter++;
			}
		}
		return counter;
	}
	const std::vector<MemoryAccess>& subsequent_conflicts(StaticMemoryLocation& X) const {
		return const_cast<BoiledDownFunction&>(*this).accesses_to_loc[X];
	}
};

/*
Algorithm 5: Is ownership beneficial?
*/
static bool ownership_beneficial(const MemoryAccess& X) {
	return false;
}

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
	mask.set((X.location.offset % LINE_SIZE) / WORD_SIZE);
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
