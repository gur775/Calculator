#include "plugin_manager.h"

#include "calc/plugin_api.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

std::string path_text(const std::filesystem::path& path) {
    return path.string();
}

bool is_identifier(std::string_view name) {
    if (name.empty()) {
        return false;
    }
    const auto first = static_cast<unsigned char>(name.front());
    if (std::isalpha(first) == 0 && name.front() != '_') {
        return false;
    }
    return std::all_of(name.begin() + 1, name.end(), [](char value) {
        const auto character = static_cast<unsigned char>(value);
        return std::isalnum(character) != 0 || value == '_';
    });
}

bool is_operator_token(std::string_view name) {
    if (name.empty() || name.size() > 8) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](char value) {
        const auto character = static_cast<unsigned char>(value);
        return std::isspace(character) == 0 && std::isalnum(character) == 0 &&
               value != '_' && value != '(' && value != ')' && value != ',';
    });
}

std::string windows_error(DWORD code) {
    return "Windows error " + std::to_string(code);
}

class ModuleGuard {
public:
    explicit ModuleGuard(HMODULE module) noexcept : module_(module) {}

    ~ModuleGuard() {
        if (module_ != nullptr) {
            FreeLibrary(module_);
        }
    }

    ModuleGuard(const ModuleGuard&) = delete;
    ModuleGuard& operator=(const ModuleGuard&) = delete;

    void release() noexcept {
        module_ = nullptr;
    }

private:
    HMODULE module_;
};

Operation make_operation(const CalcPluginDescriptor* descriptor) {
    const std::string name(descriptor->name);
    return Operation{
        name,
        descriptor->kind,
        descriptor->arity,
        descriptor->precedence,
        descriptor->associativity,
        [descriptor, name](const std::vector<double>& arguments) {
            std::array<char, 512> error{};
            double result = 0.0;
            int status = -1;
            try {
                status = descriptor->invoke(arguments.data(), arguments.size(), &result,
                                            error.data(), error.size());
            } catch (const std::exception& exception) {
                throw std::runtime_error("plugin '" + name + "' threw an exception: " +
                                         exception.what());
            } catch (...) {
                throw std::runtime_error("plugin '" + name +
                                         "' threw an unknown exception");
            }
            if (status != 0) {
                const std::size_t length = strnlen_s(error.data(), error.size());
                const std::string message = length == 0
                                                ? "plugin returned error " +
                                                      std::to_string(status)
                                                : std::string(error.data(), length);
                throw std::runtime_error("plugin '" + name + "': " + message);
            }
            return result;
        },
    };
}

std::string validate(const CalcPluginDescriptor* descriptor) {
    if (descriptor == nullptr) {
        return "discovery function returned null";
    }
    if (descriptor->abi_version != CALC_PLUGIN_ABI_VERSION) {
        return "unsupported plugin ABI version";
    }
    if (descriptor->struct_size < sizeof(CalcPluginDescriptor)) {
        return "plugin descriptor is too small";
    }
    if (descriptor->name == nullptr || descriptor->invoke == nullptr) {
        return "plugin descriptor is incomplete";
    }

    const std::string_view name(descriptor->name);
    if (descriptor->kind == CalcPluginKind::function) {
        if (!is_identifier(name)) {
            return "invalid function name";
        }
        if (descriptor->arity == 0 || descriptor->arity > 16) {
            return "function arity must be between 1 and 16";
        }
    } else if (descriptor->kind == CalcPluginKind::binary_operator) {
        if (!is_operator_token(name)) {
            return "invalid operator token";
        }
        if (descriptor->arity != 2) {
            return "a binary operator must have arity 2";
        }
        if (descriptor->precedence < 1 || descriptor->precedence > 100) {
            return "operator precedence must be between 1 and 100";
        }
        if (descriptor->associativity != CalcAssociativity::left &&
            descriptor->associativity != CalcAssociativity::right) {
            return "invalid operator associativity";
        }
    } else {
        return "unknown plugin kind";
    }
    return {};
}

}

PluginManager::~PluginManager() {
    operations_.clear();
    for (auto iterator = modules_.rbegin(); iterator != modules_.rend(); ++iterator) {
        FreeLibrary(static_cast<HMODULE>(*iterator));
    }
}

void PluginManager::load_from(const std::filesystem::path& directory) {
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        warnings_.push_back("plugin directory not found: " + path_text(directory));
        return;
    }

    std::vector<std::filesystem::path> dll_files;
    for (std::filesystem::directory_iterator iterator(directory, error), end;
         iterator != end && !error; iterator.increment(error)) {
        if (iterator->is_regular_file(error) &&
            _wcsicmp(iterator->path().extension().c_str(), L".dll") == 0) {
            dll_files.push_back(iterator->path());
        }
    }
    if (error) {
        warnings_.push_back("cannot read plugin directory: " + error.message());
    }
    std::sort(dll_files.begin(), dll_files.end());
    if (dll_files.empty()) {
        warnings_.push_back("no DLL plugins found in: " + path_text(directory));
        return;
    }

    for (const auto& path : dll_files) {
        HMODULE module = LoadLibraryW(path.c_str());
        if (module == nullptr) {
            warnings_.push_back("cannot load '" + path_text(path) + "': " +
                                windows_error(GetLastError()));
            continue;
        }
        ModuleGuard module_guard(module);

        const auto discovery = reinterpret_cast<CalcGetPluginFn>(
            GetProcAddress(module, "calc_get_plugin"));
        if (discovery == nullptr) {
            warnings_.push_back("invalid plugin '" + path_text(path) +
                                "': calc_get_plugin export is missing");
            continue;
        }

        const CalcPluginDescriptor* descriptor = nullptr;
        try {
            descriptor = discovery();
        } catch (const std::exception& exception) {
            warnings_.push_back("invalid plugin '" + path_text(path) +
                                "': discovery threw: " + exception.what());
            continue;
        } catch (...) {
            warnings_.push_back("invalid plugin '" + path_text(path) +
                                "': discovery threw an unknown exception");
            continue;
        }

        const std::string validation_error = validate(descriptor);
        if (!validation_error.empty()) {
            warnings_.push_back("invalid plugin '" + path_text(path) + "': " +
                                validation_error);
            continue;
        }

        const bool duplicate = std::any_of(
            operations_.begin(), operations_.end(), [&](const Operation& operation) {
                return operation.kind == descriptor->kind &&
                       operation.name == descriptor->name;
            });
        if (duplicate) {
            warnings_.push_back("duplicate plugin operation '" +
                                std::string(descriptor->name) + "' in: " +
                                path_text(path));
            continue;
        }

        operations_.push_back(make_operation(descriptor));
        try {
            modules_.push_back(module);
        } catch (...) {
            operations_.pop_back();
            throw;
        }
        module_guard.release();
    }

    if (operations_.empty()) {
        warnings_.push_back("no valid plugins were loaded");
    }
}

const std::vector<Operation>& PluginManager::operations() const noexcept {
    return operations_;
}

const std::vector<std::string>& PluginManager::warnings() const noexcept {
    return warnings_;
}

