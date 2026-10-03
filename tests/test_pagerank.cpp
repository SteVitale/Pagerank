#include <pr/graph.hpp>
#include <pr/mtx_parser.hpp>
#include <pr/pagerank.hpp>
#include <pr/report.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <numeric>
#include <sstream>

namespace {

constexpr double kTol = 1e-9;

pr::Graph make_graph(pr::NodeId n, std::vector<pr::Edge> edges) {
    pr::EdgeList el;
    el.n_nodes        = n;
    el.declared_edges = edges.size();
    el.edges          = std::move(edges);
    return pr::Graph::from_edges(el);
}

double sum_of(const std::vector<double>& v) { return std::accumulate(v.begin(), v.end(), 0.0); }

void expect_uniform(const pr::PageRankResult& r, pr::NodeId n) {
    EXPECT_TRUE(r.converged);
    ASSERT_EQ(r.rank.size(), n);
    for (const double x : r.rank) {
        EXPECT_NEAR(x, 1.0 / n, kTol);
    }
}

}  // namespace

TEST(PageRank, DirectedCycleIsUniform) {
    const pr::NodeId n = 6;
    std::vector<pr::Edge> e;
    for (pr::NodeId i = 0; i < n; ++i) e.push_back({i, (i + 1) % n});
    expect_uniform(pr::pagerank(make_graph(n, e), 0.85, 1e-12, 1000), n);
}

TEST(PageRank, CompleteGraphIsUniform) {
    const pr::NodeId n = 5;
    std::vector<pr::Edge> e;
    for (pr::NodeId i = 0; i < n; ++i)
        for (pr::NodeId j = 0; j < n; ++j)
            if (i != j) e.push_back({i, j});
    expect_uniform(pr::pagerank(make_graph(n, e), 0.85, 1e-12, 1000), n);
}

TEST(PageRank, OnlyDeadEndsIsUniform) {
    expect_uniform(pr::pagerank(make_graph(4, {}), 0.85, 1e-12, 1000), 4);
}

// Star: nodes 1..N-1 all point to node 0, which is a dead-end.
// Closed form (a = (1-d)/N):  c = a (1 + d (N-1)) / (1 - d/N - d^2 (N-1)/N),
// leaves:  l = a + d c / N.
TEST(PageRank, StarMatchesClosedForm) {
    const pr::NodeId n = 5;
    const double d     = 0.85;
    std::vector<pr::Edge> e;
    for (pr::NodeId i = 1; i < n; ++i) e.push_back({i, 0});

    const auto r = pr::pagerank(make_graph(n, e), d, 1e-14, 10000);
    ASSERT_TRUE(r.converged);

    const double a = (1.0 - d) / n;
    const double c = a * (1.0 + d * (n - 1)) / (1.0 - d / n - d * d * (n - 1) / n);
    const double l = a + d * c / n;
    EXPECT_NEAR(r.rank[0], c, 1e-10);
    for (pr::NodeId i = 1; i < n; ++i) EXPECT_NEAR(r.rank[i], l, 1e-10);
    EXPECT_NEAR(sum_of(r.rank), 1.0, 1e-12);
}

TEST(PageRank, StopsAtMaxIterWithoutConverging) {
    const auto el = pr::read_mtx(std::filesystem::path(PR_TEST_DATA_DIR) / "9nodi.mtx");
    ASSERT_TRUE(el.has_value());
    const auto r = pr::pagerank(pr::Graph::from_edges(*el), 0.9, 1e-12, 1);
    EXPECT_FALSE(r.converged);
    EXPECT_EQ(r.iterations, 1);
}

// End-to-end regression against the output of the legacy C program:
//   ./pagerank -e 1e-9 -k 5 9nodi.mtx   ==   9nodi.sol
TEST(PageRank, MatchesLegacyOracleOnNineNodes) {
    const std::filesystem::path dir{PR_TEST_DATA_DIR};
    const auto el = pr::read_mtx(dir / "9nodi.mtx");
    ASSERT_TRUE(el.has_value()) << el.error();

    const auto g = pr::Graph::from_edges(*el);
    const auto r = pr::pagerank(g, 0.9, 1e-9, 100);

    std::ostringstream actual;
    pr::print_report(actual, g, r, 5);

    std::ifstream sol(dir / "9nodi.sol");
    std::ostringstream expected;
    expected << sol.rdbuf();

    EXPECT_EQ(actual.str(), expected.str());
}
