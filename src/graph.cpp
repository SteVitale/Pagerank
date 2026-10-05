#include <pr/graph.hpp>

#include <algorithm>
#include <cstdint>

namespace pr {

Graph Graph::from_edges(const EdgeList& el) {
    Graph g;
    g.n_ = el.n_nodes;

    std::vector<Edge> edges;
    edges.reserve(el.edges.size());

    // Remove self-loops.
    for (const Edge edge : el.edges) {
        if (edge.src == edge.dst) {
            ++g.invalid_;
            continue;
        }

        edges.push_back(edge);
    }

    // Sort by destination, then source.
    std::sort(edges.begin(), edges.end(),
        [](const Edge& a, const Edge& b) {
            if (a.dst != b.dst) {
                return a.dst < b.dst;
            }
            return a.src < b.src;
        });

    // Remove duplicate arcs.
    const auto last = std::unique(edges.begin(), edges.end(),
        [](const Edge& a, const Edge& b) {
            return a.src == b.src && a.dst == b.dst;
        });

    g.invalid_ +=
        static_cast<std::uint64_t>(edges.end() - last);

    edges.erase(last, edges.end());

    // Build CSR offsets.
    g.offsets_.assign(
        static_cast<std::size_t>(g.n_) + 1, 0);

    for (const Edge edge : edges) {
        ++g.offsets_[static_cast<std::size_t>(edge.dst) + 1];
    }

    for (NodeId i = 0; i < g.n_; ++i) {
        g.offsets_[i + 1] += g.offsets_[i];
    }

    // Store sources in CSR order.
    g.sources_.reserve(edges.size());

    for (const Edge edge : edges) {
        g.sources_.push_back(edge.src);
    }

    // Compute out-degrees.
    g.out_deg_.assign(g.n_, 0);

    for (const Edge edge : edges) {
        ++g.out_deg_[edge.src];
    }

    // Count dead-end nodes.
    g.dead_ends_ = 0;

    for (NodeId i = 0; i < g.n_; ++i) {
        if (g.out_deg_[i] == 0) {
            ++g.dead_ends_;
        }
    }

    return g;
}

} // namespace pr