#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace vkengine {

struct HeadlessCaptureConfig {
    bool enabled{false};
    std::filesystem::path outputPath;
    uint32_t width{1280};
    uint32_t height{720};
};

inline std::filesystem::path resolveRepoRoot()
{
    std::filesystem::path root = std::filesystem::current_path();
    auto stripConfigDir = [&root]() {
        const auto name = root.filename().string();
        if (name == "Debug" || name == "Release" || name == "RelWithDebInfo" || name == "MinSizeRel") {
            root = root.parent_path();
        }
    };

    if (root.filename() == "tests") {
        root = root.parent_path();
    }
    stripConfigDir();
    if (root.filename() == "build") {
        root = root.parent_path();
    }
    return root;
}

inline HeadlessCaptureConfig parseHeadlessCaptureArgs(int argc, char** argv, const std::string& defaultName)
{
    HeadlessCaptureConfig config{};
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--headless") {
            config.enabled = true;
            continue;
        }
        constexpr std::string_view capturePrefix = "--capture=";
        if (arg.rfind(capturePrefix, 0) == 0) {
            config.enabled = true;
            config.outputPath = std::filesystem::path(std::string(arg.substr(capturePrefix.size())));
            continue;
        }
        if (arg == "--capture" && i + 1 < argc) {
            config.enabled = true;
            config.outputPath = std::filesystem::path(argv[++i]);
            continue;
        }
    }

    if (config.enabled && config.outputPath.empty()) {
        const auto root = resolveRepoRoot();
        config.outputPath = root / "tests" / "test_results" / (defaultName + ".jpg");
    }

    return config;
}

} // namespace vkengine
