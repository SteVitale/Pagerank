// src/graph.cpp
#include <pr/graph.hpp>

#include <algorithm>
#include <cassert>
#include <numeric>

namespace pr {

Graph Graph::from_edges(const EdgeList& el) {
    Graph g;
    g.n_ = el.n_nodes;

    // Pack each arc as (dst << 32 | src): sorting plain integers groups the
    // arcs by destination, sorted by source inside each group.
    std::vector<std::uint64_t> keys;
    keys.reserve(el.edges.size());
    for (const Edge& e : el.edges) {
        assert(e.src < g.n_ && e.dst < g.n_);
        if (e.src != e.dst) {  // drop self-loops
            keys.push_back((std::uint64_t{e.dst} << 32) | e.src);
        }
    }
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());  // drop duplicates

    g.invalid_ = el.edges.size() - keys.size();

    g.offsets_.assign(std::size_t{g.n_} + 1, 0);
    g.out_deg_.assign(g.n_, 0);
    g.sources_.reserve(keys.size());
    for (const std::uint64_t key : keys) {
        const auto dst = static_cast<NodeId>(key >> 32);
        const auto src = static_cast<NodeId>(key & 0xFFFFFFFFu);
        ++g.offsets_[std::size_t{dst} + 1];  // count arcs entering dst
        ++g.out_deg_[src];
        g.sources_.push_back(src);
    }
    std::partial_sum(g.offsets_.begin(), g.offsets_.end(), g.offsets_.begin());

    g.dead_ends_ = static_cast<NodeId>(std::count(g.out_deg_.begin(), g.out_deg_.end(), 0u));
    return g;
}

}  // namespace pr
