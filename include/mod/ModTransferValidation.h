#ifndef MODTRANSFERVALIDATION_H
#define MODTRANSFERVALIDATION_H

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

namespace ModTransferValidation {

inline bool isReservedWindowsName(std::string_view component) {
    const std::size_t dot = component.find('.');
    std::string stem(component.substr(0, dot));
    std::transform(stem.begin(), stem.end(), stem.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if(stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL") {
        return true;
    }
    if(stem.size() == 4 && (stem.rfind("COM", 0) == 0 || stem.rfind("LPT", 0) == 0)
       && stem[3] >= '1' && stem[3] <= '9') {
        return true;
    }
    return false;
}

inline bool isPortablePathComponent(std::string_view component) {
    if(component.empty() || component == "." || component == ".."
       || component.back() == '.' || component.back() == ' '
       || isReservedWindowsName(component)) {
        return false;
    }

    constexpr std::string_view forbidden = "<>:\"/\\|?*";
    for(const unsigned char c : component) {
        if(c < 32 || c == 127 || forbidden.find(static_cast<char>(c)) != std::string_view::npos) {
            return false;
        }
    }
    return true;
}

inline bool isValidModName(std::string_view name) {
    return name.size() <= 128 && isPortablePathComponent(name);
}

inline bool normalizeRelativeFilePath(std::string fileName,
                                      std::filesystem::path& normalizedPath) {
    if(fileName.empty() || fileName.size() > 512
       || fileName.find('\0') != std::string::npos) {
        return false;
    }

    std::replace(fileName.begin(), fileName.end(), '\\', '/');
    if(fileName.front() == '/' || fileName.back() == '/') {
        return false;
    }

    std::size_t start = 0;
    while(start < fileName.size()) {
        const std::size_t separator = fileName.find('/', start);
        const std::size_t length = separator == std::string::npos
            ? fileName.size() - start
            : separator - start;
        if(!isPortablePathComponent(std::string_view(fileName).substr(start, length))) {
            return false;
        }
        if(separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }

    normalizedPath = std::filesystem::path(fileName).lexically_normal();
    return !normalizedPath.empty() && !normalizedPath.is_absolute()
        && !normalizedPath.has_root_name() && !normalizedPath.has_root_directory();
}

inline std::string portablePathKey(const std::filesystem::path& path) {
    std::string key = path.generic_string();
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a')
                                    : static_cast<char>(c);
    });
    return key;
}

} // namespace ModTransferValidation

#endif // MODTRANSFERVALIDATION_H
