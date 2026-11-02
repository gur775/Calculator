#pragma once

#include "calc/plugin_api.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

struct Operation {
    std::string name;
    CalcPluginKind kind;
    std::uint32_t arity;
    int precedence;
    CalcAssociativity associativity;
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

