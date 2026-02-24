#include <cstdlib>
#include <iostream>
#include <string>

/**
 * @brief Helper program for unit tests.
 *
 * Prints argv and one env var to stdout, prints a marker to stderr,
 * and can return a non-zero exit code when requested.
 *
 * Usage:
 *   fixture_env [--code N] [args...]
 */
int main(int argc, char **argv) {
    int code = 0;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--code") {
            code = std::atoi(argv[i + 1]);
        }
    }

    const char *v = std::getenv("CLI_TEST_VAR");
    std::cout << "CLI_TEST_VAR=" << (v ? v : "") << "\n";

    std::cout << "ARGV:";
    for (int i = 0; i < argc; ++i) {
        std::cout << " [" << argv[i] << "]";
    }
    std::cout << "\n";

    std::cerr << "STDERR_MARKER\n";
    return code;
}
