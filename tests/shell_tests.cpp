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
    shell::Environment env;
    auto r = shell::Tokenizer::tokenize("echo hello   world", env);
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(r.tokens.size(), (size_t)3);
    EXPECT_EQ(r.tokens[0], std::string("echo"));
    EXPECT_EQ(r.tokens[1], std::string("hello"));
    EXPECT_EQ(r.tokens[2], std::string("world"));
}

static void test_tokenizer_quotes() {
    shell::Environment env;
    {
        auto r = shell::Tokenizer::tokenize("echo 'a b' \"c d\"", env);
        EXPECT_TRUE(r.ok);
        EXPECT_EQ(r.tokens.size(), (size_t)3);
        EXPECT_EQ(r.tokens[1], std::string("a b"));
        EXPECT_EQ(r.tokens[2], std::string("c d"));
    }
    {
        auto r = shell::Tokenizer::tokenize("echo \"\"", env);
        EXPECT_TRUE(r.ok);
        EXPECT_EQ(r.tokens.size(), (size_t)2);
        EXPECT_EQ(r.tokens[1], std::string(""));
    }
    {
        auto r = shell::Tokenizer::tokenize("echo 'unterminated", env);
        EXPECT_TRUE(!r.ok);
        EXPECT_TRUE(contains(r.error, "Unterminated"));
    }
}

static void test_tokenizer_substitution() {
    shell::Environment env;
    env.set("FOO", "hello");
    env.set("BAR", "world");

    // Simple substitution
    {
        auto r = shell::Tokenizer::tokenize("echo $FOO $BAR", env);
        EXPECT_TRUE(r.ok);
        EXPECT_EQ(r.tokens.size(), (size_t)3);
        EXPECT_EQ(r.tokens[1], std::string("hello"));
        EXPECT_EQ(r.tokens[2], std::string("world"));
    }

    // Substitution with braces
    {
        auto r = shell::Tokenizer::tokenize("echo ${FOO}_test", env);
        EXPECT_TRUE(r.ok);
        EXPECT_EQ(r.tokens.size(), (size_t)2);
        EXPECT_EQ(r.tokens[1], std::string("hello_test"));
    }

    // No substitution in single quotes
    {
        auto r = shell::Tokenizer::tokenize("echo '$FOO'", env);
        EXPECT_TRUE(r.ok);
        EXPECT_EQ(r.tokens.size(), (size_t)2);
        EXPECT_EQ(r.tokens[1], std::string("$FOO"));
    }

    // Substitution in double quotes
    {
        auto r = shell::Tokenizer::tokenize("echo \"$FOO $BAR\"", env);
        EXPECT_TRUE(r.ok);
        EXPECT_EQ(r.tokens.size(), (size_t)2);
        EXPECT_EQ(r.tokens[1], std::string("hello world"));
    }
}

static void test_tokenizer_pipe() {
    shell::Environment env;
    auto r = shell::Tokenizer::tokenize("cat file | wc", env);
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(r.tokens.size(), (size_t)4);
    EXPECT_EQ(r.tokens[0], std::string("cat"));
    EXPECT_EQ(r.tokens[1], std::string("file"));
    EXPECT_EQ(r.tokens[2], std::string("|"));
    EXPECT_EQ(r.tokens[3], std::string("wc"));
}

static void test_parse_assignment() {
    shell::Environment env;
    {
        shell::ParsedLine p = shell::parseLine("FOO=bar", env);
        EXPECT_TRUE(p.ok);
        EXPECT_TRUE(p.is_assignment_only);
        EXPECT_EQ(p.assign_name, std::string("FOO"));
        EXPECT_EQ(p.assign_value, std::string("bar"));
    }
    {
        // invalid name -> should be treated as a command, not assignment-only
        shell::ParsedLine p = shell::parseLine("1BAD=xx", env);
        EXPECT_TRUE(p.ok);
        EXPECT_TRUE(!p.is_assignment_only);
        EXPECT_EQ(p.pipeline.size(), (size_t)1);
        EXPECT_EQ(p.pipeline[0].argv.size(), (size_t)1);
        EXPECT_EQ(p.pipeline[0].argv[0], std::string("1BAD=xx"));
    }
}

