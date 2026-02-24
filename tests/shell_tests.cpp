#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "shell/Builtins.hpp"
#include "shell/Environment.hpp"
#include "shell/Executor.hpp"
#include "shell/ExternalRunner.hpp"
#include "shell/Parse.hpp"
#include "shell/ShellApp.hpp"
#include "shell/Tokenizer.hpp"

#ifndef FIXTURE_PATH
#error "FIXTURE_PATH is not defined"
#endif

// ---------------- minimal test framework ----------------
static int g_failed = 0;

#define EXPECT_TRUE(cond)                                         \
    do {                                                          \
        if (!(cond)) {                                            \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ \
                      << " EXPECT_TRUE(" #cond ")\n";             \
            ++g_failed;                                           \
        }                                                         \
    } while (0)

#define EXPECT_EQ(a, b)                                           \
    do {                                                          \
        auto _va = (a);                                           \
        auto _vb = (b);                                           \
        if (!(_va == _vb)) {                                      \
            std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ \
                      << " EXPECT_EQ(" #a "," #b ")\n"            \
                      << "  left:  " << _va << "\n"               \
                      << "  right: " << _vb << "\n";              \
            ++g_failed;                                           \
        }                                                         \
    } while (0)

static bool contains(const std::string &hay, const std::string &needle) {
    return hay.find(needle) != std::string::npos;
}

static std::string normalize_newlines(std::string s) {
    // Convert Windows CRLF to LF to make tests portable.
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\r') {
            continue;
        }
        out.push_back(s[i]);
    }
    return out;
}

// ---------------- tests ----------------
static void test_tokenizer_basic() {
    auto r = shell::Tokenizer::tokenize("echo hello   world");
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(r.tokens.size(), (size_t)3);
    EXPECT_EQ(r.tokens[0], std::string("echo"));
    EXPECT_EQ(r.tokens[1], std::string("hello"));
    EXPECT_EQ(r.tokens[2], std::string("world"));
}

static void test_tokenizer_quotes() {
    {
        auto r = shell::Tokenizer::tokenize("echo 'a b' \"c d\"");
        EXPECT_TRUE(r.ok);
        EXPECT_EQ(r.tokens.size(), (size_t)3);
        EXPECT_EQ(r.tokens[1], std::string("a b"));
        EXPECT_EQ(r.tokens[2], std::string("c d"));
    }
    {
        auto r = shell::Tokenizer::tokenize("echo \"\"");
        EXPECT_TRUE(r.ok);
        EXPECT_EQ(r.tokens.size(), (size_t)2);
        EXPECT_EQ(r.tokens[1], std::string(""));
    }
    {
        auto r = shell::Tokenizer::tokenize("echo 'unterminated");
        EXPECT_TRUE(!r.ok);
        EXPECT_TRUE(contains(r.error, "Unterminated"));
    }
}

static void test_parse_assignment() {
    {
        shell::ParsedLine p = shell::parseLine("FOO=bar");
        EXPECT_TRUE(p.ok);
        EXPECT_TRUE(p.is_assignment_only);
        EXPECT_EQ(p.assign_name, std::string("FOO"));
        EXPECT_EQ(p.assign_value, std::string("bar"));
    }
    {
        // invalid name -> should be treated as a command, not assignment-only
        shell::ParsedLine p = shell::parseLine("1BAD=xx");
        EXPECT_TRUE(p.ok);
        EXPECT_TRUE(!p.is_assignment_only);
        EXPECT_EQ(p.argv.size(), (size_t)1);
        EXPECT_EQ(p.argv[0], std::string("1BAD=xx"));
    }
}

static void test_builtins_echo_pwd_cat_wc() {
    // echo
    {
        std::ostringstream out, err;
        shell::IOStreams io{out, err};
        auto r =
            shell::Builtins::runIfBuiltin({"echo", "hello", "world"}, io, 0);
        EXPECT_EQ(r.exit_code, 0);
        EXPECT_EQ(out.str(), std::string("hello world\n"));
        EXPECT_EQ(err.str(), std::string(""));
    }

    // pwd: should print current_path (with trailing newline)
    {
        std::ostringstream out, err;
        shell::IOStreams io{out, err};
        auto r = shell::Builtins::runIfBuiltin({"pwd"}, io, 0);
        EXPECT_EQ(r.exit_code, 0);
        std::string got = out.str();
        EXPECT_TRUE(!got.empty());
        // weak check: output contains current_path string
        std::string cwd = std::filesystem::current_path().string();
        EXPECT_TRUE(contains(got, cwd));
    }

    // cat + wc: create a temp file
    std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "cli_shell_test_tmp.txt";
    {
        std::ofstream f(tmp);
        f << "one two\nthree\n";
    }

    // cat
    {
        std::ostringstream out, err;
        shell::IOStreams io{out, err};
        auto r = shell::Builtins::runIfBuiltin({"cat", tmp.string()}, io, 0);
        EXPECT_EQ(r.exit_code, 0);
        EXPECT_EQ(
            normalize_newlines(out.str()), std::string("one two\nthree\n")
        );
    }

    // wc: lines=2, words=3, bytes=14 (ASCII) -> "one"(3)+"
    // "(1)+"two"(3)+"\n"(1)+"three"(5)+"\n"(1) = 14
    {
        std::ostringstream out, err;
        shell::IOStreams io{out, err};
        auto r = shell::Builtins::runIfBuiltin({"wc", tmp.string()}, io, 0);
        EXPECT_EQ(r.exit_code, 0);
        EXPECT_TRUE(contains(normalize_newlines(out.str()), "2 3"));
    }

    std::error_code ec;
    std::filesystem::remove(tmp, ec);
}

