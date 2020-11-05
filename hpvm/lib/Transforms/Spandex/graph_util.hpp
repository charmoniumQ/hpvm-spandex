#pragma once
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <algorithm>
#include <type_traits>
#include <functional>
#include "util.hpp"

/*
Utilities for generic digraphs, described as
*/

template <typename Node, typename Hash = std::hash<typename std::remove_const<typename remove_reference_wrapper<Node>::type>::type>>
using AdjList = std::unordered_set<Node, Hash>;

template <typename Node, typename Hash = std::hash<typename std::remove_const<typename remove_reference_wrapper<Node>::type>::type>>
using Digraph = std::unordered_map<Node, AdjList<Node, Hash>, Hash>;

template <typename Node>
void for_each_adj_list(
    const Digraph<Node> &digraph_,
    std::function<void(const Node &, const AdjList<Node> &)> fn) {
  for (const auto &node_successors : digraph_) {
    const Node &src = node_successors.first;
    const std::unordered_set<Node> &dsts = node_successors.second;
    fn(src, dsts);
  }
}

template <typename Node>
void for_each_adj(const Digraph<Node> &digraph_,
                  std::function<void(const Node &, const Node &)> fn) {
  for_each_adj_list<Node>(digraph_,
                          [&](const Node &src, const AdjList<Node> &dsts) {
                            for (const Node &dst : dsts) {
                              fn(src, dst);
                            }
                          });
}

template <typename Node>
std::unordered_set<Node> get_nodes(const Digraph<Node> &digraph_) {
  std::unordered_set<Node> nodes;
  for_each_adj_list<Node>(digraph_, [&](const Node &src, AdjList<Node> dsts) {
    nodes.insert(src);
    for (const Node &dst : dsts) {
      nodes.insert(dst);
    }
  });
  return nodes;
}

template <typename Node> Digraph<Node> invert(const Digraph<Node> &digraph_) {
  Digraph<Node> inverse;
  for_each_adj<Node>(digraph_, [&](const Node &src, const Node &dst) {
    inverse[dst].insert(src);
  });
  return inverse;
}

template <typename Node> class BfsIt
  // Iterator traits - typedefs and types required to be STL compliant
	: public std::iterator<std::input_iterator_tag, Node, size_t, const Node*, const Node&>
{
public:
  BfsIt() {}

  BfsIt(const Digraph<Node> &digraph_, Node src)
      : _digraph{digraph_}, cur{std::make_optional<Node>(src)} {}

	bool empty() const { return !cur.has_value(); }
	const Node &operator*() const {
    assert(!empty() && "Accessed a completed iterator");
    return *cur;
  }
  Node &operator*() {
    assert(!empty() && "Accessed a completed iterator");
    return *cur;
  }
	const Node *operator->() const { return **this; }
  Node *operator->() { return **this; }
  BfsIt &operator++() {
    // only works in acyclic graph
    assert(!empty() && "Incremented a completed iterator");
	i++;
    if (_digraph.count(*cur) != 0) {
      for (const auto &next : _digraph.at(*cur)) {
        lst.push_back(next);
      }
    }
    if (lst.empty()) {
		cur = std::nullopt;
    } else {
		cur = lst.front();
      lst.pop_front();
    }
    return *this;
  }
  BfsIt operator++(int) {
    BfsIt other = *this;
    ++*this;
    return other;
  }
  bool operator==(const BfsIt &other) const {
	  if (empty() || other.empty()) {
		  return empty() && other.empty();
	  } else {
		  return i == other.i && _digraph == other._digraph;
	  }
  }
  bool operator!=(const BfsIt &other) const { return !(*this == other); }

private:
	size_t i = 0;
  const Digraph<Node> _digraph;
	std::optional<Node> cur;
  std::deque<Node> lst;
};

/*
 * TODO: replace this with BfsIt
 * For some generic-type-related reason, BfsIt does not work with
 * std::find
 */
template <typename Node>
std::vector<Node> bfs(const Digraph<Node> &digraph_, Node src) {
	std::vector<Node> ret;
  for (BfsIt<Node> it{digraph_, src}; !it.empty(); ++it) {
    // LLVM_DEBUG(dbgs() << src << " -> ... -> " << *it << "\n");
    ret.push_back(*it);
  }
  return ret;
}

template <typename Node>
bool is_descendant(const Digraph<Node> &digraph_, Node n1, Node n2) {
	return std::find(BfsIt<Node>{digraph_, n1}, BfsIt<Node>{}, n2) !=
	  BfsIt<Node>{};
}

template <typename Node1, typename Node2>
Digraph<Node2> map_graph(const Digraph<Node1> &inp,
                         std::function<Node2(const Node1 &)> fn) {
  Digraph<Node2> out;

  for_each_adj_list<Node1>(inp, [&](const Node1 &src1, const AdjList<Node1>& dst1s) {
    Node2 src2 = fn(src1);
    for (const Node1 &dst1 : dst1s) {
      Node2 dst2 = fn(dst1);
      out[src2].insert(dst2);
    }
  });

  return out;
}

template <typename Node>
void delete_node(Digraph<Node> &graph, Digraph<Node> &inverse_graph,
                 const Node &node) {
  AdjList<Node> predecessors = inverse_graph[node];
  AdjList<Node> successors = graph[node];

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
void delete_nodes(Digraph<Node> &graph, std::function<bool(const Node &)> fn) {
	Digraph<Node> inverse_graph = invert<Node>(graph);
	for (const Node &node : get_nodes<Node>(graph)) {
    if (fn(node)) {
      delete_node(graph, inverse_graph, node);
    }
  }
}

template <typename Node>
llvm::raw_ostream &dump_graphviz(llvm::raw_ostream &os,
                                 const Digraph<Node> &digraph_) {
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
