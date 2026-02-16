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

    /**
     * @brief Executes external program in child process (does not return on
     * success). This is used for pipeline execution where we need exec
     * directly.
     * @param argv argv-style vector: argv[0] is program name/path.
     * @param env_snapshot Environment as a vector of "NAME=VALUE".
     */
    static void execInChild(
        const std::vector<std::string> &argv,
        const std::vector<std::string> &env_snapshot
    );

#if defined(_WIN32)
    CommandResult executePipelineViaCmd(
        const std::vector<CommandSpec> &commands,
        Environment &env,
        IOStreams io
    );
#endif
};

}  // namespace shell
