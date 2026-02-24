#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace shell {

/**
 * @brief Represents the shell environment (key-value variables).
 *
 * The environment is initialized from the current process environment and can
 * be modified using NAME=VALUE assignments. The snapshot can be passed to
 * external processes.
 */
class Environment final {
public:
    /**
     * @brief Constructs environment initialized from the current process
     * environment.
     */
    Environment();

    /**
     * @brief Sets or overwrites an environment variable.
     * @param name Variable name.
     * @param value Variable value.
     */
    void set(const std::string &name, const std::string &value);

    /**
     * @brief Returns a snapshot of the environment as a vector of "NAME=VALUE"
     * strings.
     *
     * This form is convenient for building envp blocks for process creation.
     */
    std::vector<std::string> snapshot() const;

private:
    std::unordered_map<std::string, std::string> vars_;
};

}  // namespace shell
