#include "shell/Parse.hpp"
#include <cstddef>
#include "shell/Environment.hpp"
#include "shell/Tokenizer.hpp"
#include "shell/Utils.hpp"

namespace shell {

static bool isAssignmentOnlyToken(
    const std::string &tok,
    std::string &name,
    std::string &value
) {
    const std::size_t pos = tok.find('=');
    if (pos == std::string::npos) {
        return false;
    }
    if (pos == 0) {
        return false;
    }

    name = tok.substr(0, pos);
    value = tok.substr(pos + 1);

    return Utils::isValidEnvName(name);
}

ParsedLine parseLine(const std::string &line, const Environment &env) {
    ParsedLine result;
    result.ok = false;

    TokenizeResult token_result = Tokenizer::tokenize(line, env);
    if (!token_result.ok) {
        result.error = token_result.error;
        return result;
    }

    if (token_result.tokens.empty()) {
        result.ok = true;
        result.is_empty = true;
        return result;
    }

    if (token_result.tokens.size() == 1) {
        std::string name;
        std::string value;
        if (isAssignmentOnlyToken(token_result.tokens[0], name, value)) {
            result.ok = true;
            result.is_assignment_only = true;
            result.assign_name = std::move(name);
            result.assign_value = std::move(value);
            return result;
        }
    }

    std::vector<CommandSpec> commands;
    CommandSpec current_cmd;

    for (const auto &token : token_result.tokens) {
        if (token == "|") {
            if (current_cmd.argv.empty()) {
                result.error = "Empty command before pipe";
                return result;
            }
            commands.push_back(std::move(current_cmd));
            current_cmd = CommandSpec{};
        } else {
            current_cmd.argv.push_back(token);
        }
    }

    if (current_cmd.argv.empty()) {
        result.error = "Empty command after pipe";
        return result;
    }
    commands.push_back(std::move(current_cmd));

    result.ok = true;
    result.pipeline = std::move(commands);
    return result;
}

}  // namespace shell
