#include "SupportHPVM/DFGraph.h"
#include <utility>

struct Port {
	llvm::DFNode const * N;
	unsigned int pos;
	bool operator==(const Port& other) const { return N == other.N && pos == other.pos; }
};

namespace llvm {
	llvm::raw_ostream& operator<< (llvm::raw_ostream& stream, const llvm::DFNode& N) {
		return stream
			<< N.getFuncPointer()->getName().str()
			<< "_" << N.getLevel()
			<< "_" << N.getRank()
			// << "_" << (N.getInstruction() ? (string) N.getInstruction()->getName() : "null")
			;
	}

}

llvm::raw_ostream& operator<< (llvm::raw_ostream& stream, const Port &port) {
	return stream << *port.N << "[" << port.pos << "]";
}

namespace llvm {
	llvm::raw_ostream& operator<< (llvm::raw_ostream& stream, const llvm::DFEdge& E) {
		return stream
			<< "DFEdge{"
			<< Port{E.getSourceDF(), E.getSourcePosition()}
			<< " -> "
			<< Port{E.getDestDF(), E.getDestPosition()}
			<< "}";
	}
}

namespace std {
	template <typename First, typename Second>
	llvm::raw_ostream& operator<< (llvm::raw_ostream& stream, const pair<First, Second> &pair) {
		return stream << pair.first << " : " << pair.second;
	}
}

template <typename Collection>
llvm::raw_ostream& operator<< (llvm::raw_ostream& stream, const Collection &collection) {
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

namespace std {
	template <> struct hash<Port> {
		size_t operator() (const Port &p) const {
			return hash<DFNode const *>()(p.N) ^ hash<unsigned>()(p.pos);
		}
	};
}
