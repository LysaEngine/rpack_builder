/*
 * Copyright (c) 2024-present Henri Michelon
 *
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
*/
#include <cxxopts.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using uint32 = uint32_t;
using uint64 = uint64_t;

// Keep in sync with ResourcesPack.ixx
static constexpr char MAGIC[] { 'L','Y','P','A','C','K' };
static constexpr uint32 VERSION { 1 };
static constexpr size_t PATH_SIZE{ 256 };

#pragma pack(push, 1)
struct Header {
    char   magic[6];
    uint32 version;
    uint32 count;
};

struct Entry {
    char   path[PATH_SIZE];
    uint64 offset;
    uint64 size;
};
#pragma pack(pop)

std::vector<std::string> readFileList(const std::filesystem::path& listPath)
{
    std::ifstream in(listPath);
    if (!in) throw std::runtime_error("Cannot open file list: " + listPath.string());

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        // strip trailing \r (Windows line endings)
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        lines.push_back(line);
    }
    return lines;
}

void buildPack(
    const std::filesystem::path& outputPath,
    const std::vector<std::string>& resourcePaths,
    const std::filesystem::path& baseDir,
    bool verbose) {
    const size_t count = resourcePaths.size();

    // Validate paths & collect file sizes
    struct FileInfo {
        std::filesystem::path diskPath;
        uint64 size;
    };

    std::vector<FileInfo> files;
    files.reserve(count);

    for (const auto& rp : resourcePaths) {
        if (rp.size() >= PATH_SIZE) throw std::runtime_error("Path too long (>= 256 chars): " + rp);

        std::filesystem::path disk = baseDir / rp;
        if (!std::filesystem::exists(disk)) throw std::runtime_error("File not found: " + disk.string());

        auto sz = std::filesystem::file_size(disk);
        files.push_back({ std::move(disk), (sz) });

        if (verbose) std::cout << "  + " << rp << "  (" << sz << " bytes)\n";
    }

    // Compute data-blob offsets
    const uint64 blobStart = sizeof(Header) + static_cast<uint64>(count) * sizeof(Entry);

    std::vector<uint64> offsets(count);
    uint64 cursor = 0;
    for (size_t i = 0; i < count; ++i) {
        offsets[i] = cursor;
        cursor += files[i].size;
    }

    // Build directory
    std::vector<Entry> directory(count);
    for (size_t i = 0; i < count; ++i) {
        Entry& e = directory[i];
        std::memset(&e, 0, sizeof(e));
        std::strncpy(e.path, resourcePaths[i].c_str(), PATH_SIZE - 1);
        e.offset = blobStart + offsets[i];
        e.size = files[i].size;
    }

    // Write the output file
    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot open output file: " + outputPath.string());

    // Header
    Header hdr{};
    std::memcpy(hdr.magic, MAGIC, 6);
    hdr.version = VERSION;
    hdr.count   = static_cast<uint32>(count);
    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    // Directory
    for (const auto& e : directory) {
        out.write(reinterpret_cast<const char*>(&e), sizeof(e));
    }

    // Data blob — stream each resource file
    constexpr size_t BUF_SIZE = 1024 * 1024; // 1 MiB copy buffer
    std::vector<char> buf(BUF_SIZE);

    for (size_t i = 0; i < count; ++i) {
        std::ifstream src(files[i].diskPath, std::ios::binary);
        if (!src) throw std::runtime_error("Cannot read: " + files[i].diskPath.string());

        uint64 remaining = files[i].size;
        while (remaining > 0) {
            size_t chunk = std::min(remaining, static_cast<uint64>(BUF_SIZE));
            src.read(buf.data(), static_cast<std::streamsize>(chunk));
            auto read = static_cast<size_t>(src.gcount());
            if (read == 0) throw std::runtime_error("Unexpected EOF: " + files[i].diskPath.string());
            out.write(buf.data(), static_cast<std::streamsize>(read));
            remaining -= read;
        }
    }

    if (!out) throw std::runtime_error("Write error on: " + outputPath.string());

    uint64 totalSize = out.tellp();
    out.close();

    if (verbose) {
        auto formatSize = [](const uint64 bytes) -> std::string {
            constexpr double KB = 1024.0;
            constexpr double MB = 1024.0 * 1024.0;
            constexpr double GB = 1024.0 * 1024.0 * 1024.0;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2);
            if (bytes >= static_cast<uint64>(GB))
                oss << bytes / GB << " GiB";
            else if (bytes >= static_cast<uint64>(MB))
                oss << bytes / MB << " MiB";
            else if (bytes >= static_cast<uint64>(KB))
                oss << bytes / KB << " KiB";
            else
                oss << bytes << " B";
            oss << "  (" << bytes << " bytes)";
            return oss.str();
        };

        std::cout << "Pack written: " << outputPath.string() << "\n"
                  << "  Resources : " << count << "\n"
                  << "  Total size: " << formatSize(totalSize) << "\n";
    }
}

int main(const int argc, char* argv[])
{
    cxxopts::Options options("rpack_builder", "Build a binary resources pack");

    options.add_options()
        ("o,output",  "Output .rpack file path",
            cxxopts::value<std::string>())
        ("l,list",    "Text file listing resource paths (one per line, relative to base dir)",
            cxxopts::value<std::string>())
        ("b,base",    "Base directory on disk (default: current directory)",
            cxxopts::value<std::string>()->default_value("."))
        ("v,verbose", "Print each resource being added",
            cxxopts::value<bool>()->default_value("false"))
        ("h,help",    "Print help")
    ;

    cxxopts::ParseResult result;
    try {
        result = options.parse(argc, argv);
    } catch (const cxxopts::exceptions::exception& ex) {
        std::cerr << "Argument error: " << ex.what() << "\n\n"
                  << options.help() << "\n";
        return 1;
    }

    if (result.count("help") || argc == 1) {
        std::cout << options.help() << "\n";
        return 0;
    }

    // Validate required options
    bool ok = true;
    if (!result.count("output")) { std::cerr << "Error: --output is required\n"; ok = false; }
    if (!result.count("list"))   { std::cerr << "Error: --list is required\n";   ok = false; }
    if (!ok) {
        std::cerr << "\n" << options.help() << "\n";
        return 1;
    }

    const std::filesystem::path outputPath = result["output"].as<std::string>();
    const std::filesystem::path listPath = result["list"].as<std::string>();
    const std::filesystem::path baseDir = result["base"].as<std::string>();
    const bool verbose = result["verbose"].as<bool>();

    try {
        if (verbose) {
            std::cout << "Base directory : " << std::filesystem::absolute(baseDir) << "\n"
                      << "File list      : " << listPath << "\n"
                      << "Output         : " << outputPath << "\n\n";
        }

        const auto resourcePaths = readFileList(listPath);
        if (resourcePaths.empty()) throw std::runtime_error("File list is empty: " + listPath.string());

        if (verbose) std::cout << "Adding " << resourcePaths.size() << " resource(s):\n";

        buildPack(outputPath, resourcePaths, baseDir, verbose);

    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
