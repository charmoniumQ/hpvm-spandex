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

raw_ostream &dump_graphviz_ports(raw_ostream &os, const digraph<Port> &dfg) {
  os << "digraph structs {\n";

  os << "\tnode [shape=record];\n";

  std::unordered_set<DFNode const *> dfnodes;
  std::unordered_map<DFNode const *, unsigned int> dfnode_inps;
  std::unordered_map<DFNode const *, unsigned int> dfnode_outs;
  for_each_adj<Port>(dfg, [&](const Port &src, const Port &dst) {
    // omit port info
    dfnodes.insert(src.N);
    dfnodes.insert(dst.N);

    dfnode_inps[dst.N] = std::max(dfnode_inps[dst.N], dst.pos + 1);
    dfnode_outs[src.N] = std::max(dfnode_outs[src.N], src.pos + 1);
  });

  for (DFNode const *node : dfnodes) {
    os << "\t"
       << "\"" << *node << "\" "
       << "["
       << "label=\"{";
    for (size_t i = 0; i < dfnode_inps[node]; ++i) {
      if (i != 0) {
        os << "|";
      }
      os << "<i" << i << ">i" << i;
    }
    os << "}|" << *node << "|{";
    for (size_t i = 0; i < dfnode_outs[node]; ++i) {
      if (i != 0) {
        os << "|";
      }
      os << "<o" << i << ">o" << i;
    }
    os << "}\""
       << "];\n";
  }

  os << "\n";

  for_each_adj<Port>(dfg, [&](const Port &src, const Port &dst) {
    // TODO: a way to escape quotes in strings
    os << "\t"
       << "\"" << *src.N << "\" "
       << "-> "
       << "\"" << *dst.N << "\" "
       << "["
       << "tailport=o" << src.pos << ", "
       << "headport=i" << dst.pos << ", "
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

  const DFNode *normalize_dst(const DFNode *dst) {
    return isa<DFInternalNode>(dst)
               ? (reinterpret_cast<const DFInternalNode *>(dst))
                     ->getChildGraph()
                     ->getEntry()
               : dst;
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
  });
  return result;
}

#endif
