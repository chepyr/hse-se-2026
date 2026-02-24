#pragma once
#include <string>

namespace shell {

/**
 * @brief Result of executing a command.
 */
struct CommandResult final {
    /** @brief Process exit code (0 usually means success). */
    int exit_code = 0;

    /** @brief If true, the REPL should stop after this command. */
    bool request_exit = false;
};

}  // namespace shell
