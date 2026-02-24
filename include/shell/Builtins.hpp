#pragma once
#include <string>
#include <vector>
#include "shell/CommandResult.hpp"
#include "shell/Executor.hpp"

namespace shell {

/**
 * @brief Built-in command implementations.
 */
class Builtins final {
public:
    /**
     * @brief Executes a built-in command if the name matches.
     * @param argv Command argv (argv[0] is the command name).
     * @param io Output/error streams.
     * @param last_exit_code Exit code of previous command.
     * @return CommandResult. If command is unknown, exit_code is set to -1.
     */
    static CommandResult runIfBuiltin(
        const std::vector<std::string> &argv,
        IOStreams io,
        int last_exit_code
    );

private:
    static CommandResult
    cmdEcho(const std::vector<std::string> &argv, IOStreams io);
    static CommandResult cmdPwd(IOStreams io);
    static CommandResult
    cmdCat(const std::vector<std::string> &argv, IOStreams io);
    static CommandResult
    cmdWc(const std::vector<std::string> &argv, IOStreams io);
    static CommandResult cmdExit(
        const std::vector<std::string> &argv,
        IOStreams io,
        int last_exit_code
    );
};

}  // namespace shell
