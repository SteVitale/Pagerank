// include/pr/report.hpp
#pragma once

#include <pr/graph.hpp>
#include <pr/pagerank.hpp>

#include <cstdint>
#include <ostream>

namespace pr {

/// Prints the summary and the top-k nodes (ids are 0-based).
/// `top` is clamped to the number of nodes. Ties are broken by lower id.
void print_report(std::ostream& out, const Graph& g, const PageRankResult& res,
                  std::int32_t top);

}  // namespace pr
