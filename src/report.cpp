#include <pr/report.hpp>

#include <algorithm>
#include <format>
#include <numeric>
#include <vector>

namespace pr {

void print_report(std::ostream& out, const Graph& g, const PageRankResult& res, std::int32_t top) {
    out << std::format("Number of nodes: {}\n", g.n_nodes());
    out << std::format("Number of dead-end nodes: {}\n", g.n_dead_ends());
    out << std::format("Number of valid arcs: {}\n", g.n_edges());
    
    if (res.converged) {
        out << std::format("Converged after {} iterations\n", res.iterations);
    } else {
        out << std::format("Did not converge after {} iterations\n", res.iterations);
    }

    const double sum = std::accumulate(res.rank.begin(), res.rank.end(), 0.0);
    out << std::format("Sum of ranks: {:.4f}   (should be 1)\n", sum);

    const auto k = std::min<std::size_t>(top, res.rank.size());

    std::vector<NodeId> ids(res.rank.size());
    std::iota(ids.begin(), ids.end(), NodeId{0});

    std::partial_sort(
        ids.begin(),
        ids.begin() + k,
        ids.end(),
        [&](NodeId a, NodeId b) {
            return res.rank[a] > res.rank[b];
        }
    );

    out << std::format("Top {} nodes:\n", k);

    for (std::size_t i = 0; i < k; ++i) {
        out << std::format("  {} {:.6f}\n", ids[i], res.rank[ids[i]]);
    }
}

}  // namespace pr
