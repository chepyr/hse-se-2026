#include "shell/Utils.hpp"
#include <cctype>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace shell {

bool Utils::isInteractiveStdin() {
#if defined(_WIN32)
    return ::_isatty(_fileno(stdin)) != 0;
#else
    return ::isatty(STDIN_FILENO) != 0;
#endif
}

bool Utils::isValidEnvName(const std::string &name) {
    if (name.empty()) {
        return false;
    }
    auto is_alpha = [](unsigned char c) { return std::isalpha(c) != 0; };
    auto is_alnum = [](unsigned char c) { return std::isalnum(c) != 0; };

    unsigned char c0 = static_cast<unsigned char>(name[0]);
    if (!(is_alpha(c0) || c0 == '_')) {
        return false;
    }

    for (size_t i = 1; i < name.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(name[i]);
        if (!(is_alnum(c) || c == '_')) {
            return false;
        }
    }
    return true;
}

}  // namespace shell
