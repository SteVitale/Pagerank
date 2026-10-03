// include/pr/options.hpp
#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace pr {

inline constexpr double       kDefaultDamping = 0.9;
inline constexpr double       kDefaultEps     = 1.0e-7;
inline constexpr std::int32_t kDefaultMaxIter = 100;
inline constexpr std::int32_t kDefaultTop     = 3;
inline constexpr std::int32_t kDefaultThreads = 1;

/// Validated command-line options.
/// Invariants when parse_args succeeds: 0 < damping < 1, eps finite and > 0,
/// max_iter >= 1, top >= 1, threads >= 1, input non-empty (unless show_help).
/// `top` is clamped to the number of nodes later, once the graph is loaded.
struct Options {
    double                damping   = kDefaultDamping;
    double                eps       = kDefaultEps;
    std::int32_t          max_iter  = kDefaultMaxIter;
    std::int32_t          top       = kDefaultTop;
    std::int32_t          threads   = kDefaultThreads;
    std::filesystem::path input;
    bool                  show_help = false;
};

/// Parses the command line (POSIX getopt). Returns the error message on failure.
[[nodiscard]] std::expected<Options, std::string> parse_args(int argc, char* argv[]);

[[nodiscard]] std::string usage(std::string_view program_name);

}  // namespace pr
