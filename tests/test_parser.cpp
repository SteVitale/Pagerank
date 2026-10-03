#include <pr/mtx_parser.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

// Writes `content` to a temporary file, removed on destruction.
class TempFile {
public:
    explicit TempFile(const std::string& content) {
        static std::atomic<int> counter{0};
        path_ = std::filesystem::temp_directory_path() /
                ("pr_test_" + std::to_string(::getpid()) + "_" + std::to_string(counter++) + ".mtx");
        std::ofstream(path_, std::ios::binary) << content;
    }
    ~TempFile() { std::filesystem::remove(path_); }
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace

TEST(Parser, NineNodesFile) {
    const auto el = pr::read_mtx(std::filesystem::path(PR_TEST_DATA_DIR) / "9nodi.mtx");
    ASSERT_TRUE(el.has_value()) << el.error();
    EXPECT_EQ(el->n_nodes, 9u);
    EXPECT_EQ(el->declared_edges, 19u);
    ASSERT_EQ(el->edges.size(), 19u);
    // first line "5 6" -> 0-based (4, 5); last line "3 8" -> (2, 7)
    EXPECT_EQ(el->edges.front().src, 4u);
    EXPECT_EQ(el->edges.front().dst, 5u);
    EXPECT_EQ(el->edges.back().src, 2u);
    EXPECT_EQ(el->edges.back().dst, 7u);
}

TEST(Parser, CommentsBlankLinesAndCrLf) {
    const TempFile f("%%MatrixMarket matrix coordinate pattern general\r\n"
                     "% a comment\r\n"
                     "\r\n"
                     "3 3 2\r\n"
                     "% comment between arcs\r\n"
                     "1 2\r\n"
                     "\r\n"
                     "3 1\r\n");
    const auto el = pr::read_mtx(f.path());
    ASSERT_TRUE(el.has_value()) << el.error();
    EXPECT_EQ(el->n_nodes, 3u);
    EXPECT_EQ(el->edges.size(), 2u);
}

TEST(Parser, WeightColumnIsIgnored) {
    const TempFile f("2 2 1\n1 2 0.5\n");
    const auto el = pr::read_mtx(f.path());
    ASSERT_TRUE(el.has_value()) << el.error();
    EXPECT_EQ(el->edges.size(), 1u);
}

TEST(Parser, MissingFile) {
    const auto el = pr::read_mtx("/nonexistent/dir/nope.mtx");
    EXPECT_FALSE(el.has_value());
}

class BadFile : public ::testing::TestWithParam<std::string> {};

TEST_P(BadFile, IsRejected) {
    const TempFile f(GetParam());
    const auto el = pr::read_mtx(f.path());
    ASSERT_FALSE(el.has_value());
    EXPECT_FALSE(el.error().empty());
}

INSTANTIATE_TEST_SUITE_P(
    Parser, BadFile,
    ::testing::Values(
        std::string{""},                       // empty file
        std::string{"% only comments\n"},      // no header
        std::string{"3 3\n"},                  // header with 2 numbers
        std::string{"3 4 0\n"},                // not square
        std::string{"0 0 0\n"},                // no nodes
        std::string{"3 3 1\n0 1\n"},           // index 0
        std::string{"3 3 1\n1 4\n"},           // index N+1
        std::string{"3 3 1\n-1 2\n"},          // negative index
        std::string{"3 3 1\n1\n"},             // truncated line
        std::string{"3 3 1\n1 x\n"},           // not a number
        std::string{"3 3 1\n1 2.5\n"},         // not an integer
        std::string{"3 3 2\n1 2\n"},           // fewer arcs than declared
        std::string{"3 3 1\n1 2\n2 3\n"}));    // more arcs than declared
