#include "shell/Executor.hpp"
#include <sstream>
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
    const size_t n = pipeline.size();

    // Create pipes
    std::vector<std::array<int, 2>> pipes(n - 1);
    for (size_t i = 0; i < n - 1; ++i) {
        if (pipe(pipes[i].data()) < 0) {
            io.err << "pipe() failed: " << std::strerror(errno) << '\n';
            return {1, false};
        }
    }

    // Fork and execute each command
    std::vector<pid_t> pids;

    for (size_t i = 0; i < n; ++i) {
        pid_t pid = fork();

        if (pid < 0) {
            io.err << "fork() failed: " << std::strerror(errno) << '\n';
            // Close all pipes
            for (size_t j = 0; j < n - 1; ++j) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return {1, false};
        }

        if (pid == 0) {
            // Child process

            // Setup stdin
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            // Setup stdout
            if (i < n - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Close all pipe file descriptors in child
            for (size_t j = 0; j < n - 1; ++j) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Execute command
            const auto &argv = pipeline[i].argv;

            // In child, stdin/stdout are already redirected via dup2
            // Use std::cin/cout/cerr which will use the redirected file
            // descriptors
            IOStreams child_io{std::cin, std::cout, std::cerr};

            // Check for built-in (note: most builtins don't make sense in
            // pipeline) But we'll handle them anyway for completeness
            CommandResult br = Builtins::runIfBuiltin(argv, child_io, 0);
            if (br.exit_code != -1) {
                std::exit(br.exit_code);
            }

            // Execute external command
            ExternalRunner::execInChild(argv, env.snapshot());
            std::exit(127);  // Should not reach here
        }

        pids.push_back(pid);
    }

    // Parent: close all pipes
    for (size_t i = 0; i < n - 1; ++i) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all children
    int last_status = 0;
    for (pid_t pid : pids) {
        int status;
        waitpid(pid, &status, 0);
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

    // Check if this is a pipeline
    if (parsed.pipeline.size() > 1) {
#if !defined(_WIN32)
        return executePipelinePosix(parsed.pipeline, env, io);
#else
        io.err << "Pipeline execution not yet implemented on Windows\n";
        return {1, false};
#endif
    }

    // Single command (no pipeline)
    const auto &argv = parsed.pipeline[0].argv;

    // Built-ins
    CommandResult br = Builtins::runIfBuiltin(argv, io, last_exit_code);
    if (br.exit_code != -1) {
        return br;
    }

    // External command
    return ExternalRunner::run(argv, env.snapshot(), io);
}

}  // namespace shell
