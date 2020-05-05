#include "SupportHPVM/DFGraph.h"
#include <utility>
#include <string>
#include <regex>
#include <cxxabi.h>

std::string demangle(const std::string &input) {
  int status = -1;
  std::string real_input = input.substr(0, input.size() - 8)
      // 	input.substr(input.size() - 8) == std::string{"_cloned_"}
      // 	? input.substr(0, input.size() - 8)
      // 	: input
      ;

  auto output =
      abi::__cxa_demangle(real_input.c_str(), nullptr, nullptr, &status);
  if (status == 0) {
    auto real_output = std::regex_replace(output, std::regex("unsigned "), "u");
    return {real_output};
  } else {
    dbgs() << "abi::__cxa_demangle returned " << status << " on " << real_input
           << "\n";
    return {input};
  }
}

struct Port {
  llvm::DFNode const *N;
  unsigned int pos;
  bool operator==(const Port &other) const {
    return N == other.N && pos == other.pos;
  }
};

namespace std {
template <> struct hash<Port> {
  size_t operator()(const Port &p) const {
    return hash<DFNode const *>()(p.N) ^ hash<unsigned>()(p.pos);
  }
};
} // namespace std

namespace llvm {
llvm::raw_ostream &operator<<(llvm::raw_ostream &stream,
                              const llvm::DFNode &N) {
  return stream << demangle(N.getFuncPointer()->getName().str()) << "("
                << N.getLevel() << "," << N.getRank() << ")";
}
} // namespace llvm

llvm::raw_ostream &operator<<(llvm::raw_ostream &stream, const Port &port) {
  return stream << *port.N << "[" << port.pos << "]";
}

namespace llvm {
llvm::raw_ostream &operator<<(llvm::raw_ostream &stream,
                              const llvm::DFEdge &E) {
  return stream << "DFEdge{" << Port{E.getSourceDF(), E.getSourcePosition()}
                << " -> " << Port{E.getDestDF(), E.getDestPosition()} << "}";
}
} // namespace llvm

namespace std {
template <typename First, typename Second>
llvm::raw_ostream &operator<<(llvm::raw_ostream &stream,
                              const pair<First, Second> &pair) {
  return stream << pair.first << " : " << pair.second;
}
} // namespace std

template <typename Collection>
llvm::raw_ostream &operator<<(llvm::raw_ostream &stream,
                              const Collection &collection) {
  stream << "[";
  typename Collection::const_iterator it = collection.cbegin();
  if (it != collection.cend()) {
    stream << *it;
    ++it;
  }
  while (it != collection.cend()) {
    stream << ", " << *it;
    ++it;
  }
  return stream << "]";
}
