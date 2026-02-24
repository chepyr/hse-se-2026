#include "shell/Builtins.hpp"
#include <cctype>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

namespace shell {

// Parses a base-10 integer into `out`. Returns false on invalid input or
// overflow.
static bool parseInt(const std::string &s, int &out) {
    if (s.empty()) {
        return false;
    }
    std::size_t idx = 0;
    try {
        long v = std::stol(s, &idx, 10);
        if (idx != s.size()) {
            return false;
        }
        if (v < std::numeric_limits<int>::min() ||
            v > std::numeric_limits<int>::max()) {
            return false;
        }
        out = static_cast<int>(v);
        return true;
    } catch (...) {
        return false;
    }
}

CommandResult Builtins::runIfBuiltin(
    const std::vector<std::string> &argv,
    IOStreams io,
    int last_exit_code
) {
    CommandResult unknown;
    unknown.exit_code = -1;

    if (argv.empty()) {
        return unknown;
    }

    const std::string &name = argv[0];
    if (name == "echo") {
        return cmdEcho(argv, io);
    }
    if (name == "pwd") {
        return cmdPwd(io);
    }
    if (name == "cat") {
        return cmdCat(argv, io);
    }
    if (name == "wc") {
        return cmdWc(argv, io);
    }
    if (name == "exit") {
        return cmdExit(argv, io, last_exit_code);
    }

    return unknown;
}

CommandResult
Builtins::cmdEcho(const std::vector<std::string> &argv, IOStreams io) {
    // echo prints all arguments after the command name separated by spaces.
    for (size_t i = 1; i < argv.size(); ++i) {
        if (i > 1) {
            io.out << ' ';
        }
        io.out << argv[i];
    }
    io.out << '\n';
    return {0, false};
}

CommandResult Builtins::cmdPwd(IOStreams io) {
    try {
        io.out << std::filesystem::current_path().string() << '\n';
        return {0, false};
    } catch (const std::exception &e) {
        io.err << "pwd: " << e.what() << '\n';
        return {1, false};
    }
}

CommandResult
Builtins::cmdCat(const std::vector<std::string> &argv, IOStreams io) {
    if (argv.size() != 2) {
        io.err << "cat: expected exactly one file argument\n";
        return {2, false};
    }

    const std::string &path = argv[1];
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        io.err << "cat: " << path << ": cannot open file\n";
        return {1, false};
    }

    io.out << in.rdbuf();
    return {0, false};
}

CommandResult
Builtins::cmdWc(const std::vector<std::string> &argv, IOStreams io) {
    if (argv.size() != 2) {
        io.err << "wc: expected exactly one file argument\n";
        return {2, false};
    }

    const std::string &path = argv[1];
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        io.err << "wc: " << path << ": cannot open file\n";
        return {1, false};
    }

    std::uint64_t lines = 0;
    std::uint64_t words = 0;
    std::uint64_t bytes = 0;

    bool in_word = false;
    char ch;
    while (in.get(ch)) {
        ++bytes;
        if (ch == '\n') {
            ++lines;
        }

        unsigned char uc = static_cast<unsigned char>(ch);
        bool is_space = (std::isspace(uc) != 0);
        if (is_space) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            ++words;
        }
    }

    io.out << lines << ' ' << words << ' ' << bytes << '\n';
    return {0, false};
}

CommandResult Builtins::cmdExit(
    const std::vector<std::string> &argv,
    IOStreams io,
    int last_exit_code
) {
    if (argv.size() == 1) {
        return {last_exit_code, true};
    }
    if (argv.size() == 2) {
        int code = 0;
        if (!parseInt(argv[1], code)) {
            io.err << "exit: numeric argument required\n";
            return {2, true};
        }
        return {code, true};
    }
    io.err << "exit: too many arguments\n";
    return {2, false};
}

}  // namespace shell
