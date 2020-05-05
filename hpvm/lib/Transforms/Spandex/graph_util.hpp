#include <unordered_map>
#include <functional>

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
      relaxed_edges.insert(make_pair(src, eventual_dst));
    }
  }
  return relaxed_edges;
}

template <typename Node>
llvm::raw_ostream &dump_graphviz(llvm::raw_ostream &os,
                                 const std::unordered_map<Node, Node> &edges) {
  os << "strict digraph {\n";
  for (const auto &edge : edges) {
    // TODO: a cleaner way to escape this
    os << "\t"
       << "\"" << edge.first << "\""
       << " -> "
       << "\"" << edge.second << "\""
       << ";\n";
  }
  os << "}\n";
  return os;
}
