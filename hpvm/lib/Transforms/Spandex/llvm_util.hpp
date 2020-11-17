#pragma once
#include <utility>
#include <functional>
#include <optional>
#define DEBUG_TYPE "Spandex"
#include "llvm/Support/Debug.h"
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

template <typename T> str
llvm_to_str_ndbg(T& obj) {
	std::string s;
	llvm::raw_string_ostream os {s};
	obj.print(os);
	return os.str();
}

template <typename T> str
llvm_to_str(T& obj) {
	std::string s;
	llvm::raw_string_ostream os {s};
	obj.print(os, true);
	return os.str();
}

static llvm::Value&
const_int(llvm::LLVMContext& context, size_t val = 0, unsigned width = 64, bool is_signed = false) {
	const auto& type = llvm::IntegerType::get(context, width);
	return ptr2ref<llvm::Value>(llvm::ConstantInt::get(type, val, is_signed));
}

static llvm::Value&
binary_op(llvm::Instruction::BinaryOps op, llvm::Value& left, llvm::Value& right) {
	return *llvm::BinaryOperator::Create(op, &left, &right);
}

BETTER_ENUM(AccessKind, char, load, store, rmw, cas)

/*
There is a weird problem between ResultStr and std::pair<std::reference_wrapper<...>, ...>

ResultStr<std::pair<std::reference_wrapper<const llvm::Value>, ...>>
has a weird problem.

Ok(std::make_pair<std::reference_wrapper<const llvm::Value>, ...>(...))
converts to
types::Ok<pair<const not_copyable &, int> >
which is not convertible to
types::Ok<pair<std::reference_wrapper<const not_copyable>, int> >

Therefore, I will use raw pointers
 */
static ResultStr<std::pair<const llvm::Value*, AccessKind>>
get_pointer_target(const llvm::Instruction& instruction) {
	auto pair = std::make_pair<const llvm::Value*, AccessKind>;
	if (const auto* store_inst = llvm::dyn_cast<llvm::StoreInst>(&instruction)) {
		return Ok(pair(store_inst->getPointerOperand(), AccessKind::store));
	} else if (const auto* load_inst = llvm::dyn_cast<llvm::LoadInst>(&instruction)) {
		return Ok(pair(load_inst->getPointerOperand(), AccessKind::load));
	} else if (const auto* cas_inst = llvm::dyn_cast<llvm::AtomicCmpXchgInst>(&instruction)) {
		return Ok(pair(cas_inst->getPointerOperand(), AccessKind::cas));
	} else if (const auto* rmw_inst = llvm::dyn_cast<llvm::AtomicRMWInst>(&instruction)) {
		return Ok(pair(rmw_inst->getPointerOperand(), AccessKind::rmw));
	} else {
		return Err(str{"Instruction "} + llvm_to_str(instruction) + str{" is not a memory access"});
	}
}

static bool is_memory_access(const llvm::Instruction& instruction) {
	return get_pointer_target(instruction).isOk();
}

#define DEBUG(expr) std::cerr << __FILE__ ":" << __LINE__ << ": " #expr ": " << expr << std::endl;

static ResultStr<std::pair<const llvm::Value*, const llvm::Value*>>
split_pointer(const llvm::DataLayout& data_layout, const llvm::Value& pointer) {
	using binops = llvm::Instruction::BinaryOps;
	auto pair = std::make_pair<const llvm::Value*, const llvm::Value*>;
	llvm::LLVMContext& context = pointer.getContext();

	if (llvm::isa<llvm::Argument>(&pointer)) {
		return Ok(pair(&pointer, &const_int(context)));
	} else if (llvm::isa<llvm::CallBase>(&pointer)) {
		return Ok(pair(&pointer, &const_int(context)));
	} else if (const auto* _gep = llvm::dyn_cast<llvm::GetElementPtrInst>(&pointer)) {
		const auto& gep = ptr2ref<llvm::GetElementPtrInst>(_gep);
		llvm::Value* offset = &const_int(context);

		llvm::Type* type = gep.getPointerOperandType();
		for (const auto& use : gep.indices()) {
			assert(offset);
			assert(type && (type->isPointerTy() || type->isArrayTy()));
			unsigned int type_size = data_layout.getIndexTypeSizeInBits(type);
			offset = &binary_op(binops::Add, *offset, binary_op(binops::Mul, *use, const_int(context, type_size)));
			if (type->isArrayTy()) {
				type = type->getArrayElementType();
			} else if (type->isPointerTy()) {
				type = type->getPointerElementType();
			} else {
				return Err(str{"GEP '"} + llvm_to_str(gep) + str{" ' has an index on a non-pointer type '"} + llvm_to_str(*type) + str{"'"});
			}
		}
		return Ok(pair(gep.getPointerOperand(), offset));
	} else {
		return Err(str{"Unknown value type '"} + llvm_to_str(pointer) + str{"'"});
	}
}

