#ifndef GRAPH_UTIL_HPP
#define GRAPH_UTIL_HPP

/*
Utilities for generic digraphs, described as

template <typename Node>
std::unordered_map<Node, std::unordered_set<Node>>

*/

#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <algorithm>
#include <type_traits>
#include <functional>
#include "type_util.hpp"

template <typename Node> using adj_list = std::unordered_set<Node>;

template <typename Node>
using digraph = std::unordered_map<Node, adj_list<Node>>;

template <typename Node>
void for_each_adj_list(
    const digraph<Node> &digraph_,
    std::function<void(const Node &, const adj_list<Node> &)> fn) {
  for (const auto &node_successors : digraph_) {
    const Node &src = node_successors.first;
    const std::unordered_set<Node> &dsts = node_successors.second;
    fn(src, dsts);
  }
}

template <typename Node>
void for_each_adj(const digraph<Node> &digraph_,
                  std::function<void(const Node &, const Node &)> fn) {
  for_each_adj_list<Node>(digraph_,
                          [&](const Node &src, const adj_list<Node> &dsts) {
                            for (const Node &dst : dsts) {
                              fn(src, dst);
                            }
                          });
}

template <typename Node>
std::unordered_set<Node> get_nodes(const digraph<Node> &digraph_) {
  std::unordered_set<Node> nodes;
  for_each_adj_list<Node>(digraph_, [&](const Node &src, adj_list<Node> dsts) {
    nodes.insert(src);
    for (const Node &dst : dsts) {
      nodes.insert(dst);
    }
  });
  return nodes;
}

template <typename Node> digraph<Node> invert(const digraph<Node> &digraph_) {
  digraph<Node> inverse;
  for_each_adj<Node>(digraph_, [&](const Node &src, const Node &dst) {
    inverse[dst].insert(src);
  });
  return inverse;
}

template <typename Node> class bfs_iterator {
public:
  bfs_iterator() : valid_cur{false} {}

  bfs_iterator(const digraph<Node> &digraph_, Node src)
      : _digraph{digraph_}, cur{src}, valid_cur{true} {}

  bool empty() { return !valid_cur; }
  Node &operator*() {
    assert(!empty() && "Accessed a completed iterator");
    return cur;
  }
  Node *operator->() { return &cur; }
  bfs_iterator &operator++() {
    // only owrks in acyclic graph
    assert(!empty() && "Incremented a completed iterator");
    if (_digraph.count(cur) != 0) {
      for (const auto &next : _digraph.at(cur)) {
        lst.push_back(next);
      }
    }
    if (lst.empty()) {
      valid_cur = false;
    } else {
      cur = lst.front();
      lst.pop_front();
    }
    return *this;
  }
  bool operator==(const bfs_iterator &other) {
    return cur == other.cur || (empty() && other.empty());
  }
  bool operator!=(const bfs_iterator &other) { return !(*this == other); }
  bfs_iterator operator++(int) {
    bfs_iterator other = *this;
    ++*this;
    return other;
  }
  // Iterator traits - typedefs and types required to be STL compliant
  typedef std::ptrdiff_t difference_type;
  typedef Node value_type;
  typedef Node *pointer;
  typedef Node &reference;
  typedef size_t size_type;
  std::input_iterator_tag iterator_category;

private:
  const digraph<Node> _digraph;
  Node cur;
  bool valid_cur;
  std::deque<Node> lst;
};

/*
 * TODO: replace this with bfs_iterator
 * For some generic-type-related reason, bfs_iterator does not work with
 * std::find
 */
template <typename Node>
std::vector<Node> bfs(const digraph<Node> &digraph_, Node src) {
	std::vector<Node> ret;
  for (bfs_iterator<Node> it{digraph_, src}; !it.empty(); ++it) {
    // LLVM_DEBUG(dbgs() << src << " -> ... -> " << *it << "\n");
    ret.push_back(*it);
  }
  return ret;
}

template <typename Node>
bool is_descendant(const digraph<Node> &digraph_, Node n1, Node n2) {
	auto descendants = bfs<Node>(digraph_, n1);
  return std::find(descendants.cbegin(), descendants.cend(), n2) !=
	  descendants.cend();
}

