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
 * @brief Splits a command line into tokens, supporting single and double
 * quotes.
 *
 * This tokenizer supports:
 * - Whitespace splitting outside quotes
 * - '...' and "..." where everything inside becomes part of one token
 * - Empty quoted strings produce an empty token ("" -> "")
 *
 * This HW2 tokenizer does not interpret escapes or substitutions.
 */
class Tokenizer final {
public:
    /**
     * @brief Tokenize one input line.
     * @param line Input command line.
     * @return TokenizeResult with tokens or an error.
     */
    static TokenizeResult tokenize(const std::string &line);
};

}  // namespace shell
