#pragma once
#include <istream>
#include <ostream>
#include <string>
#include "shell/CommandResult.hpp"
#include "shell/Environment.hpp"
#include "shell/Parse.hpp"

namespace shell {

/**
 * @brief I/O streams used by commands.
 */
struct IOStreams final {
    /** @brief Command standard output stream. */
    std::ostream &out;

    /** @brief Command standard error stream. */
    std::ostream &err;
};

/**
 * @brief Executes parsed lines: assignment, built-in or external.
 */
class Executor final {
public:
    /**
     * @brief Executes a parsed line.
     * @param parsed ParsedLine.
     * @param env Shell environment (may be modified by assignments).
     * @param io Output/error streams.
     * @param last_exit_code Exit code of previous command (used by `exit`
     * without args, optional policy).
     * @return CommandResult.
     */
    CommandResult execute(
        const ParsedLine &parsed,
        Environment &env,
        IOStreams io,
        int last_exit_code
    ) const;
};

}  // namespace shell
