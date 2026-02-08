#pragma once
#include <string>
#include <vector>
#include "shell/CommandResult.hpp"
#include "shell/Executor.hpp"

namespace shell {

/**
 * @brief Runs external programs, capturing stdout/stderr and returning exit
 * codes.
 *
 * Platform-specific implementation is provided in src/ExternalRunner_posix.cpp
 * and src/ExternalRunner_win.cpp.
 */
class ExternalRunner final {
public:
    /**
     * @brief Runs an external program.
     * @param argv argv-style vector: argv[0] is program name/path.
     * @param env_snapshot Environment as a vector of "NAME=VALUE".
     * @param io Output/error streams.
     * @return CommandResult with the child exit code.
     */
    static CommandResult run(
        const std::vector<std::string> &argv,
        const std::vector<std::string> &env_snapshot,
        IOStreams io
    );
};

}  // namespace shell
