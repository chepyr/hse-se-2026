#include <iostream>
#include "shell/ShellApp.hpp"

/**
 * @brief Program entry point.
 */
int main() {
    shell::ShellApp app(std::cin, std::cout, std::cerr);
    return app.run();
}
