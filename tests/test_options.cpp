#include <pr/options.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

using Args = std::vector<std::string>;

// parse_args wants (argc, char*[]): build them from a list of strings.
// The program name is prepended automatically.
std::expected<pr::Options, std::string> parse(Args args) {
    args.insert(args.begin(), "pagerank");
    std::vector<char*> argv;
    for (auto& a : args) {
        argv.push_back(a.data());
    }
    argv.push_back(nullptr);
    return pr::parse_args(static_cast<int>(args.size()), argv.data());
}

}  // namespace

// ---------------------------------------------------------------------------
// Valid command lines
// ---------------------------------------------------------------------------

TEST(Options, Defaults) {
    const auto o = parse({"g.mtx"});
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ(o->input.string(), "g.mtx");
    EXPECT_DOUBLE_EQ(o->damping, pr::kDefaultDamping);
    EXPECT_DOUBLE_EQ(o->eps, pr::kDefaultEps);
    EXPECT_EQ(o->max_iter, pr::kDefaultMaxIter);
    EXPECT_EQ(o->top, pr::kDefaultTop);
    EXPECT_EQ(o->threads, pr::kDefaultThreads);
    EXPECT_FALSE(o->show_help);
}

TEST(Options, AllOptionsSeparatedValue) {
    const auto o = parse({"-d", "0.85", "-e", "1e-9", "-m", "200", "-k", "10", "-t", "4", "g.mtx"});
    ASSERT_TRUE(o.has_value());
    EXPECT_DOUBLE_EQ(o->damping, 0.85);
    EXPECT_DOUBLE_EQ(o->eps, 1e-9);
    EXPECT_EQ(o->max_iter, 200);
    EXPECT_EQ(o->top, 10);
    EXPECT_EQ(o->threads, 4);
}

TEST(Options, AttachedValues) {
    const auto o = parse({"-k5", "-d0.85", "g.mtx"});
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ(o->top, 5);
    EXPECT_DOUBLE_EQ(o->damping, 0.85);
}

TEST(Options, LastOccurrenceWins) {
    const auto o = parse({"-k", "3", "-k", "7", "g.mtx"});
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ(o->top, 7);
}

TEST(Options, LowestValidBounds) {
    const auto o = parse({"-d", "0.000001", "-e", "1e-300", "-m", "1", "-k", "1", "-t", "1", "g.mtx"});
    ASSERT_TRUE(o.has_value());
}

TEST(Options, HighestValidDamping) {
    const auto o = parse({"-d", "0.999999", "g.mtx"});
    ASSERT_TRUE(o.has_value());
}

TEST(Options, HelpDoesNotNeedInputFile) {
    const auto o = parse({"-h"});
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(o->show_help);
}

// ---------------------------------------------------------------------------
// Invalid command lines: every one must be rejected with a message
// ---------------------------------------------------------------------------

class InvalidArgs : public ::testing::TestWithParam<Args> {};

TEST_P(InvalidArgs, IsRejected) {
    const auto o = parse(GetParam());
    ASSERT_FALSE(o.has_value());
    EXPECT_FALSE(o.error().empty());
}

INSTANTIATE_TEST_SUITE_P(
    Options, InvalidArgs,
    ::testing::Values(
        // damping: 0 < d < 1, finite, a real number
        Args{"-d", "0", "g.mtx"}, Args{"-d", "1", "g.mtx"}, Args{"-d", "1.5", "g.mtx"},
        Args{"-d", "-0.1", "g.mtx"}, Args{"-d", "nan", "g.mtx"}, Args{"-d", "inf", "g.mtx"},
        Args{"-d", "abc", "g.mtx"}, Args{"-d", "", "g.mtx"}, Args{"-d", "0.5x", "g.mtx"},
        // eps: > 0, finite
        Args{"-e", "0", "g.mtx"}, Args{"-e", "-1e-7", "g.mtx"}, Args{"-e", "nan", "g.mtx"},
        Args{"-e", "1e999", "g.mtx"},
        // integers: >= 1, whole string, no overflow
        Args{"-m", "0", "g.mtx"}, Args{"-m", "5x", "g.mtx"}, Args{"-m", "1.5", "g.mtx"},
        Args{"-m", "99999999999", "g.mtx"}, Args{"-k", "0", "g.mtx"},
        Args{"-k", "-3", "g.mtx"}, Args{"-t", "0", "g.mtx"},
        // structure of the command line
        Args{"-z", "g.mtx"},          // unknown option
        Args{"g.mtx", "-k"},          // missing value
        Args{"-k"},                   // missing value and file
        Args{},                       // no input file
        Args{"-k", "3"},              // options but no input file
        Args{"a.mtx", "b.mtx"}));     // two input files
