// src/pagerank.cpp
#include <pr/pagerank.hpp>

#include <cmath>
#include <utility>

namespace pr {

PageRankResult pagerank(const Graph& g, double damping, double eps, std::int32_t max_iter) {
    const NodeId n     = g.n_nodes();
    const double inv_n = 1.0 / static_cast<double>(n);

    std::vector<double> x(n, inv_n);  // rank at iteration t
    std::vector<double> x_next(n);    // rank at iteration t+1
    std::vector<double> contrib(n);   // x[i] / out(i), 0 for dead-ends

    PageRankResult res{{}, 0, false, 0.0};

    for (std::int32_t iter = 1; iter <= max_iter; ++iter) {
        // Pass 1: per-node contribution and total rank sitting on dead-ends.
        double dead_mass = 0.0;
        for (NodeId i = 0; i < n; ++i) {
            const std::uint32_t out = g.out_degree(i);
            if (out == 0) {
                contrib[i] = 0.0;
                dead_mass += x[i];
            } else {
                contrib[i] = x[i] / static_cast<double>(out);
            }
        }
        // Teleport + dead-end mass redistributed uniformly: same for every node.
        const double base = (1.0 - damping) * inv_n + damping * dead_mass * inv_n;

        // Pass 2: pull from incoming neighbours; accumulate the L1 error.
        double err = 0.0;
        for (NodeId j = 0; j < n; ++j) {
            double sum = 0.0;
            for (const NodeId i : g.in_neighbors(j)) {
                sum += contrib[i];
            }
            x_next[j] = base + damping * sum;
            err += std::fabs(x_next[j] - x[j]);
        }

        x.swap(x_next);
        res.iterations = iter;
        res.error      = err;
        if (err < eps) {
            res.converged = true;
            break;
        }
    }

    res.rank = std::move(x);
    return res;
}

}  // namespace pr
