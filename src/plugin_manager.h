#pragma once

#include "calculator.h"

#include <filesystem>
#include <string>
#include <vector>

class PluginManager {
public:
    PluginManager() = default;
    ~PluginManager();

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    void load_from(const std::filesystem::path& directory);

    [[nodiscard]] const std::vector<Operation>& operations() const noexcept;
    [[nodiscard]] const std::vector<std::string>& warnings() const noexcept;

private:
    std::vector<void*> modules_;
    std::vector<Operation> operations_;
    std::vector<std::string> warnings_;
};

