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

static void writeAllFromFdToStream(int fd, std::ostream &os, std::mutex &mtx) {
    constexpr size_t kBufSize = 4096;
    char buf[kBufSize];

    while (true) {
        ssize_t n = ::read(fd, buf, kBufSize);
        if (n == 0) {
            break;  // EOF
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        std::lock_guard<std::mutex> lk(mtx);
        os.write(buf, n);
        os.flush();
    }
}

static void setChildEnvironment(const std::vector<std::string>& env_snapshot) {
    for (const std::string& kv : env_snapshot) {
        auto pos = kv.find('=');
        if (pos == std::string::npos) continue;

        std::string key = kv.substr(0, pos);
        std::string val = kv.substr(pos + 1);
        ::setenv(key.c_str(), val.c_str(), 1);  // overwrite = 1
    }
}

CommandResult ExternalRunner::run(
    const std::vector<std::string> &argv,
    const std::vector<std::string> &env_snapshot,
    IOStreams io
) {
    if (argv.empty()) {
        return {2, false};
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

        setChildEnvironment(env_snapshot);

        std::vector<char *> cargv;
        cargv.reserve(argv.size() + 1);
        for (const auto &s : argv) {
            cargv.push_back(const_cast<char *>(s.c_str()));
        }
        cargv.push_back(nullptr);

        ::execvp(cargv[0], cargv.data());

        // exec failed
        const char *msg = std::strerror(errno);
        ::write(STDERR_FILENO, msg, std::strlen(msg));
        ::write(STDERR_FILENO, "\n", 1);
        _exit(127);
    }

    // Parent
    ::close(out_pipe[1]);
    ::close(err_pipe[1]);

    std::mutex out_mtx, err_mtx;
    std::thread t_out([&]() {
        writeAllFromFdToStream(out_pipe[0], io.out, out_mtx);
    });
    std::thread t_err([&]() {
        writeAllFromFdToStream(err_pipe[0], io.err, err_mtx);
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

    t_out.join();
    t_err.join();

    int code = 0;
    if (WIFEXITED(status)) {
        code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        code = 128 + WTERMSIG(status);
    } else {
        code = 127;
    }

    return {code, false};
}

}  // namespace shell
