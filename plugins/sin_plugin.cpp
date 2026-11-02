#include "calc/plugin_api.h"

#include <cmath>
#include <cstring>
#include <exception>
#include <numbers>
#include <stdexcept>

namespace {

void set_error(char* buffer, std::size_t size, const char* message) {
    if (buffer != nullptr && size != 0) {
        strncpy_s(buffer, size, message, _TRUNCATE);
    }
}

int CALC_PLUGIN_CALL invoke_sine(const double* arguments, std::size_t count,
                                 double* result, char* error, std::size_t error_size) {
    try {
        if (arguments == nullptr || result == nullptr || count != 1) {
            throw std::invalid_argument("sin expects exactly one argument");
        }
        const double radians = arguments[0] * std::numbers::pi / 180.0;
        *result = std::sin(radians);
        return 0;
    } catch (const std::exception& exception) {
        set_error(error, error_size, exception.what());
        return 1;
    } catch (...) {
        set_error(error, error_size, "unknown error in sin");
        return 1;
    }
}

const CalcPluginDescriptor descriptor{
    CALC_PLUGIN_ABI_VERSION,
    sizeof(CalcPluginDescriptor),
    "sin",
    CalcPluginKind::function,
    1,
    0,
    CalcAssociativity::none,
    &invoke_sine,
};

}

CALC_PLUGIN_EXPORT const CalcPluginDescriptor* CALC_PLUGIN_CALL calc_get_plugin() {
    return &descriptor;
}

