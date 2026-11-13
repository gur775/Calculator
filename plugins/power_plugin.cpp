#include "calc/plugin_api.h"

#include <cmath>
#include <cstring>
#include <exception>
#include <stdexcept>

namespace {

void set_error(char* buffer, std::size_t size, const char* message) {
    if (buffer != nullptr && size != 0) {
        strncpy_s(buffer, size, message, _TRUNCATE);
    }
}

int CALC_PLUGIN_CALL invoke_power(const double* arguments, std::size_t count,
                                  double* result, char* error, std::size_t error_size) {
    try {
        if (arguments == nullptr || result == nullptr || count != 2) {
            throw std::invalid_argument("power expects exactly two arguments");
        }
        const double value = std::pow(arguments[0], arguments[1]);
        if (!std::isfinite(value)) {
            throw std::domain_error("power is undefined for these arguments");
        }
        *result = value;
        return 0;
    } catch (const std::exception& exception) {
        set_error(error, error_size, exception.what());
        return 1;
    } catch (...) {
        set_error(error, error_size, "unknown error in power");
        return 1;
    }
}

const CalcPluginDescriptor descriptor{
    CALC_PLUGIN_ABI_VERSION,
    sizeof(CalcPluginDescriptor),
    "^",
    CalcPluginKind::binary_operator,
    2,
    30,
    CalcAssociativity::right,
    &invoke_power,
};

}

CALC_PLUGIN_EXPORT const CalcPluginDescriptor* CALC_PLUGIN_CALL calc_get_plugin() {
    return &descriptor;
}

