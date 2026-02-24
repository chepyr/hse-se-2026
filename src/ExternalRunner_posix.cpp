#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>
#include "shell/ExternalRunner.hpp"

namespace shell {

static void pumpFdToStream(
    int fd,
    std::ostream &stream,
    std::mutex &stream_mutex
) {
    constexpr size_t kBufSize = 4096;
    char buf[kBufSize];

    while (true) {
        ssize_t bytes_read = ::read(fd, buf, kBufSize);
        if (bytes_read == 0) {
            break;
        }
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        std::lock_guard<std::mutex> lock(stream_mutex);
        stream.write(buf, bytes_read);
        stream.flush();
    }
}

static void applyEnvironment(const std::vector<std::string> &env_snapshot) {
    for (const std::string &entry : env_snapshot) {
        auto eq_pos = entry.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        std::string key = entry.substr(0, eq_pos);
        std::string value = entry.substr(eq_pos + 1);
        ::setenv(key.c_str(), value.c_str(), 1);
    }
}

CommandResult ExternalRunner::run(
    const std::vector<std::string> &argv,
    const std::vector<std::string> &env_snapshot,
    IOStreams io
) {
    if (argv.empty() || argv[0].empty()) {
        io.err << "command not found\n";
        return {127, false};
    }

    int out_pipe[2]{-1, -1};
    int err_pipe[2]{-1, -1};

    if (::pipe(out_pipe) != 0) {
        io.err << "failed to create stdout pipe\n";
        return {127, false};
    }
    if (::pipe(err_pipe) != 0) {
        ::close(out_pipe[0]);
        ::close(out_pipe[1]);
        io.err << "failed to create stderr pipe\n";
        return {127, false};
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(out_pipe[0]);
        ::close(out_pipe[1]);
        ::close(err_pipe[0]);
        ::close(err_pipe[1]);
        io.err << "failed to fork\n";
        return {127, false};
    }

    if (pid == 0) {
        // Child
        ::dup2(out_pipe[1], STDOUT_FILENO);
        ::dup2(err_pipe[1], STDERR_FILENO);

        ::close(out_pipe[0]);
        ::close(out_pipe[1]);
        ::close(err_pipe[0]);
        ::close(err_pipe[1]);

        applyEnvironment(env_snapshot);

        std::vector<char *> exec_argv;
        exec_argv.reserve(argv.size() + 1);
        for (const auto &arg : argv) {
            exec_argv.push_back(const_cast<char *>(arg.c_str()));
        }
        exec_argv.push_back(nullptr);

        ::execvp(exec_argv[0], exec_argv.data());

        const char *err_msg = std::strerror(errno);
        ::write(STDERR_FILENO, err_msg, std::strlen(err_msg));
        ::write(STDERR_FILENO, "\n", 1);
        _exit(127);
    }

    ::close(out_pipe[1]);
    ::close(err_pipe[1]);

    std::mutex stdout_mutex;
    std::mutex stderr_mutex;
    std::thread stdout_thread([&]() {
        pumpFdToStream(out_pipe[0], io.out, stdout_mutex);
    });
    std::thread stderr_thread([&]() {
        pumpFdToStream(err_pipe[0], io.err, stderr_mutex);
    });

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        break;
    }

    ::close(out_pipe[0]);
    ::close(err_pipe[0]);

    stdout_thread.join();
    stderr_thread.join();

    int exit_code = 0;
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        exit_code = 128 + WTERMSIG(status);
    } else {
        exit_code = 127;
    }

    return {exit_code, false};
}

void ExternalRunner::execInChild(
    const std::vector<std::string> &argv,
    const std::vector<std::string> &env_snapshot
) {
    if (argv.empty() || argv[0].empty()) {
        const char *msg = "command not found\n";
        ::write(STDERR_FILENO, msg, std::strlen(msg));
        _exit(127);
    }

    applyEnvironment(env_snapshot);

    std::vector<char *> exec_argv;
    exec_argv.reserve(argv.size() + 1);
    for (const auto &arg : argv) {
        exec_argv.push_back(const_cast<char *>(arg.c_str()));
    }
    exec_argv.push_back(nullptr);

    ::execvp(exec_argv[0], exec_argv.data());

    const char *err_msg = std::strerror(errno);
    ::write(STDERR_FILENO, err_msg, std::strlen(err_msg));
    ::write(STDERR_FILENO, "\n", 1);
    _exit(127);
}

}  // namespace shell
