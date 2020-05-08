#ifndef GRAPH_UTIL_HPP
#define GRAPH_UTIL_HPP

#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <algorithm>
#include <functional>
#include "type_util.hpp"

template <typename Node>
std::unordered_map<Node, Node>
get_relaxed_edges(const std::unordered_map<Node, Node> &edges,
                  std::function<bool(const Node &)> keep_node,
                  bool ignore_dead_ends = false) {
  std::unordered_map<Node, Node> relaxed_edges;

  for (auto edge : edges) {
    Node src{edge.first};
    Node dst{edge.second};
    if (keep_node(src)) {
      Node descendants = dst;
      while (!keep_node(descendants)) {
        if (edges.count(descendants) == 0) {
          if (ignore_dead_ends) {
            goto break_outer;
          } else {
            LLVM_DEBUG(dbgs() << src << " dead ends at " << dst
                              << " which is not keepable\n");
            assert(false);
          }
        } else {
          // LLVM_DEBUG(dbgs() << src << " -> ... -> " << descendants << " -> "
          //                   << edges.at(descendants) << "\n");
          descendants = edges.at(descendants);
        }
      }
      assert(keep_node(src) && keep_node(descendants));
      relaxed_edges[src].insert(descendants);
    break_outer:;
    }
  }
  return relaxed_edges;
}

template <typename Node>
std::unordered_set<Node>
get_nodes(const std::unordered_map<Node, std::unordered_set<Node>> &edges) {
  std::unordered_set<Node> nodes;
  for (const auto &edge : edges) {
    nodes.insert(edge.first);
    for (const auto &node : edge.second) {
      nodes.insert(node);
    }
  }
  return nodes;
}

template <typename Node> class bfs_iterator {
public:
  bfs_iterator() : valid_cur{false} {}

  bfs_iterator(const std::unordered_map<Node, std::unordered_set<Node>> &_edges,
               Node src)
      : edges{_edges}, cur{src}, valid_cur{true} {}

  bool empty() { return !valid_cur; }
  Node &operator*() {
    assert(!empty() && "Accessed a completed iterator");
    return cur;
  }
  Node *operator->() { return &cur; }
  bfs_iterator &operator++() {
    // only owrks in acyclic graph
    assert(!empty() && "Incremented a completed iterator");
    if (edges.count(cur) != 0) {
      for (const auto &next : edges.at(cur)) {
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
  const std::unordered_map<Node, std::unordered_set<Node>> edges;
  Node cur;
  bool valid_cur;
  std::deque<Node> lst;
};

template <typename Node>
std::vector<Node>
bfs(const std::unordered_map<Node, std::unordered_set<Node>> &edges, Node src) {
  std::vector<Node> ret;
  for (bfs_iterator<Node> it{edges, src}; !it.empty(); ++it) {
    // LLVM_DEBUG(dbgs() << src << " -> ... -> " << *it << "\n");
    ret.push_back(*it);
  }
  return ret;
}

template <typename Node>
bool is_descendant(
    const std::unordered_map<Node, std::unordered_set<Node>> &edges, Node n1,
    Node n2) {
  auto descendants = bfs<Node>(edges, n1);
  return std::find(std::cbegin(descendants), std::cend(descendants), n2) !=
         std::cend(descendants);
}

template <typename Node1, typename Node2>
std::unordered_map<Node2, std::unordered_set<Node2>>
map_graph(const std::unordered_map<Node1, std::unordered_set<Node1>> &inp,
          std::function<Node2(const Node1 &)> fn) {
  std::unordered_map<Node2, std::unordered_set<Node2>> out;
  for (const auto &edge : inp) {
    Node2 fn_src = fn(edge.first);
    for (Node1 dst : edge.second) {
      Node2 fn_dst = fn(dst);
      out[fn_src].insert(fn_dst);
    }
  }
  return out;
}

template <typename Node>
llvm::raw_ostream &
dump_graphviz(llvm::raw_ostream &os,
              const std::unordered_map<Node, std::unordered_set<Node>> &edges) {
  os << "strict digraph {\n";

  std::unordered_set<DFNode const *> nodes;

  for (const auto &node : get_nodes(edges)) {
    os << "\t"
       << "\"" << node << "\" "
       << ";\n";
  }

  os << "\n";

  for (const auto &edge : edges) {
    for (const auto &port : edge.second) {
      auto src = edge.first;
      auto dst = port;
      // TODO: a way to escape quotes in strings
      os << "\t"
         << "\"" << src << "\" "
         << "-> "
         << "\"" << dst << "\" "
         << ";\n";
    }
  }
  os << "}\n";
  return os;
}

llvm::raw_ostream &dump_graphviz_ports(
    llvm::raw_ostream &os,
    const std::unordered_map<Port, std::unordered_set<Port>> &edges) {
  os << "digraph structs {\n";

  os << "\tnode [shape=record];\n";

  std::unordered_set<DFNode const *> dfnodes;
  std::unordered_map<DFNode const *, unsigned int> dfnode_inps;
  std::unordered_map<DFNode const *, unsigned int> dfnode_outs;
  for (const auto &edge : edges) {
    for (const auto &node : edge.second) {
      auto &src = node;
      auto &dst = edge.first;

      // omit port info
      dfnodes.insert(src.N);
      dfnodes.insert(dst.N);

      dfnode_inps[dst.N] = std::max(dfnode_inps[dst.N], dst.pos + 1);
      dfnode_outs[src.N] = std::max(dfnode_outs[src.N], src.pos + 1);
    }
  }

  for (const auto &node : dfnodes) {
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
