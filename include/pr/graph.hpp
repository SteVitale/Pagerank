// include/pr/graph.hpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace pr {

using NodeId  = std::uint32_t;  // N < 2^32
using EdgeIdx = std::uint64_t;  // M may exceed 2^32

struct Edge {
    NodeId src, dst;
};

/// Raw arcs as read from a file: may contain self-loops and duplicates.
struct EdgeList {
    NodeId            n_nodes{};
    std::uint64_t     declared_edges{};
    std::vector<Edge> edges;
};

/// Immutable directed graph in CSR form over the TRANSPOSED graph:
/// the arcs entering node j are sources[offsets[j] .. offsets[j+1]).
///
/// Invariants: no self-loops, no duplicate arcs, each row sorted ascending,
/// out_degree[i] = number of times i appears in `sources`.
class Graph {
public:
    /// Builds the graph, discarding self-loops and duplicate arcs.
    [[nodiscard]] static Graph from_edges(const EdgeList& el);

    [[nodiscard]] NodeId n_nodes() const noexcept { return n_; }
    [[nodiscard]] EdgeIdx n_edges() const noexcept { return sources_.size(); }
    /// Arcs of the input that were discarded (self-loops + duplicates).
    [[nodiscard]] std::uint64_t n_invalid() const noexcept { return invalid_; }
    [[nodiscard]] NodeId n_dead_ends() const noexcept { return dead_ends_; }

    [[nodiscard]] std::span<const NodeId> in_neighbors(NodeId j) const noexcept {
        return {sources_.data() + offsets_[j], sources_.data() + offsets_[j + 1]};
    }
    [[nodiscard]] std::uint32_t out_degree(NodeId i) const noexcept { return out_deg_[i]; }

private:
    NodeId                     n_{};
    std::uint64_t              invalid_{};
    NodeId                     dead_ends_{};
    std::vector<EdgeIdx>       offsets_;  // size N+1
    std::vector<NodeId>        sources_;  // size M
    std::vector<std::uint32_t> out_deg_;  // size N
};

}  // namespace pr
