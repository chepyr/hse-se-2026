#include "shell/ShellApp.hpp"
#include <string>
#include "shell/Parse.hpp"
#include "shell/Utils.hpp"

namespace shell {

ShellApp::ShellApp(std::istream &in, std::ostream &out, std::ostream &err)
    : in_(in), out_(out), err_(err), env_(), executor_() {
}

void ShellApp::printPromptIfInteractive() const {
    if (Utils::isInteractiveStdin()) {
        out_ << "> " << std::flush;
    }
}

int ShellApp::run() {
    std::string line;
    while (true) {
        printPromptIfInteractive();

        if (!std::getline(in_, line)) {
            // EOF: exit gracefully with last exit code (common shell behavior
            // is 0/last)
            return last_exit_code_;
        }

        ParsedLine parsed = parseLine(line);
        IOStreams io{out_, err_};

        CommandResult res =
            executor_.execute(parsed, env_, io, last_exit_code_);
        last_exit_code_ = res.exit_code;

        if (res.request_exit) {
            return res.exit_code;
        }
    }
}

}  // namespace shell