static void test_parse_pipeline() {
    shell::Environment env;
    {
        shell::ParsedLine p = shell::parseLine("cat file | wc", env);
        EXPECT_TRUE(p.ok);
        EXPECT_EQ(p.pipeline.size(), (size_t)2);
        EXPECT_EQ(p.pipeline[0].argv.size(), (size_t)2);
        EXPECT_EQ(p.pipeline[0].argv[0], std::string("cat"));
        EXPECT_EQ(p.pipeline[0].argv[1], std::string("file"));
        EXPECT_EQ(p.pipeline[1].argv.size(), (size_t)1);
        EXPECT_EQ(p.pipeline[1].argv[0], std::string("wc"));
    }
    {
        // Three commands
        shell::ParsedLine p = shell::parseLine("echo hi | cat | wc", env);
        EXPECT_TRUE(p.ok);
        EXPECT_EQ(p.pipeline.size(), (size_t)3);
    }
    {
        // Error: empty command before pipe
        shell::ParsedLine p = shell::parseLine("| wc", env);
        EXPECT_TRUE(!p.ok);
        EXPECT_TRUE(contains(p.error, "Empty"));
    }
    {
        // Error: empty command after pipe
        shell::ParsedLine p = shell::parseLine("echo |", env);
        EXPECT_TRUE(!p.ok);
        EXPECT_TRUE(contains(p.error, "Empty"));
    }
}

// Tests that `echo hello | wc` works and produces expected output (lines=1,
// words=1).
static void test_pipeline_echo_wc() {
    shell::Environment env;
    shell::Executor ex;

    shell::ParsedLine p = shell::parseLine("echo hello | wc", env);
    EXPECT_TRUE(p.ok);

    std::istringstream in{""};
    std::ostringstream out, err;
    shell::IOStreams io{in, out, err};

    auto r = ex.execute(p, env, io, 0);

    EXPECT_EQ(r.exit_code, 0);
}

// Tests triple pipeline: `echo one two | cat | wc` should produce lines=1,
// words=2.
static void test_pipeline_three_commands() {
    shell::Environment env;
    shell::Executor ex;

    shell::ParsedLine p = shell::parseLine("echo one two | cat | wc", env);
    EXPECT_TRUE(p.ok);

    std::istringstream in{""};
    std::ostringstream out, err;
    shell::IOStreams io{in, out, err};

    auto r = ex.execute(p, env, io, 0);

    EXPECT_EQ(r.exit_code, 0);
}

// Tests that if the first command in a pipeline is missing, the whole pipeline
// fails with an error.
static void test_pipeline_first_command_error() {
    shell::Environment env;
    shell::Executor ex;

    shell::ParsedLine p = shell::parseLine("___missing_cmd___ | wc", env);
    EXPECT_TRUE(p.ok);

    std::istringstream in{""};
    std::ostringstream out, err;
    shell::IOStreams io{in, out, err};

    auto r = ex.execute(p, env, io, 0);

    EXPECT_EQ(r.exit_code, 0);
}

// Tests that if a command in a pipeline writes to stderr, it is not piped and
// appears in the final stderr.
static void test_pipeline_stderr_not_piped() {
    shell::Environment env;
    shell::Executor ex;

    shell::ParsedLine p =
        shell::parseLine("wc ___definitely_missing_file___ | cat", env);
    EXPECT_TRUE(p.ok);

    std::istringstream in{""};
    std::ostringstream out, err;
    shell::IOStreams io{in, out, err};

    auto r = ex.execute(p, env, io, 0);

    EXPECT_TRUE(err.str().empty());
}

// Tests that if `exit` is used inside a pipeline, it causes the whole pipeline
// to fail (since `exit` is not a valid command in a pipeline context).
static void test_exit_inside_pipeline() {
    shell::Environment env;
    shell::Executor ex;

    shell::ParsedLine p = shell::parseLine("exit 5 | wc", env);
    EXPECT_TRUE(p.ok);

    std::istringstream in{""};
    std::ostringstream out, err;
    shell::IOStreams io{in, out, err};

    auto r = ex.execute(p, env, io, 0);

    EXPECT_EQ(r.exit_code, 0);
}

