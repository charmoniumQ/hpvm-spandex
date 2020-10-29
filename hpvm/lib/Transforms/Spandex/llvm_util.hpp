#pragma once
#include <utility>
#include <optional>
#define DEBUG_TYPE "Spandex"
#include "llvm/Support/Debug.h"
#include "enum.h"
#include "util.hpp"

llvm::raw_ostream &operator<<(llvm::raw_ostream &stream,
                              const llvm::DFNode &N) {
  return stream << demangle(N.getFuncPointer()->getName().str())
                << (N.isEntryNode() ? ".entry" : N.isExitNode() ? ".exit" : "");
}

template <typename First, typename Second>
llvm::raw_ostream &operator<<(llvm::raw_ostream &stream,
                              const std::pair<First, Second> &pair) {
  return stream << pair.first << " : " << pair.second;
}

BETTER_ENUM(AccessKind, char, load, store, rmw, cas)

std::optional<std::pair<const llvm::Value&, AccessKind>>
get_pointer_target(const llvm::Instruction& instruction) {
	if (llvm::isa<llvm::StoreInst>(&instruction)) {
		const auto& store_inst = ptr2ref<llvm::StoreInst>(llvm::dyn_cast<llvm::StoreInst>(&instruction));
		return std::make_optional<std::pair<const llvm::Value&, AccessKind>>(
			ptr2ref<llvm::Value>(store_inst.getPointerOperand()),
			AccessKind::store
		);
	} else if (llvm::isa<llvm::LoadInst>(&instruction)) {
		const auto& load_inst = ptr2ref<llvm::LoadInst>(llvm::dyn_cast<llvm::LoadInst>(&instruction));
		return std::make_optional<std::pair<const llvm::Value&, AccessKind>>(
			ptr2ref<llvm::Value>(load_inst.getPointerOperand()),
			AccessKind::load
		);
	} else if (llvm::isa<llvm::AtomicCmpXchgInst>(&instruction)) {
		const auto& cas_inst = ptr2ref<llvm::AtomicCmpXchgInst>(llvm::dyn_cast<llvm::AtomicCmpXchgInst>(&instruction));
		return std::make_optional<std::pair<const llvm::Value&, AccessKind>>(
			ptr2ref<llvm::Value>(cas_inst.getPointerOperand()),
			AccessKind::cas
		);
	} else if (llvm::isa<llvm::AtomicRMWInst>(&instruction)) {
		const auto& rmw_inst = ptr2ref<llvm::AtomicRMWInst>(llvm::dyn_cast<llvm::AtomicRMWInst>(&instruction));
		return std::make_optional<std::pair<const llvm::Value&, AccessKind>>(
			ptr2ref<llvm::Value>(rmw_inst.getPointerOperand()),
			AccessKind::rmw
		);
	} else {
		return std::nullopt;
	}
}

std::optional<std::pair<const llvm::Value&, const llvm::Value&>>
split_pointer(const llvm::Value& pointer) {
	// TODO: write split_pointer
	// - handle GEP
	return std::nullopt;
}

template <typename T>
std::optional<T>
statically_evaluate(const llvm::Value&) {
	/* Don't recognize the type T, so I can't statically evaluate.*/
	return std::nullopt;
}

template <>
std::optional<size_t>
statically_evaluate(const llvm::Value& value) {
	return std::nullopt;
}