#define STRINGIFY(macro) #macro

template <typename T> ResultStr<T>
statically_evaluate(const llvm::Value&) {
	/* Don't recognize the type T, so I can't statically evaluate.*/
	// Unfortunately typeid(T).name() doesn't work because LLVM compiles with -fno-rtti
	return Err(str{"No known method to statically evaluate to type '" STRINGIFY(__PRETTY_FUNCTION__) "'"});
}

template <> ResultStr<uint64_t>
statically_evaluate(const llvm::Value& value) {
	/*
	  TODO: make the template type a variable, which has enable_if<is_integral<T>...>
	  TODO: implement more BinaryOps
	  TODO: implement UnaryOps
	  TODO: implement proper overflow nuw and nsw
	 */

	const llvm::Type& type = ptr2ref<llvm::Type>(value.getType());
	if (!type.isIntegerTy()) {
		return Err(str{"'"} + llvm_to_str(value) + str{"' is of type '"} + llvm_to_str(type) + str{"' (non-integral), not u64"});
	}
	const llvm::IntegerType& int_type = ptr2ref<llvm::IntegerType>(llvm::dyn_cast<llvm::IntegerType>(&type));
	if (int_type.getBitWidth() > sizeof(uint64_t) * 8) {
		return Err(str{"'"} + llvm_to_str(value) + str{"' is of type '"} + llvm_to_str(type) + str{"', not u64 (too large bitwidth)"});
	}

	if (const auto* constant_int = llvm::dyn_cast<llvm::ConstantInt>(&value)) {
		return Ok(constant_int->getZExtValue());
	} else if (const auto* _binary_op = llvm::dyn_cast<llvm::BinaryOperator>(&value)) {
		const auto& binary_op = ptr2ref<llvm::BinaryOperator>(_binary_op);
		auto lhs = statically_evaluate<uint64_t>(ptr2ref<llvm::Value>(binary_op.getOperand(0)));
		auto rhs = statically_evaluate<uint64_t>(ptr2ref<llvm::Value>(binary_op.getOperand(1)));
		if (lhs.isOk() && rhs.isOk()) {
			using binops = llvm::Instruction::BinaryOps;
			switch (binary_op.getOpcode()) {
			case binops::Add:
				return Ok(lhs.unwrap() + rhs.unwrap());
			case binops::Mul:
				return Ok(lhs.unwrap() * rhs.unwrap());
			default:
				return Err(str{"statically_evaluate does not support op code '"} + llvm_to_str(value) + str{"'"});
			}
		} else if (lhs.isOk()) {
			return Err(str{"right-hand-side of '"} + llvm_to_str(value) + str{"' is not statically evaluatable"});
		} else {
			return Err(str{ "left-hand-side of '"} + llvm_to_str(value) + str{"' is not statically evaluatable"});
		}
	} else {
		return Err(str{"Value type of '"} + llvm_to_str(value) + str{"' is not statically evaluatable"});
	}
}

class Segment {
public:
	Segment(const llvm::Value& _base, unsigned short _block_size)
		: base{_base}
		, block_size{_block_size}
	{ }
	const llvm::Value& base;
	unsigned short block_size;
	bool operator==(const Segment& other) const { return &base == &other.base; }
	bool operator!=(const Segment& other) const { return !(*this == other); }
	Segment rebase(const llvm::Value& new_base) const {
		return Segment{new_base, block_size};
	}
};

