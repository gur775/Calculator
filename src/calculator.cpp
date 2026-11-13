#include "calculator.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace {

Operation make_binary(std::string name, int precedence,
                      std::function<double(double, double)> function) {
    return Operation{
        std::move(name),
        CalcPluginKind::binary_operator,
        2,
        precedence,
        CalcAssociativity::left,
        [function = std::move(function)](const std::vector<double>& arguments) {
            return function(arguments.at(0), arguments.at(1));
        },
    };
}

bool is_identifier_start(char value) {
    const auto character = static_cast<unsigned char>(value);
    return std::isalpha(character) != 0 || value == '_';
}

bool is_identifier_part(char value) {
    const auto character = static_cast<unsigned char>(value);
    return std::isalnum(character) != 0 || value == '_';
}

class Parser {
public:
    Parser(std::string_view input, const std::vector<Operation>& operations)
        : input_(input), operations_(operations) {}

    double parse() {
        const double result = parse_expression(0);
        skip_spaces();
        if (position_ != input_.size()) {
            fail("unexpected token");
        }
        if (!std::isfinite(result)) {
            throw std::runtime_error("calculation produced a non-finite result");
        }
        return result;
    }

private:
    [[noreturn]] void fail(std::string_view message) const {
        throw std::runtime_error(std::string(message) + " at position " +
                                 std::to_string(position_ + 1));
    }

    void skip_spaces() {
        while (position_ < input_.size() &&
               std::isspace(static_cast<unsigned char>(input_[position_])) != 0) {
            ++position_;
        }
    }

    const Operation* find_function(std::string_view name) const {
        const auto iterator = std::find_if(
            operations_.begin(), operations_.end(), [&](const Operation& operation) {
                return operation.kind == CalcPluginKind::function && operation.name == name;
            });
        return iterator == operations_.end() ? nullptr : &*iterator;
    }

    const Operation* match_operator() {
        skip_spaces();
        const Operation* match = nullptr;
        for (const auto& operation : operations_) {
            if (operation.kind != CalcPluginKind::binary_operator ||
                operation.name.size() > input_.size() - position_) {
                continue;
            }
            if (input_.substr(position_, operation.name.size()) == operation.name &&
                (match == nullptr || operation.name.size() > match->name.size())) {
                match = &operation;
            }
        }
        return match;
    }

    double parse_expression(int minimum_precedence) {
        double left = parse_prefix();

        while (true) {
            const std::size_t before_operator = position_;
            const Operation* operation = match_operator();
            if (operation == nullptr || operation->precedence < minimum_precedence) {
                position_ = before_operator;
                break;
            }

            position_ += operation->name.size();
            const int right_precedence =
                operation->associativity == CalcAssociativity::right
                    ? operation->precedence
                    : operation->precedence + 1;
            const double right = parse_expression(right_precedence);
            left = operation->invoke({left, right});
            if (!std::isfinite(left)) {
                throw std::runtime_error("operation '" + operation->name +
                                         "' produced a non-finite result");
            }
        }
        return left;
    }

    double parse_prefix() {
        skip_spaces();
        if (position_ == input_.size()) {
            fail("expected a value");
        }

        if (input_[position_] == '+' || input_[position_] == '-') {
            const char sign = input_[position_++];
            const double value = parse_expression(25);
            return sign == '-' ? -value : value;
        }

        if (input_[position_] == '(') {
            ++position_;
            const double value = parse_expression(0);
            skip_spaces();
            if (position_ == input_.size() || input_[position_] != ')') {
                fail("expected ')'");
            }
            ++position_;
            return value;
        }

        if (is_identifier_start(input_[position_])) {
            return parse_function_call();
        }

        return parse_number();
    }

    double parse_function_call() {
        const std::size_t name_start = position_++;
        while (position_ < input_.size() && is_identifier_part(input_[position_])) {
            ++position_;
        }
        const std::string_view name = input_.substr(name_start, position_ - name_start);
        const Operation* function = find_function(name);
        if (function == nullptr) {
            throw std::runtime_error("unknown function '" + std::string(name) + "'");
        }

        skip_spaces();
        if (position_ == input_.size() || input_[position_] != '(') {
            fail("expected '('");
        }
        ++position_;

        std::vector<double> arguments;
        skip_spaces();
        if (position_ < input_.size() && input_[position_] != ')') {
            while (true) {
                arguments.push_back(parse_expression(0));
                skip_spaces();
                if (position_ == input_.size() || input_[position_] != ',') {
                    break;
                }
                ++position_;
            }
        }

        skip_spaces();
        if (position_ == input_.size() || input_[position_] != ')') {
            fail("expected ')' after function arguments");
        }
        ++position_;

        if (arguments.size() != function->arity) {
            throw std::runtime_error("function '" + function->name + "' expects " +
                                     std::to_string(function->arity) + " argument(s)");
        }
        return function->invoke(arguments);
    }

    double parse_number() {
        const char* first = input_.data() + position_;
        const char* last = input_.data() + input_.size();
        double value = 0.0;
        const auto result = std::from_chars(first, last, value, std::chars_format::general);
        if (result.ec != std::errc{} || result.ptr == first) {
            fail("expected a number");
        }
        position_ += static_cast<std::size_t>(result.ptr - first);
        if (!std::isfinite(value)) {
            fail("number is outside the supported range");
        }
        return value;
    }

    std::string_view input_;
    const std::vector<Operation>& operations_;
    std::size_t position_ = 0;
};

}

Calculator::Calculator() {
    add_operation(make_binary("+", 10, [](double left, double right) {
        return left + right;
    }));
    add_operation(make_binary("-", 10, [](double left, double right) {
        return left - right;
    }));
    add_operation(make_binary("*", 20, [](double left, double right) {
        return left * right;
    }));
    add_operation(make_binary("/", 20, [](double left, double right) {
        if (right == 0.0) {
            throw std::domain_error("division by zero");
        }
        return left / right;
    }));
}

void Calculator::add_operation(Operation operation) {
    if (operation.name.empty() || !operation.invoke) {
        throw std::invalid_argument("operation metadata is incomplete");
    }
    const auto duplicate = std::find_if(
        operations_.begin(), operations_.end(), [&](const Operation& existing) {
            return existing.kind == operation.kind && existing.name == operation.name;
        });
    if (duplicate != operations_.end()) {
        throw std::invalid_argument("duplicate operation '" + operation.name + "'");
    }
    operations_.push_back(std::move(operation));
}

double Calculator::evaluate(std::string_view expression) const {
    if (expression.find_first_not_of(" \t\r\n") == std::string_view::npos) {
        throw std::invalid_argument("expression is empty");
    }
    return Parser(expression, operations_).parse();
}

