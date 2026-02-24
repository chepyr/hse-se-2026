#pragma once
#include <string>
#include <vector>

namespace shell {

/**
 * @brief Result of tokenizing a single input line.
 */
struct TokenizeResult final {
    /** @brief True if tokenization succeeded. */
    bool ok = false;

    /** @brief Tokens (argv-style). Valid only if ok==true. */
    std::vector<std::string> tokens;

    /** @brief Error message (human-readable). Valid only if ok==false. */
    std::string error;
};

/**
 * @brief Splits a command line into tokens, supporting quotes and variable
 * substitution.
 *
 * This tokenizer supports:
 * - Whitespace splitting outside quotes
 * - '...' (single quotes - no substitution)
 * - "..." (double quotes - with substitution)
 * - $VAR and ${VAR} variable substitution
 * - Pipe character | as a special separator token
 *
 * Empty quoted strings produce an empty token ("" -> "")
 */
class Tokenizer final {
public:
    /**
     * @brief Tokenize one input line with variable substitution.
     * @param line Input command line.
     * @param env Environment for variable substitution.
     * @return TokenizeResult with tokens or an error.
     */
    static TokenizeResult
    tokenize(const std::string &line, const class Environment &env);
};

}  // namespace shell
