#include <pr/graph.hpp>
#include <pr/mtx_parser.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <vector>

namespace {

std::vector<pr::NodeId> in_of(const pr::Graph& g, pr::NodeId j) {
    const auto s = g.in_neighbors(j);
    return {s.begin(), s.end()};
}

using V = std::vector<pr::NodeId>;

}  // namespace

TEST(Graph, DropsSelfLoopsAndDuplicates) {
    pr::EdgeList el;
    el.n_nodes = 4;
    el.edges   = {{0, 1}, {0, 1}, {2, 2}, {1, 2}, {3, 2}, {0, 2}};
    el.declared_edges = el.edges.size();

    const auto g = pr::Graph::from_edges(el);
    EXPECT_EQ(g.n_nodes(), 4u);
    EXPECT_EQ(g.n_edges(), 4u);    // (0,1) (1,2) (3,2) (0,2)
    EXPECT_EQ(g.n_invalid(), 2u);  // one duplicate + one self-loop

    EXPECT_EQ(in_of(g, 0), V{});
    EXPECT_EQ(in_of(g, 1), V{0});
    EXPECT_EQ(in_of(g, 2), (V{0, 1, 3}));  // sorted ascending
    EXPECT_EQ(in_of(g, 3), V{});

    EXPECT_EQ(g.out_degree(0), 2u);
    EXPECT_EQ(g.out_degree(1), 1u);
    EXPECT_EQ(g.out_degree(2), 0u);
    EXPECT_EQ(g.out_degree(3), 1u);
    EXPECT_EQ(g.n_dead_ends(), 1u);
}

TEST(Graph, NoEdgesMeansAllDeadEnds) {
    pr::EdgeList el;
    el.n_nodes = 3;
    const auto g = pr::Graph::from_edges(el);
    EXPECT_EQ(g.n_edges(), 0u);
    EXPECT_EQ(g.n_dead_ends(), 3u);
    for (pr::NodeId j = 0; j < 3; ++j) {
        EXPECT_TRUE(g.in_neighbors(j).empty());
    }
}

TEST(Graph, NineNodesMatchesLegacyOracle) {
    const auto el = pr::read_mtx(std::filesystem::path(PR_TEST_DATA_DIR) / "9nodi.mtx");
    ASSERT_TRUE(el.has_value()) << el.error();
    const auto g = pr::Graph::from_edges(*el);

    EXPECT_EQ(g.n_nodes(), 9u);
    EXPECT_EQ(g.n_edges(), 11u);    // "Number of valid arcs: 11" in 9nodi.sol
    EXPECT_EQ(g.n_invalid(), 8u);   // 19 - 11
    EXPECT_EQ(g.n_dead_ends(), 2u); // "Number of dead-end nodes: 2"
}

TEST(Graph, CsrInvariantsOnNineNodes) {
    const auto el = pr::read_mtx(std::filesystem::path(PR_TEST_DATA_DIR) / "9nodi.mtx");
    ASSERT_TRUE(el.has_value());
    const auto g = pr::Graph::from_edges(*el);

    std::vector<std::uint32_t> out(g.n_nodes(), 0);
    std::uint64_t total = 0;
    for (pr::NodeId j = 0; j < g.n_nodes(); ++j) {
        const auto row = g.in_neighbors(j);
        total += row.size();
        EXPECT_TRUE(std::is_sorted(row.begin(), row.end())) << "row " << j;
        EXPECT_TRUE(std::adjacent_find(row.begin(), row.end()) == row.end()) << "dup in row " << j;
        for (const pr::NodeId i : row) {
            EXPECT_NE(i, j) << "self-loop in row " << j;
            ++out[i];
        }
    }
    EXPECT_EQ(total, g.n_edges());
    for (pr::NodeId i = 0; i < g.n_nodes(); ++i) {
        EXPECT_EQ(out[i], g.out_degree(i)) << "node " << i;
    }
}