template <typename Node1, typename Node2>
digraph<Node2> map_graph(const digraph<Node1> &inp,
                         std::function<Node2(const Node1 &)> fn) {
  digraph<Node2> out;

  for_each_adj_list<Node1>(inp, [&](const Node1 &src1, const adj_list<Node1>& dst1s) {
    Node2 src2 = fn(src1);
    for (const Node1 &dst1 : dst1s) {
      Node2 dst2 = fn(dst1);
      out[src2].insert(dst2);
    }
  });

  return out;
}

template <typename Node>
void delete_node(digraph<Node> &graph, digraph<Node> &inverse_graph,
                 const Node &node) {
  adj_list<Node> predecessors = inverse_graph[node];
  adj_list<Node> successors = graph[node];

  // dbgs() << "Deleting " << node << "{\n";
  // for (const Node& predecessor : predecessors) {
  // 	dbgs() << "  Deleting inp-edge " << predecessor << " -> " << node <<
  // "\n";
  // }
  // for (const Node& successor : successors) {
  // 	dbgs() << "  Deleting out-edge " << node << " -> " << successor << "\n";
  // }
  // for (const Node& predecessor : predecessors) {
  // 	for (const Node& successor : successors) {
  // 		dbgs() << "  Inserting " << predecessor << " -> " << successor
  // <<
  // "\n";
  // 	}
  // }
  // dbgs() << "}\n";

  // update inverse_graph
  {
    // delete its adjacency list
    inverse_graph.erase(node);

    // delete from every adjacency list it occurs (its successors)
    for (const Node &successor : successors) {
      assert(inverse_graph.at(successor).count(node) != 0 &&
             "Inverse graph is wrong");
      inverse_graph.at(successor).erase(node);
    }

    // bridge successors to predecessors
    for (const Node &successor : successors) {
      for (const Node &predecessor : predecessors) {
        inverse_graph.at(successor).insert(predecessor);
      }
    }
  }

  // update graph
  {
    // delete from every adjacency list it occurs (its predecessors)
    for (const Node &predecessor : predecessors) {
      assert(graph.at(predecessor).count(node) != 0 &&
             "Inverse graph is wrong");
      graph.at(predecessor).erase(node);
    }

    // delete its adjacency list
    graph.erase(node);

    // bridge predecessors to sucessors
    for (const Node &predecessor : predecessors) {
      for (const Node &successor : successors) {
        graph.at(predecessor).insert(successor);
      }
    }
  }
  assert(get_nodes(graph).count(node) == 0);
  assert(get_nodes(inverse_graph).count(node) == 0);
}

template <typename Node>
void delete_nodes(digraph<Node> &graph, std::function<bool(const Node &)> fn) {
	digraph<Node> inverse_graph = invert<Node>(graph);
	for (const Node &node : get_nodes<Node>(graph)) {
    if (fn(node)) {
      delete_node(graph, inverse_graph, node);
    }
  }
}

template <typename Node>
llvm::raw_ostream &dump_graphviz(llvm::raw_ostream &os,
                                 const digraph<Node> &digraph_) {
  os << "strict digraph {\n";

  std::unordered_set<DFNode const *> nodes;

  for (const Node &node : get_nodes(digraph_)) {
    os << "\t"
       << "\"" << node << "\" "
       << ";\n";
  }

  os << "\n";

  for_each_adj<Node>(digraph_, [&](const Node &src, const Node &dst) {
    // TODO: a way to escape quotes in strings
    os << "\t"
       << "\"" << src << "\" "
       << "-> "
       << "\"" << dst << "\" "
       << ";\n";
  });
  os << "}\n";
  return os;
}

#define DUMP_GRAPHVIZ(graph)                                                   \
  {                                                                            \
    std::error_code EC;                                                        \
    raw_fd_ostream stream{StringRef{#graph ".dot"}, EC};                       \
    assert(!EC);                                                               \
    dump_graphviz<decltype(graph)::key_type>(stream, graph);	\
  }

#endif
