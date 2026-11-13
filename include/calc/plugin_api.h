#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#  define CALC_PLUGIN_CALL __cdecl
#  if defined(CALC_PLUGIN_BUILD)
#    define CALC_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#  else
#    define CALC_PLUGIN_EXPORT extern "C"
#  endif
#else
#  define CALC_PLUGIN_CALL
#  define CALC_PLUGIN_EXPORT extern "C"
#endif

inline constexpr std::uint32_t CALC_PLUGIN_ABI_VERSION = 1;

enum class CalcPluginKind : std::uint32_t {
    function = 1,
    binary_operator = 2,
};

enum class CalcAssociativity : std::uint32_t {
    none = 0,
    left = 1,
    right = 2,
};


using CalcInvokeFn = int(CALC_PLUGIN_CALL*)(
    const double* arguments,
    std::size_t argument_count,
    double* result,
    char* error_buffer,
    std::size_t error_buffer_size);

struct CalcPluginDescriptor {
    std::uint32_t abi_version;
    std::uint32_t struct_size;
    const char* name;
    CalcPluginKind kind;
    std::uint32_t arity;
    std::int32_t precedence;
    CalcAssociativity associativity;
    CalcInvokeFn invoke;
};

using CalcGetPluginFn = const CalcPluginDescriptor*(CALC_PLUGIN_CALL*)();


CALC_PLUGIN_EXPORT const CalcPluginDescriptor* CALC_PLUGIN_CALL calc_get_plugin();

