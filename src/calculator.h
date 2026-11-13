#pragma once

#include "calc/plugin_api.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

struct Operation {
    std::string name;
    CalcPluginKind kind = CalcPluginKind::function;
    std::uint32_t arity = 0;
    int precedence = 0;
    CalcAssociativity associativity = CalcAssociativity::none;
    std::function<double(const std::vector<double>&)> invoke;
};

class Calculator {
public:
    Calculator();

    void add_operation(Operation operation);
    [[nodiscard]] double evaluate(std::string_view expression) const;

private:
    std::vector<Operation> operations_;
};

