// src/mtx_parser.cpp
#include <pr/mtx_parser.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <format>
#include <fstream>
#include <limits>

namespace pr {
namespace {

// Blank line or '%' comment.
bool skippable(const std::string& s) {
    const auto p = s.find_first_not_of(" \t\r");
    return p == std::string::npos || s[p] == '%';
}

// Reads exactly K unsigned integers from the start of `s`.
// Anything after the K-th number is ignored (e.g. the weight column of
// "coordinate real" files).
template <std::size_t K>
bool read_numbers(std::string_view s, std::array<std::uint64_t, K>& out) {
    const char* p   = s.data();
    const char* end = p + s.size();
    for (auto& v : out) {
        while (p != end && (*p == ' ' || *p == '\t' || *p == '\r')) {
            ++p;
        }
        const auto [next, ec] = std::from_chars(p, end, v);
        if (ec != std::errc{}) {
            return false;
        }
        if (next != end && *next != ' ' && *next != '\t' && *next != '\r') {
            return false;  // e.g. "5x" or "5.5"
        }
        p = next;
    }
    return true;
}

}  // namespace

std::expected<EdgeList, std::string> read_mtx(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return std::unexpected(std::format("cannot open '{}'", path.string()));
    }

    std::string line;
    std::size_t line_no = 0;

    // --- header: first line that is not a comment ---------------------------
    bool have_header = false;
    while (!have_header && std::getline(in, line)) {
        ++line_no;
        have_header = !skippable(line);
    }
    std::array<std::uint64_t, 3> h{};  // rows, cols, arcs
    if (!have_header || !read_numbers(line, h)) {
        return std::unexpected("invalid or missing header (expected: rows cols arcs)");
    }
    if (h[0] != h[1]) {
        return std::unexpected(std::format("matrix is not square ({} x {})", h[0], h[1]));
    }
    if (h[0] == 0 || h[0] > std::numeric_limits<NodeId>::max()) {
        return std::unexpected(std::format("unsupported number of nodes: {}", h[0]));
    }

    EdgeList el;
    el.n_nodes        = static_cast<NodeId>(h[0]);
    el.declared_edges = h[2];
    // The header is untrusted: do not reserve more than ~16M arcs up front.
    el.edges.reserve(std::min<std::uint64_t>(h[2], std::uint64_t{1} << 24));

    // --- arcs ---------------------------------------------------------------
    std::array<std::uint64_t, 2> e{};
    while (std::getline(in, line)) {
        ++line_no;
        if (skippable(line)) {
            continue;
        }
        if (!read_numbers(line, e)) {
            return std::unexpected(std::format("line {}: malformed arc", line_no));
        }
        if (e[0] < 1 || e[0] > h[0] || e[1] < 1 || e[1] > h[0]) {
            return std::unexpected(
                std::format("line {}: node index out of range [1, {}]", line_no, h[0]));
        }
        el.edges.push_back({static_cast<NodeId>(e[0] - 1), static_cast<NodeId>(e[1] - 1)});
    }

    if (el.edges.size() != el.declared_edges) {
        return std::unexpected(std::format("header declares {} arcs but the file contains {}",
                                           el.declared_edges, el.edges.size()));
    }
    return el;
}

}  // namespace pr
