// src/options.cpp

#include <pr/options.hpp>

#include <charconv>
#include <cmath>
#include <format>
#include <unistd.h>

namespace pr {

namespace {

bool parse_double(const char* s, double& value) {
    const char* end = s + std::char_traits<char>::length(s);
    const auto [ptr, ec] = std::from_chars(s, end, value);

    return ec == std::errc{} && ptr == end && std::isfinite(value);
}

bool parse_int(const char* s, std::int32_t& value) {
    const char* end = s + std::char_traits<char>::length(s);
    const auto [ptr, ec] = std::from_chars(s, end, value);

    return ec == std::errc{} && ptr == end;
}

} // namespace

std::expected<Options, std::string> parse_args(int argc, char* argv[]) {
    Options options;

    opterr = 0;
    optind = 1;

    int c;
    while ((c = getopt(argc, argv, "d:e:m:k:t:h")) != -1) {

        switch (c) {
        case 'd':
            if (!parse_double(optarg, options.damping) ||
                options.damping <= 0 || options.damping >= 1) {
                return std::unexpected("invalid value for -d");
            }
            break;

        case 'e':
            if (!parse_double(optarg, options.eps) ||
                options.eps <= 0) {
                return std::unexpected("invalid value for -e");
            }
            break;

        case 'm':
            if (!parse_int(optarg, options.max_iter) ||
                options.max_iter < 1) {
                return std::unexpected("invalid value for -m");
            }
            break;

        case 'k':
            if (!parse_int(optarg, options.top) ||
                options.top < 1) {
                return std::unexpected("invalid value for -k");
            }
            break;

        case 't':
            if (!parse_int(optarg, options.threads) ||
                options.threads < 1) {
                return std::unexpected("invalid value for -t");
            }
            break;

        case 'h':
            options.show_help = true;
            return options;

        default:
            return std::unexpected("unknown option or missing value");
        }
    }

    if (optind >= argc) {
        return std::unexpected("missing input file");
    }

    if (optind + 1 < argc) {
        return std::unexpected("too many arguments");
    }

    options.input = argv[optind];
    return options;
}

std::string usage(std::string_view program_name) {
    return std::format(
        "Usage: {} [-d damping] [-e eps] [-m maxiter] "
        "[-k top] [-t threads] <graph.mtx>\n",
        program_name);
}

} // namespace pr