#define DEBUG_TYPE "Spandex"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm_util.hpp"
#include "hpvm_util.hpp"
#include "enum.h"

BETTER_ENUM(SpandexRequestType, char, O_data, O, S, V, WTfwd, WTfwd_data, Vo, WTo, WTo_data, unassigned)

using Req = SpandexRequestType;

constexpr unsigned char LINE_SIZE = 64;
constexpr unsigned char WORDS_SIZE = 8;
using WordMask = std::bitset<LINE_SIZE / WORDS_SIZE>;

class MemoryAccess {
public:
	const llvm::Pass& pass;
	const llvm::Function& function;
	const llvm::BasicBlock& basic_block;
	const llvm::Instruction& instruction;
	const llvm::Value& pointer;
	const AccessKind access_kind;
	const llvm::Value& base;
	const size_t offset;
	const bool producer;
	const hpvm::Target target;
	const bool owner_pred_available;
	Req req_type;
	WordMask mask;

private:
MemoryAccess(const llvm::Pass& _pass, const llvm::Function& _function, const llvm::BasicBlock& _basic_block, const llvm::Instruction& _instruction, const llvm::Value& _pointer, const AccessKind _access_kind, const llvm::Value& _base, const size_t _offset, const bool _producer, const hpvm::Target _target, const bool _owner_pred_available)
	: pass{_pass}
	, function{_function}
	, basic_block{_basic_block}
	, instruction{_instruction}
	, pointer{_pointer}
	, access_kind{_access_kind}
	, base{_base}
	, offset{_offset}
	, producer{_producer}
	, target{_target}
	, owner_pred_available{_owner_pred_available}
	, req_type{Req::unassigned}
	, mask{}
	{ }

public:
	static MemoryAccess create(const llvm::Pass& pass, const llvm::Instruction& instruction, bool producer, hpvm::Target target, bool owner_pred_available) {
		const auto& function = ptr2ref<llvm::Function>(instruction.getFunction());
		const auto& basic_block = ptr2ref<llvm::BasicBlock>(instruction.getParent());

		const auto pointer_x_access = get_pointer_target(instruction);
		if (!pointer_x_access) {
			errs() << "Instruction '";
			instruction.print(errs(), true);
			errs() << "' is not a memory access\n";
			abort();
		}
		const auto& pointer = pointer_x_access->first;
		const auto& access_kind = pointer_x_access->second;

		const auto base_x_offset = split_pointer(pointer);
		if (!base_x_offset) {
			errs() << "Pointer '";
			pointer.print(errs(), true);
			errs() << "' could not be split into base and offset\n";
			abort();
		}
		const auto& base = base_x_offset->first;

		auto _offset = statically_evaluate<size_t>(base_x_offset->second);
		if (!_offset) {
			errs() << "Offset '";
			base_x_offset->second.print(errs(), true);
			errs() << "' could not be statically evaluated\n";
			abort();
		}
		const auto& offset = *_offset;
		return {pass, function, basic_block, instruction, pointer, access_kind, base, offset, producer, target, owner_pred_available};
	}
};

/*
Algorithm 5: Is ownership beneficial?
*/
bool ownership_beneficial(const MemoryAccess& X) {
	return false;
}

/*
Algorithm 6: Is shared-state beneficial?
*/
bool shared_state_beneficial(const MemoryAccess& X) {
	return false;
}

/*
Algorithm 7: Is owner-prediction beneficial?
*/
bool owner_pred_beneficial(const MemoryAccess& X) {
	return false;
}

WordMask requested_words_only(const MemoryAccess& X) {
	WordMask mask;
	mask.set(X.offset % LINE_SIZE);
	return mask;
}

WordMask intra_synch_load_reuse(const MemoryAccess& X) {
	WordMask mask;
	// TODO: do this
	return mask;
}

/*
Algorithm 1: Select load request type.
*/
void request_type_for_load(MemoryAccess& X) {
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
void request_type_for_store(MemoryAccess& X) {
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
void request_type_for_rmw(MemoryAccess& X) {
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
void granularity_selection(MemoryAccess& X) {
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

void assign_request_types(llvm::Function& function, const llvm::Argument& argument, bool producer, hpvm::Target target, bool owner_pred_available, llvm::Pass& pass) {
	/*
	ScalarEvolution &SE =
		pass.getAnalysis<ScalarEvolutionWrapperPass>(function).getSE();
	// SE.getSmallConstantTripCount(const Loop*) is not const

	const LoopInfo &LI = pass.getAnalysis<LoopInfoWrapperPass>(function).getLoopInfo();

	AAResultsProxy& AA = pass.getAnalysis<AAResultsWrapperPass>(function).getBestAAResults();
	// AA.alias is not const.
	*/
}
