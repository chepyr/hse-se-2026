#include "shell/Executor.hpp"
#include <array>
#include <stdexcept>
#include "shell/Builtins.hpp"
#include "shell/ExternalRunner.hpp"

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#endif

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif
#include <iostream>

namespace shell {

#if !defined(_WIN32)
// POSIX implementation for pipeline execution
static CommandResult executePipelinePosix(
    const std::vector<CommandSpec> &pipeline,
    Environment &env,
    IOStreams io
) {
    const size_t cmd_count = pipeline.size();

    std::vector<std::array<int, 2>> pipes(cmd_count - 1);
    for (size_t i = 0; i < cmd_count - 1; ++i) {
        if (pipe(pipes[i].data()) < 0) {
            io.err << "pipe() failed: " << std::strerror(errno) << '\n';
            return {1, false};
        }
    }

    std::vector<pid_t> child_pids;

    for (size_t i = 0; i < cmd_count; ++i) {
        pid_t pid = fork();

        if (pid < 0) {
            io.err << "fork() failed: " << std::strerror(errno) << '\n';
            for (size_t j = 0; j < cmd_count - 1; ++j) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return {1, false};
        }

        if (pid == 0) {
            const auto &argv = pipeline[i].argv;
            if (argv[0].empty()) {
                const char *msg = "command not found\n";
                ::write(STDERR_FILENO, msg, std::strlen(msg));
                _exit(127);
            }

            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            if (i < cmd_count - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            for (size_t j = 0; j < cmd_count - 1; ++j) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            IOStreams child_io{std::cin, std::cout, std::cerr};

            CommandResult builtin_result =
                Builtins::runIfBuiltin(argv, child_io, 0);
            if (builtin_result.exit_code != -1) {
                _exit(builtin_result.exit_code);
            }

            ExternalRunner::execInChild(argv, env.snapshot());
            _exit(127);
        }

        child_pids.push_back(pid);
    }

    for (size_t i = 0; i < cmd_count - 1; ++i) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    int last_status = 0;
    for (pid_t child_pid : child_pids) {
        int status = 0;
        waitpid(child_pid, &status, 0);
        if (WIFEXITED(status)) {
            last_status = WEXITSTATUS(status);
        } else {
            last_status = 1;
        }
    }

    return {last_status, false};
}
#endif

CommandResult Executor::execute(
    const ParsedLine &parsed,
    Environment &env,
    IOStreams io,
    int last_exit_code
) const {
    if (!parsed.ok) {
        io.err << "parse error: " << parsed.error << '\n';
        return {2, false};
    }

    if (parsed.is_empty) {
        return {0, false};
    }

    if (parsed.is_assignment_only) {
        env.set(parsed.assign_name, parsed.assign_value);
        return {0, false};
    }

    try {
        if (parsed.pipeline.size() > 1) {
#if !defined(_WIN32)
            return executePipelinePosix(parsed.pipeline, env, io);
#else
            return executePipelineViaCmd(parsed.pipeline, env, io);
#endif
        }

        const auto &argv = parsed.pipeline[0].argv;

        if (argv[0].empty()) {
            io.err << "command not found\n";
            return {127, false};
        }

        CommandResult builtin_result =
            Builtins::runIfBuiltin(argv, io, last_exit_code);
        if (builtin_result.exit_code != -1) {
            return builtin_result;
        }

        return ExternalRunner::run(argv, env.snapshot(), io);
    } catch (const std::exception &ex) {
        io.err << "error: " << ex.what() << '\n';
        return {1, false};
    }
}

}  // namespace shell
