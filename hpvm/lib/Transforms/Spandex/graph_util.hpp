#ifndef GRAPH_UTIL_HPP
#define GRAPH_UTIL_HPP

#include <unordered_map>
#include <unordered_set>
#include <functional>
#include "type_util.hpp"

template <typename Node>
std::unordered_map<Node, std::unordered_set<Node>>
get_relaxed_edges(const std::unordered_map<Node, std::unordered_set<Node>> &edges,
                  std::function<bool(const Node &)> keep_node,
                  bool ignore_dead_ends = false) {
	std::unordered_map<Node, std::unordered_set<Node>> relaxed_edges;

  for (auto edge : edges) {
    Node src{edge.first};
    Node dst{edge.second};
    if (keep_node(src)) {
      Node eventual_dst = dst;
      while (!keep_node(eventual_dst)) {
        if (edges.count(eventual_dst) == 0) {
          if (ignore_dead_ends) {
            continue;
          } else {
            LLVM_DEBUG(dbgs() << src << " dead ends at " << dst
                              << " which is not keepable\n");
            assert(false);
          }
        } else {
          LLVM_DEBUG(dbgs() << src << " -> ... -> " << eventual_dst << " -> "
                            << edges.at(eventual_dst) << "\n");
          eventual_dst = edges.at(eventual_dst);
        }
      }
      assert(keep_node(src) && keep_node(eventual_dst));
      relaxed_edges[src].insert(eventual_dst);
    }
  }
  return relaxed_edges;
}

template <typename Node>
std::unordered_set<Node>
get_nodes(const std::unordered_map<Node, std::unordered_set<Node>> &edges) {
	std::unordered_set<Node> nodes;
  for (const auto &edge : edges) {
	  for (const auto &node : edge.second) {
		  nodes.insert(edge.first);
		  nodes.insert(node);
	  }
  }
  return nodes;
}

llvm::raw_ostream &dump_graphviz(llvm::raw_ostream &os,
                                 const std::unordered_map<Port, std::unordered_set<Port>> &edges) {
  os << "digraph structs {\n";

  os << "\tnode [shape=record];\n";

  std::unordered_set<DFNode const*> dfnodes;
  std::unordered_map<DFNode const*, unsigned int> dfnode_inps;
  std::unordered_map<DFNode const*, unsigned int> dfnode_outs;
  for (const auto &edge : edges) {
	  for (const auto &node : edge.second) {
		  auto& src = node;
		  auto& dst = edge.first;

		  // omit port info
		  dfnodes.insert(src.N);
		  dfnodes.insert(dst.N);

		  dfnode_inps[dst.N] = std::max(dfnode_inps[dst.N], dst.pos+1);
		  dfnode_outs[src.N] = std::max(dfnode_outs[src.N], src.pos+1);
	  }
  }

  for (const auto& node : dfnodes) {
	  os << "\t"
		 << "\"" << *node << "\" "
		 << "["
		 << "label=\"{";
	  for (size_t i = 0; i < dfnode_outs[node]; ++i) {
		  if (i != 0) {
			  os << "|";
		  }
		  os << "<i" << i << ">i" << i;
	  }
	  os << "}|" << *node << "|{";
	  for (size_t i = 0; i < dfnode_inps[node]; ++i) {
		  if (i != 0) {
			  os << "|";
		  }
		  os << "<o" << i << ">o" << i;
	  }
	  os << "}\""
		 << "];\n";
  }

  os << "\n";

  for (const auto &edge : edges) {
	  for (const auto &port : edge.second) {
		  auto src = edge.first;
		  auto dst = port;
		  // TODO: a way to escape quotes in strings
		  os << "\t"
			 << "\"" << *src.N << "\" "
			 << "-> "
			 << "\"" << *dst.N << "\" "
			 << "["
			 << "headport=i" << dst.pos << ", "
			 << "tailport=o" << src.pos << ", "
			 << "];\n";
	  }
  }
  os << "}\n";
  return os;
}

#endif
