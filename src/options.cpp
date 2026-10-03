// src/options.cpp  (minimal version, POSIX getopt)
#include <pr/options.hpp>

#include <charconv>
#include <cmath>
#include <cstring>
#include <format>
#include <unistd.h>

namespace pr {
namespace {

// Whole string must be a valid, finite number ("5x", "", "nan", overflow -> false).
template <typename T>
bool parse(const char* s, T& out) {
    const char* end = s + std::strlen(s);
    const auto [ptr, ec] = std::from_chars(s, end, out);
    return ec == std::errc{} && ptr == end && std::isfinite(out);
}

}  // namespace

std::expected<Options, std::string> parse_args(int argc, char* argv[]) {
    Options o;
    opterr = 0;  // getopt must not print its own messages
    optind = 1;  // reset global state (needed when called more than once, e.g. tests)

    int c;
    while ((c = getopt(argc, argv, "d:e:m:k:t:h")) != -1) {
        bool ok = true;
        switch (c) {
        case 'd': ok = parse(optarg, o.damping) && o.damping > 0 && o.damping < 1; break;
        case 'e': ok = parse(optarg, o.eps) && o.eps > 0; break;
        case 'm': ok = parse(optarg, o.max_iter) && o.max_iter >= 1; break;
        case 'k': ok = parse(optarg, o.top) && o.top >= 1; break;
        case 't': ok = parse(optarg, o.threads) && o.threads >= 1; break;
        case 'h': o.show_help = true; return o;
        default:
            return std::unexpected(
                std::format("unknown option or missing value: -{}", static_cast<char>(optopt)));
        }
        if (!ok) {
            return std::unexpected(
                std::format("invalid value '{}' for -{}", optarg, static_cast<char>(c)));
        }
    }

    if (optind >= argc) {
        return std::unexpected("missing input file");
    }
    if (optind + 1 < argc) {
        return std::unexpected("too many arguments: expected a single input file");
    }
    o.input = argv[optind];
    return o;
}

std::string usage(std::string_view prog) {
    return std::format("Usage: {} [-d damping] [-e eps] [-m maxiter] [-k top] [-t threads] <graph.mtx>\n",
                       prog);
}

}  // namespace pr
