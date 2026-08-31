/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef MODPAYLOADINTEGRITY_H
#define MODPAYLOADINTEGRITY_H

#include <mod/Dune2RAssetManager.h>
#include <mod/ModTransferValidation.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace ModPayloadIntegrity {

inline bool isSha256Digest(const std::string& value) {
    return value.size() == 64
        && std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isxdigit(c) != 0;
        });
}

inline bool verifyChecksummedPayload(const std::filesystem::path& root,
                                     std::string& error,
                                     const std::string& managedStampName = ".dunecity-managed") {
    try {
        if(std::filesystem::is_symlink(root) || !std::filesystem::is_directory(root)) {
            error = "payload root is missing or is a symbolic link";
            return false;
        }

        const std::filesystem::path checksumPath = root / "checksums.sha256";
        if(std::filesystem::is_symlink(checksumPath)
           || !std::filesystem::is_regular_file(checksumPath)) {
            error = "checksums.sha256 is missing or unsafe";
            return false;
        }

        std::ifstream checksumFile(checksumPath);
        if(!checksumFile) {
            error = "checksums.sha256 is unreadable";
            return false;
        }

        std::map<std::string, std::pair<std::filesystem::path, std::string>> expectedFiles;
        std::string line;
        std::size_t lineNumber = 0;
        while(std::getline(checksumFile, line)) {
            ++lineNumber;
            if(!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if(line.size() < 67 || line[64] != ' ' || line[65] != ' ') {
                error = "invalid checksum entry on line " + std::to_string(lineNumber);
                return false;
            }

            std::string digest = line.substr(0, 64);
            std::transform(digest.begin(), digest.end(), digest.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if(!isSha256Digest(digest)) {
                error = "invalid SHA-256 digest on line " + std::to_string(lineNumber);
                return false;
            }

            std::filesystem::path relativePath;
            if(!ModTransferValidation::normalizeRelativeFilePath(line.substr(66), relativePath)
               || relativePath.generic_string() == "checksums.sha256"
               || relativePath.generic_string() == managedStampName) {
                error = "invalid checksum path on line " + std::to_string(lineNumber);
                return false;
            }

            const std::string key = ModTransferValidation::portablePathKey(relativePath);
            if(!expectedFiles.emplace(key, std::make_pair(relativePath, digest)).second) {
                error = "duplicate or case-colliding checksum path on line "
                    + std::to_string(lineNumber);
                return false;
            }
        }

        if(!checksumFile.eof() || expectedFiles.empty()) {
            error = "checksum manifest is empty or unreadable";
            return false;
        }

        for(const auto& expected : expectedFiles) {
            const auto& entry = expected.second;
            const std::filesystem::path filePath = root / entry.first;
            if(std::filesystem::is_symlink(filePath)
               || !std::filesystem::is_regular_file(filePath)) {
                error = "missing or unsafe payload file: " + entry.first.generic_string();
                return false;
            }
            if(Dune2RAssetManager::sha256File(filePath.string()) != entry.second) {
                error = "checksum mismatch: " + entry.first.generic_string();
                return false;
            }
        }

        std::set<std::string> actualFiles;
        for(const auto& directoryEntry : std::filesystem::recursive_directory_iterator(root)) {
            if(directoryEntry.is_symlink()) {
                error = "payload contains a symbolic link";
                return false;
            }
            if(!directoryEntry.is_regular_file()) {
                continue;
            }

            const std::filesystem::path relativePath =
                std::filesystem::relative(directoryEntry.path(), root);
            const std::string relativeName = relativePath.generic_string();
            if(relativeName == "checksums.sha256" || relativeName == managedStampName) {
                continue;
            }

            std::filesystem::path normalizedPath;
            if(!ModTransferValidation::normalizeRelativeFilePath(relativeName, normalizedPath)) {
                error = "payload contains a non-portable path: " + relativeName;
                return false;
            }
            const std::string key = ModTransferValidation::portablePathKey(normalizedPath);
            if(!actualFiles.insert(key).second) {
                error = "payload contains duplicate or case-colliding paths";
                return false;
            }
            if(expectedFiles.find(key) == expectedFiles.end()) {
                error = "payload file is absent from checksums.sha256: " + relativeName;
                return false;
            }
        }

        if(actualFiles.size() != expectedFiles.size()) {
            error = "checksums.sha256 contains files absent from the payload";
            return false;
        }
    } catch(const std::exception& e) {
        error = e.what();
        return false;
    }

    error.clear();
    return true;
}

} // namespace ModPayloadIntegrity

#endif // MODPAYLOADINTEGRITY_H
