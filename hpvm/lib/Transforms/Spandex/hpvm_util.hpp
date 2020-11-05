#pragma once
#include "SupportHPVM/DFGraph.h"
#include "graph_util.hpp"
#include "llvm_util.hpp"

struct Port {
  const llvm::DFNode &N;
  unsigned int pos;
  bool operator==(const Port &other) const {
    return &N == &other.N && pos == other.pos;
  }
  bool operator!=(const Port &other) const { return !(*this == other); }
  llvm::Type & get_type() const {
	  return ptr2ref<llvm::Type>(ptr2ref<llvm::Argument>(ptr2ref<Function>(N.getFuncPointer()).arg_begin() + pos).getType());
  }
};

namespace std {
template <> struct hash<Port> {
  size_t operator()(const Port &p) const {
    return hash<const DFNode *>()(&p.N) ^ hash<unsigned>()(p.pos);
  }
};
} // namespace std

llvm::raw_ostream &operator<<(llvm::raw_ostream &stream, const Port &port) {
  return stream << port.N << "[" << port.pos << "]";
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &stream,
                              const llvm::DFEdge &E) {
	return stream << "DFEdge{" << Port{ptr2ref<llvm::DFNode>(E.getSourceDF()), E.getSourcePosition()}
	<< " -> " << Port{ptr2ref<llvm::DFNode>(E.getDestDF()), E.getDestPosition()} << "}";
}

class get_dfg_helper : public llvm::DFNodeVisitor {
private:
  Digraph<Port> dfg;

public:
	const llvm::DFNode& normalize(const llvm::DFNode& orig, bool src) {
    if (orig.getKind() == DFNode::InternalNode) {
	  const DFInternalNode& orig2 = reinterpret_cast<const llvm::DFInternalNode &>(orig);
      DFGraph& g = ptr2ref<DFGraph>(orig2.getChildGraph());
      return src ? ptr2ref<llvm::DFNode>(g.getExit()) : ptr2ref<llvm::DFNode>(g.getEntry());
    } else {
      return orig;
    }
  }

  virtual void visit2(const llvm::DFNode& N) {
	  for (auto _edge = N.outdfedge_begin(); _edge != N.outdfedge_end(); ++_edge) {
		  const auto& edge = ptr2ref<llvm::DFEdge>(*_edge);
		  Port src {normalize(ptr2ref<DFNode>(edge.getSourceDF()), true), edge.getSourcePosition()};
		  Port dst {normalize(ptr2ref<DFNode>(edge.getDestDF()), false), edge.getDestPosition()};
      // LLVM_DEBUG(dbgs() << src << " -> " << dst << "\n");
      dfg[src].insert(dst);
    }
  }

  virtual void visit(llvm::DFInternalNode *N) {
	visit2(ptr2ref<llvm::DFNode>(reinterpret_cast<llvm::DFNode*>(N)));
    for (const llvm::DFNode *child : ptr2ref<llvm::DFGraph>(N->getChildGraph())) {
		const_cast<llvm::DFNode &>(ptr2ref<llvm::DFNode>(child)).applyDFNodeVisitor(*this);
    }
  }
  virtual void visit(llvm::DFLeafNode *N) {
	  visit2(ptr2ref<llvm::DFNode>(reinterpret_cast<llvm::DFNode*>(N)));
  }
  Digraph<Port> get_dfg() { return dfg; }
};

Digraph<Port> get_dfg(const llvm::DFInternalNode& N) {
  get_dfg_helper gdh;
  // I know this visitor does not modify the graph a priori
  // but the graph visitor API (not owned by me) is marked as non-const
  const_cast<llvm::DFInternalNode&>(N).applyDFNodeVisitor(gdh);
  return gdh.get_dfg();
}

Digraph<Port> get_leaf_dfg(const Digraph<Port>& dfg) {
	Digraph<Port> leaf_dfg{dfg}; // copy
	delete_nodes<Port>(leaf_dfg, [](const Port &port) {
		return port.N.isDummyNode() && port.N.getLevel() != 1;
	});
	return leaf_dfg;
}

Digraph<Ref<llvm::DFNode>> get_coarse_leaf_dfg(const Digraph<Port>& leaf_dfg) {
	return map_graph<Port, Ref<llvm::DFNode>>(leaf_dfg, [](const Port &port) { return std::cref(port.N); });
}