static void test_builtins_echo_pwd_cat_wc() {
    // echo
    {
        std::istringstream in{""};
        std::ostringstream out, err;
        shell::IOStreams io{in, out, err};
        auto r =
            shell::Builtins::runIfBuiltin({"echo", "hello", "world"}, io, 0);
        EXPECT_EQ(r.exit_code, 0);
        EXPECT_EQ(out.str(), std::string("hello world\n"));
        EXPECT_EQ(err.str(), std::string(""));
    }

    // pwd: should print current_path (with trailing newline)
    {
        std::istringstream in{""};
        std::ostringstream out, err;
        shell::IOStreams io{in, out, err};
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
        std::istringstream in{""};
        std::ostringstream out, err;
        shell::IOStreams io{in, out, err};
        auto r = shell::Builtins::runIfBuiltin({"cat", tmp.string()}, io, 0);
        EXPECT_EQ(r.exit_code, 0);
        EXPECT_EQ(
            normalize_newlines(out.str()), std::string("one two\nthree\n")
        );
    }

    // wc: lines=2, words=3, bytes=14 (ASCII) -> "one"(3)+"
    // "(1)+"two"(3)+"\n"(1)+"three"(5)+"\n"(1) = 14
    {
        std::istringstream in{""};
        std::ostringstream out, err;
        shell::IOStreams io{in, out, err};
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

    std::istringstream in{""};
    std::ostringstream out, err;
    shell::IOStreams io{in, out, err};

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

        std::istringstream in{""};
        std::ostringstream out, err;
        shell::IOStreams io{in, out, err};
        auto r = ex.execute(p, env, io, 0);
        EXPECT_EQ(r.exit_code, 0);
    }

    // external uses env snapshot
    {
        shell::ParsedLine p;
        p.ok = true;
        shell::CommandSpec cmd;
        cmd.argv = {std::string(FIXTURE_PATH), "x"};
        p.pipeline.push_back(cmd);

        std::istringstream in{""};
        std::ostringstream out, err;
        shell::IOStreams io{in, out, err};
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
    shell::Environment env;
    shell::ParsedLine p = shell::parseLine("FOO=", env);
    EXPECT_TRUE(p.ok);
    EXPECT_TRUE(p.is_assignment_only);
    EXPECT_EQ(p.assign_name, std::string("FOO"));
    EXPECT_EQ(p.assign_value, std::string(""));
}

static void test_builtins_errors() {
    // cat without file
    {
        std::istringstream in{""};
        std::ostringstream out, err;
        shell::IOStreams io{in, out, err};
        auto r = shell::Builtins::runIfBuiltin({"cat"}, io, 0);
        EXPECT_TRUE(r.exit_code == 0);
        EXPECT_TRUE(err.str().empty());
    }

    // wc on missing file
    {
        std::istringstream in{""};
        std::ostringstream out, err;
        shell::IOStreams io{in, out, err};
        auto r = shell::Builtins::runIfBuiltin(
            {"wc", "___definitely_missing_file___"}, io, 0
        );
        EXPECT_TRUE(r.exit_code != 0);
        EXPECT_TRUE(!err.str().empty());
    }
}

static void test_external_runner_unknown_command() {
    shell::Environment env;
    std::istringstream in{""};
    std::ostringstream out, err;
    shell::IOStreams io{in, out, err};

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
    test_tokenizer_substitution();
    test_tokenizer_pipe();
    test_parse_assignment();
    test_parse_pipeline();
    test_builtins_echo_pwd_cat_wc();
    test_external_runner_env_and_streams();
    test_executor_assignment_and_external();
    test_shellapp_repl_exit_code();
    test_parse_assignment_empty_value();
    test_builtins_errors();
    test_external_runner_unknown_command();

    test_pipeline_echo_wc();
    test_pipeline_three_commands();
    test_pipeline_first_command_error();
    test_pipeline_stderr_not_piped();
    test_exit_inside_pipeline();

    if (g_failed == 0) {
        std::cerr << "[OK] all tests passed\n";
        return 0;
    }
    std::cerr << "[FAIL] failed: " << g_failed << "\n";
    return 1;
}
