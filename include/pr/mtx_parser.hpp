// include/pr/mtx_parser.hpp
#pragma once

#include <pr/graph.hpp>  // NodeId, Edge, EdgeList

#include <expected>
#include <filesystem>
#include <string>

namespace pr {

/// Reads a Matrix Market coordinate file into an EdgeList.
///
/// Format: '%' comment lines, then "R C M", then M lines "i j" (1-based).
/// Node ids are converted to 0-based here, and only here.
///
/// Errors (returned as a message): file not readable, bad header, R != C,
/// malformed line, index outside [1, N], number of arcs != M.
/// Self-loops and duplicates are NOT removed here (that is Graph's job).
[[nodiscard]] std::expected<EdgeList, std::string> read_mtx(const std::filesystem::path& path);

}  // namespace pr
