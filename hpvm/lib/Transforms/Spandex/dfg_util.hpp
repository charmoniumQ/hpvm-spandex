#ifndef DFG_UTIL_HPP
#define DFG_UTIL_HPP

/*
Utilities specific to HPVM DFGs.
*/

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "SupportHPVM/DFGraph.h"
#include "type_util.hpp"
#include "graph_util.hpp"

using namespace llvm;

raw_ostream &dump_graphviz_ports(raw_ostream &os, const digraph<Port> &dfg,
                                 bool inps_only = false) {
  os << "digraph structs {\n";

  os << "\tnode [shape=record];\n";

  std::unordered_set<DFNode const *> dfnodes;
  for_each_adj<Port>(dfg, [&](const Port &src, const Port &dst) {
    // omit port info
    dfnodes.insert(src.N);
    dfnodes.insert(dst.N);
  });

  for (DFNode const *node : dfnodes) {
    os << "\t"
       << "\"" << *node << "\" "
       << "["
       << "label=\"{";
    Function *fn = node->getFuncPointer();
    StructType *ST = node->getOutputType();
    if (node->isExitNode()) {
      unsigned i = 0;
      for (auto elem_it = ST->element_begin(); elem_it != ST->element_end();
           ++elem_it, ++i) {
        if (i != 0) {
          os << "|";
        }
        os << "<i" << i << ">" << **elem_it;
      }
    } else {
      unsigned i = 0;
      for (auto arg_it = fn->arg_begin(); arg_it != fn->arg_end();
           ++arg_it, ++i) {
        if (i != 0) {
          os << "|";
        }
        os << "<i" << i << ">" << *arg_it->getType() << " "
           << arg_it->getName();
      }
    }
    os << "}|";
    os << *node;
    if (!inps_only) {
      os << "|{";
      if (node->isEntryNode()) {
        unsigned i = 0;
        for (auto arg_it = fn->arg_begin(); arg_it != fn->arg_end();
             ++arg_it, ++i) {
          if (i != 0) {
            os << "|";
          }
          os << "<o" << i << ">" << *arg_it->getType();
        }
      } else {
        unsigned i = 0;
        for (auto elem_it = ST->element_begin(); elem_it != ST->element_end();
             ++elem_it, ++i) {
          if (i != 0) {
            os << "|";
          }
          os << "<o" << i << ">" << **elem_it;
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
       << "\"" << *src.N << "\" "
       << "-> "
       << "\"" << *dst.N << "\" "
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
    raw_fd_ostream stream{StringRef{#graph ".dot"}, EC};                       \
    assert(!EC);                                                               \
    dump_graphviz_ports(stream, graph);                                        \
  }

#define DUMP_GRAPHVIZ_PORT_PTRS(graph)                                         \
  {                                                                            \
    std::error_code EC;                                                        \
    raw_fd_ostream stream{StringRef{#graph ".dot"}, EC};                       \
    assert(!EC);                                                               \
    dump_graphviz_ports(stream, graph, true);                                  \
  }

class get_dfg_helper : public DFNodeVisitor {
private:
  digraph<Port> dfg;

public:
  const DFNode *normalize(const DFNode *orig, bool src) {
    if (orig->getKind() == DFNode::InternalNode) {
      DFInternalNode *orig2 = const_cast<DFInternalNode *>(
          reinterpret_cast<const DFInternalNode *>(orig));
      DFGraph *g = orig2->getChildGraph();
      return src ? g->getExit() : g->getEntry();
    } else {
      return orig;
    }
  }

  virtual void visit2(const DFNode *N) {
    for (auto edge = N->outdfedge_begin(); edge != N->outdfedge_end(); ++edge) {
      Port src{normalize((*edge)->getSourceDF(), true),
               (*edge)->getSourcePosition()};
      Port dst{normalize((*edge)->getDestDF(), false),
               (*edge)->getDestPosition()};
      // LLVM_DEBUG(dbgs() << src << " -> " << dst << "\n");
      dfg[src].insert(dst);
    }
  }

  virtual void visit(DFInternalNode *N) {
    visit2(reinterpret_cast<const DFNode *>(N));
    for (DFNode *child : *N->getChildGraph()) {
      child->applyDFNodeVisitor(*this);
    }
  }
  virtual void visit(DFLeafNode *N) {
    visit2(reinterpret_cast<const DFNode *>(N));
  }
  digraph<Port> get_dfg() { return dfg; }
};

digraph<Port> get_dfg(const DFInternalNode *N) {
  get_dfg_helper gdh;
  // I know this visitor does not modify the graph a priori
  // but the graph visitor API (not owned by me) is marked as non-const
  const_cast<DFInternalNode *>(N)->applyDFNodeVisitor(gdh);
  return gdh.get_dfg();
}

digraph<Port> get_mem_comm_dfg(const digraph<Port> &dfg,
                               const digraph<DFNode const *> &coarse_dfg) {
  digraph<Port> result;
  for_each_adj_list<Port>(dfg, [&](const Port &src, auto dsts) {
    if (dsts.size() > 1 && dsts.cbegin()->get_type()->isPointerTy()) {
      for (const Port &dst1 : dsts) {
        for (const Port &dst2 : dsts) {
          if (dst1 != dst2 && dst1.N != dst2.N) {
            if (true &&
                dst1.N->getFuncPointer()->hasAttribute(dst1.pos + 1,
                                                       Attribute::Out) &&
                dst2.N->getFuncPointer()->hasAttribute(dst2.pos + 1,
                                                       Attribute::In) &&
                is_descendant(coarse_dfg, dst1.N, dst2.N)) {
              result[dst1].insert(dst2);
            }
          }
        }
      }
    }
  });
  return result;
}

#endif
