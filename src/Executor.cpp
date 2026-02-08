#include "shell/Executor.hpp"
#include "shell/Builtins.hpp"
#include "shell/ExternalRunner.hpp"

namespace shell {

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

    // Built-ins
    CommandResult br = Builtins::runIfBuiltin(parsed.argv, io, last_exit_code);
    if (br.exit_code != -1) {
        return br;
    }

    // External command
    return ExternalRunner::run(parsed.argv, env.snapshot(), io);
}

}  // namespace shell
