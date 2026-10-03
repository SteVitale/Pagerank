// include/pr/pagerank.hpp
#pragma once

#include <pr/graph.hpp>

#include <cstdint>
#include <vector>

namespace pr {

struct PageRankResult {
    std::vector<double> rank;        // rank[j], sums to 1
    std::int32_t        iterations;  // iterations performed
    bool                converged;   // L1 error < eps reached within max_iter
    double              error;       // L1 error of the last iteration
};

/// Sequential PageRank (pull formulation).
///   x'_j = (1-d)/N + d * ( sum_{i in IN(j)} x_i/out(i) + (1/N) * sum_{dead-end i} x_i )
/// starting from x = 1/N, until ||x' - x||_1 < eps or max_iter iterations.
[[nodiscard]] PageRankResult pagerank(const Graph& g, double damping, double eps,
                                      std::int32_t max_iter);

}  // namespace pr
