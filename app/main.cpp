// app/main.cpp
#include <pr/graph.hpp>
#include <pr/mtx_parser.hpp>
#include <pr/options.hpp>
#include <pr/pagerank.hpp>
#include <pr/report.hpp>

#include <iostream>

// Exit codes: 0 ok, 1 bad command line, 2 I/O error or malformed input.
int main(int argc, char* argv[]) {
    const auto opts = pr::parse_args(argc, argv);
    if (!opts) {
        std::cerr << "Error: " << opts.error() << '\n' << pr::usage(argv[0]);
        return 1;
    }
    if (opts->show_help) {
        std::cout << pr::usage(argv[0]);
        return 0;
    }

    const auto edges = pr::read_mtx(opts->input);
    if (!edges) {
        std::cerr << "Error: " << edges.error() << '\n';
        return 2;
    }

    const auto graph = pr::Graph::from_edges(*edges);
    const auto res   = pr::pagerank(graph, opts->damping, opts->eps, opts->max_iter);
    pr::print_report(std::cout, graph, res, opts->top);
    return 0;
}