Digraph<Port> get_mem_comm_dfg(const Digraph<Port> &dfg,
                               const Digraph<Ref<llvm::DFNode>> &coarse_dfg) {
  Digraph<Port> result;
  for_each_adj_list<Port>(dfg, [&](const Port &src, const AdjList<Port> dsts) {
    if (dsts.size() > 1 && dsts.cbegin()->get_type().isPointerTy()) {
      for (const Port &dst1 : dsts) {
        for (const Port &dst2 : dsts) {
          if (dst1 != dst2 && &dst1.N != &dst2.N) {
            if (true &&
                ptr2ref<llvm::Function>(dst1.N.getFuncPointer()).hasAttribute(dst1.pos + 1, llvm::Attribute::Out) &&
                ptr2ref<llvm::Function>(dst2.N.getFuncPointer()).hasAttribute(dst2.pos + 1, llvm::Attribute::In ) &&
                is_descendant<Ref<llvm::DFNode>>(coarse_dfg, dst1.N, dst2.N)) {
              result[dst1].insert(dst2);
            }
          }
        }
      }
    }
  });
  return result;
}


llvm::raw_ostream &dump_graphviz_ports(llvm::raw_ostream &os, const Digraph<Port> &dfg,
									   bool inps_only = false) {
  os << "digraph structs {\n";

  os << "\tnode [shape=record];\n";

  std::unordered_set<const llvm::DFNode *> dfnodes;
  for_each_adj<Port>(dfg, [&](const Port &src, const Port &dst) {
    // omit port info
    dfnodes.insert(&src.N);
    dfnodes.insert(&dst.N);
  });

  for (const llvm::DFNode* _node : dfnodes) {
	  const auto& node = ptr2ref<llvm::DFNode>(_node);
    os << "\t"
       << "\"" << node << "\" "
       << "["
       << "label=\"{";
    const llvm::Function &fn = ptr2ref<llvm::Function>(node.getFuncPointer());
    const llvm::StructType &ST = ptr2ref<llvm::StructType>(node.getOutputType());
    if (node.isExitNode()) {
      unsigned i = 0;
      for (llvm::Type* const* type_it = ST.element_begin(); type_it != ST.element_end(); ++type_it, ++i) {
		  const auto& type = ptr2ref<llvm::Type>(ptr2ref<llvm::Type *>(type_it));
        if (i != 0) {
          os << "|";
        }
        os << "<i" << i << ">" << type;
      }
    } else {
      unsigned i = 0;
      for (const llvm::Argument* arg_it = fn.arg_begin(); arg_it != fn.arg_end();
           ++arg_it, ++i) {
		  const auto& arg = ptr2ref<llvm::Argument>(arg_it);
        if (i != 0) {
          os << "|";
        }
        os << "<i" << i << ">" << ptr2ref<Type>(arg.getType()) << " " << arg.getName();
      }
    }
    os << "}|";
    os << node;
    if (!inps_only) {
      os << "|{";
      if (node.isEntryNode()) {
        unsigned i = 0;
      for (const llvm::Argument* arg_it = fn.arg_begin(); arg_it != fn.arg_end();
           ++arg_it, ++i) {
		  const auto& arg = ptr2ref<llvm::Argument>(arg_it);
          if (i != 0) {
            os << "|";
          }
          os << "<o" << i << ">" << arg.getType();
        }
      } else {
        unsigned i = 0;
      for (llvm::Type* const* type_it = ST.element_begin(); type_it != ST.element_end(); ++type_it, ++i) {
		  const auto& type = ptr2ref<llvm::Type>(ptr2ref<llvm::Type *>(type_it));
          if (i != 0) {
            os << "|";
          }
          os << "<o" << i << ">" << type;
        }
      }
      os << "}";
    }
    os << "\"];\n";
  }

  os << "\n";

  for_each_adj<Port>(dfg, [&](const Port &src, const Port &dst) {
    // TODO: a way to escape quotes in strings
    os << "\t"
       << "\"" << src.N << "\" "
       << "-> "
       << "\"" << dst.N << "\" "
       << "["
       << "tailport=" << (inps_only ? "i" : "o") << src.pos << ", "
       << "headport=" << (inps_only ? "i" : "i") << dst.pos << ", "
       << "];\n";
  });
  os << "}\n";
  return os;
}

#define DUMP_GRAPHVIZ_PORTS(graph)                                             \
  {                                                                            \
    std::error_code EC;                                                        \
    llvm::raw_fd_ostream stream{StringRef{#graph ".dot"}, EC};                       \
    assert(!EC);                                                               \
    dump_graphviz_ports(stream, graph);                                        \
  }

#define DUMP_GRAPHVIZ_PORT_PTRS(graph)                                         \
  {                                                                            \
    std::error_code EC;                                                        \
    llvm::raw_fd_ostream stream{StringRef{#graph ".dot"}, EC};                       \
    assert(!EC);                                                               \
    dump_graphviz_ports(stream, graph, true);                                  \
  }