static void test_external_runner_env_and_streams() {
    shell::Environment env;
    env.set("CLI_TEST_VAR", "hello");

    std::ostringstream out, err;
    shell::IOStreams io{out, err};

    const std::string fixture = std::string(FIXTURE_PATH);

    // Run helper, ask it to return 7
    shell::CommandResult r = shell::ExternalRunner::run(
        {fixture, "--code", "7", "arg1"}, env.snapshot(), io
    );

    EXPECT_EQ(r.exit_code, 7);

    std::string so = out.str();
    std::string se = err.str();

    EXPECT_TRUE(contains(so, "CLI_TEST_VAR=hello"));
    EXPECT_TRUE(contains(so, "ARGV:"));
    EXPECT_TRUE(contains(se, "STDERR_MARKER"));
}

static void test_executor_assignment_and_external() {
    shell::Environment env;
    shell::Executor ex;

    // assignment-only modifies env
    {
        shell::ParsedLine p;
        p.ok = true;
        p.is_assignment_only = true;
        p.assign_name = "CLI_TEST_VAR";
        p.assign_value = "world";

        std::ostringstream out, err;
        shell::IOStreams io{out, err};
        auto r = ex.execute(p, env, io, 0);
        EXPECT_EQ(r.exit_code, 0);
    }

    // external uses env snapshot
    {
        shell::ParsedLine p;
        p.ok = true;
        p.argv = {std::string(FIXTURE_PATH), "x"};

        std::ostringstream out, err;
        shell::IOStreams io{out, err};
        auto r = ex.execute(p, env, io, 0);

        EXPECT_EQ(r.exit_code, 0);
        EXPECT_TRUE(contains(out.str(), "CLI_TEST_VAR=world"));
    }
}

static void test_shellapp_repl_exit_code() {
    std::istringstream in("echo hi\nexit 5\n");
    std::ostringstream out, err;

    shell::ShellApp app(in, out, err);
    int code = app.run();

    EXPECT_EQ(code, 5);
    EXPECT_TRUE(contains(out.str(), "hi\n"));
}

static void test_parse_assignment_empty_value() {
    shell::ParsedLine p = shell::parseLine("FOO=");
    EXPECT_TRUE(p.ok);
    EXPECT_TRUE(p.is_assignment_only);
    EXPECT_EQ(p.assign_name, std::string("FOO"));
    EXPECT_EQ(p.assign_value, std::string(""));
}

static void test_builtins_errors() {
    // cat without file
    {
        std::ostringstream out, err;
        shell::IOStreams io{out, err};
        auto r = shell::Builtins::runIfBuiltin({"cat"}, io, 0);
        EXPECT_TRUE(r.exit_code != 0);
        EXPECT_TRUE(!err.str().empty());
    }

    // wc on missing file
    {
        std::ostringstream out, err;
        shell::IOStreams io{out, err};
        auto r = shell::Builtins::runIfBuiltin(
            {"wc", "___definitely_missing_file___"}, io, 0
        );
        EXPECT_TRUE(r.exit_code != 0);
        EXPECT_TRUE(!err.str().empty());
    }
}

static void test_external_runner_unknown_command() {
    shell::Environment env;
    std::ostringstream out, err;
    shell::IOStreams io{out, err};

    auto r = shell::ExternalRunner::run(
        {"___definitely_missing_executable___"}, env.snapshot(), io
    );

    EXPECT_TRUE(r.exit_code != 0);
    EXPECT_TRUE(!err.str().empty());
}

// ---------------- main ----------------
int main() {
    test_tokenizer_basic();
    test_tokenizer_quotes();
    test_parse_assignment();
    test_builtins_echo_pwd_cat_wc();
    test_external_runner_env_and_streams();
    test_executor_assignment_and_external();
    test_shellapp_repl_exit_code();
    test_parse_assignment_empty_value();
    test_builtins_errors();
    test_external_runner_unknown_command();

    if (g_failed == 0) {
        std::cerr << "[OK] all tests passed\n";
        return 0;
    }
    std::cerr << "[FAIL] failed: " << g_failed << "\n";
    return 1;
}