class Block {
public:
	Block(Segment _segment, unsigned int _block_index)
		: segment{_segment}
		, block_index{_block_index}
	{ }
	Segment segment;
	unsigned int block_index;
	bool operator==(const Block& other) const { return segment == other.segment && block_index == other.block_index; }
	bool operator!=(const Block& other) const { return !(*this == other); }
	Block operator+(int i) const {
		assert(int(block_index) + i >= 0);
		return Block{segment, block_index + i};
	}
	Block operator-(int i) const {
		return *this + (-i);
	}
	Block rebase(const llvm::Value& new_base) const {
		return Block{segment.rebase(new_base), block_index};
	}
};

class Address {
public:
	Address(Block _block, unsigned short _block_offset)
		: block{_block}
	{ }
	Block block;
	unsigned short block_offset;
	bool aligned(unsigned short alignment) const {
		assert(block.segment.block_size % alignment == 0);
		return block_offset % alignment == 0;
	}
	bool operator==(const Address& other) const { return block == other.block && block_offset == other.block_offset; }
	bool operator!=(const Address& other) const { return !(*this == other); }
	Address operator+(int i) const {
		assert(block_offset + i > 0);
		unsigned int new_block_index = (block_offset + i) / block.segment.block_size;
		unsigned short new_block_offset = (block_offset + i) % block.segment.block_size;
		return Address{Block{block.segment, new_block_index}, new_block_offset};
	}
	Address operator-(int i) const { return *this + (-i); }

	static ResultStr<Address>
	create(const llvm::DataLayout& data_layout, const llvm::Value& pointer, unsigned short block_size) {
		const auto base_x_offset = TRY(split_pointer(data_layout, pointer));
		const auto& base = *base_x_offset.first;

		/*
		 TODO: also permit:
		 - result of malloc
		 - reference to static data
		*/
		if (llvm::isa<llvm::Argument>(&base)) {
			size_t offset = TRY(statically_evaluate<size_t>(*base_x_offset.second));
			unsigned int block_index = offset / block_size;
			unsigned short block_offset = offset % block_size;
			return Ok(Address{Block{Segment{base, block_size}, block_index}, block_offset});
		} else if (llvm::isa<llvm::CallBase>(&base)) {
			return Err(str{"Base pointer '"} + llvm_to_str(base) + str{"' is a callbase which has not yet been implemented"});
		} else {
			return Err(str{"Base pointer '"} + llvm_to_str(base) + str{"' is not the start of a segment"});
		}
	}
	Address rebase(const llvm::Value& new_base) const {
		return Address{block.rebase(new_base), block_offset};
	}
};

namespace std {
	template<> struct hash<Segment> {
		std::size_t operator()(const Segment& segment) const noexcept {
			return reinterpret_cast<size_t>(&segment.base);
		}
	};

	template<> struct hash<Block> {
		std::size_t operator()(const Block& block) const noexcept {
			return reinterpret_cast<size_t>(&block.segment.base) ^ block.block_index;
		}
	};

	template<> struct hash<Address> {
		std::size_t operator()(const Address& address) const noexcept {
			return reinterpret_cast<size_t>(&address.block.segment.base) ^ address.block.block_index ^ address.block_offset;
		}
	};

	template <> struct hash<llvm::DFNode> {
		std::size_t operator()(const llvm::DFNode& address) const noexcept {
			return reinterpret_cast<size_t>(&address);
		}
	};

	bool operator==(const llvm::DFNode& a, const llvm::DFNode& b) {
		return &a == &b;
	}
	bool operator!=(const llvm::DFNode& a, const llvm::DFNode& b) {
		return !(a == b);
	}
}

bool operator==(const llvm::DFNode& a, const llvm::DFNode& b) {
	return &a == &b;
}
bool operator!=(const llvm::DFNode& a, const llvm::DFNode& b) {
	return !(a == b);
}
