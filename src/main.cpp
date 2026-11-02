#include "calculator.h"
#include "plugin_manager.h"

#include <Windows.h>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::filesystem::path executable_directory() {
    std::wstring buffer(260, L'\0');
    while (true) {
        const DWORD length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            throw std::runtime_error("cannot determine executable location");
        }
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::string command_line_expression(int argument_count, char* arguments[]) {
    std::string expression;
    for (int index = 1; index < argument_count; ++index) {
        if (!expression.empty()) {
            expression.push_back(' ');
        }
        expression += arguments[index];
    }
    return expression;
}

bool evaluate_and_print(const Calculator& calculator, const std::string& expression) {
    try {
        double result = calculator.evaluate(expression);
        if (result == 0.0) {
            result = 0.0;
        }
        std::cout << std::setprecision(15) << result << '\n';
        return true;
    } catch (const std::exception& exception) {
        std::cerr << "Error: " << exception.what() << '\n';
        return false;
    }
}

}

int main(int argc, char* argv[]) {
    try {
        PluginManager plugins;
        plugins.load_from(executable_directory() / "plugins");
        for (const auto& warning : plugins.warnings()) {
            std::cerr << "Warning: " << warning << '\n';
        }

        Calculator calculator;
        for (const auto& operation : plugins.operations()) {
            try {
                calculator.add_operation(operation);
            } catch (const std::exception& exception) {
                std::cerr << "Warning: skipped operation '" << operation.name
                          << "': " << exception.what() << '\n';
            }
        }

        if (argc > 1) {
            return evaluate_and_print(calculator,
                                      command_line_expression(argc, argv))
                       ? 0
                       : 1;
        }

        bool success = true;
        std::string expression;
        while (std::getline(std::cin, expression)) {
            if (!expression.empty()) {
                success = evaluate_and_print(calculator, expression) && success;
            }
        }
        return success ? 0 : 1;
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 2;
    }
}

