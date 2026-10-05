#include <pr/mtx_parser.hpp>

#include <cstdint>
#include <format>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace pr {

namespace {

bool skippable(const std::string& line) {
    const auto pos = line.find_first_not_of(" \t\r");
    return pos == std::string::npos || line[pos] == '%';
}

} // namespace

std::expected<EdgeList, std::string>
read_mtx(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return std::unexpected(
            std::format("cannot open '{}'", path.string()));
    }

    std::string line;
    std::size_t line_no = 0;

    // Header
    while (std::getline(in, line)) {
        ++line_no;

        if (skippable(line)) {
            continue;
        }

        std::istringstream iss(line);

        std::uint64_t rows;
        std::uint64_t cols;
        std::uint64_t declared_edges;

        if (!(iss >> rows >> cols >> declared_edges)) {
            return std::unexpected(
                "invalid or missing header (expected: rows cols arcs)");
        }

        if (rows != cols) {
            return std::unexpected(
                std::format("matrix is not square ({} x {})", rows, cols));
        }

        if (rows == 0 ||
            rows > std::numeric_limits<NodeId>::max()) {
            return std::unexpected(
                std::format("unsupported number of nodes: {}", rows));
        }

        EdgeList result;
        result.n_nodes = static_cast<NodeId>(rows);
        result.declared_edges = declared_edges;
        result.edges.reserve(declared_edges);

        // Arcs
        while (std::getline(in, line)) {
            ++line_no;

            if (skippable(line)) {
                continue;
            }

            std::istringstream iss(line);

            std::uint64_t src;
            std::uint64_t dst;

            if (!(iss >> src >> dst)) {
                return std::unexpected(
                    std::format("line {}: malformed arc", line_no));
            }

            if (src < 1 || src > rows ||
                dst < 1 || dst > rows) {
                return std::unexpected(
                    std::format(
                        "line {}: node index out of range [1, {}]",
                        line_no, rows));
            }

            result.edges.push_back({
                static_cast<NodeId>(src - 1),
                static_cast<NodeId>(dst - 1)
            });
        }

        if (result.edges.size() != result.declared_edges) {
            return std::unexpected(
                std::format(
                    "header declares {} arcs but the file contains {}",
                    result.declared_edges,
                    result.edges.size()));
        }

        return result;
    }

    return std::unexpected(
        "invalid or missing header (expected: rows cols arcs)");
}

} // namespace pr