#pragma once
#include <string>

namespace shell {

/**
 * @brief Miscellaneous small utilities.
 */
class Utils final {
public:
    /**
     * @brief Returns true if stdin is a terminal (interactive session).
     */
    static bool isInteractiveStdin();

    /**
     * @brief Checks if a string is a valid environment variable name:
     * [A-Za-z_][A-Za-z0-9_]*
     */
    static bool isValidEnvName(const std::string &name);
};

}  // namespace shell
