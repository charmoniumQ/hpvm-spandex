#pragma once
#include "SupportHPVM/DFGraph.h"
#include "llvm_util.hpp"

struct Port {
  llvm::DFNode const *N;
  unsigned int pos;
  bool operator==(const Port &other) const {
    return N == other.N && pos == other.pos;
  }
  bool operator!=(const Port &other) const { return !(*this == other); }
  llvm::Type *get_type() const {
    return (N->getFuncPointer()->arg_begin() + pos)->getType();
  }
};

namespace std {
template <> struct hash<Port> {
  size_t operator()(const Port &p) const {
    return hash<DFNode const *>()(p.N) ^ hash<unsigned>()(p.pos);
  }
};
} // namespace std

llvm::raw_ostream &operator<<(llvm::raw_ostream &stream, const Port &port) {
  return stream << *port.N << "[" << port.pos << "]";
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &stream,
                              const llvm::DFEdge &E) {
  return stream << "DFEdge{" << Port{E.getSourceDF(), E.getSourcePosition()}
                << " -> " << Port{E.getDestDF(), E.getDestPosition()} << "}";
}
