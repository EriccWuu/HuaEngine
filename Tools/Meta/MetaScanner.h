#pragma once

#include <string>
#include <vector>

namespace clang::tooling { class CompilationDatabase; }

namespace HuaMeta {
    struct ScanOptions {
        std::string Output;
        std::string Depfile;
        std::string ResourceDirectory;
        std::string Module;
        std::string Configuration;
        std::string RepositoryRoot;
        std::string EntryHeader;
    };

    int Scan(const clang::tooling::CompilationDatabase& database,
             const std::vector<std::string>& sourcePaths, const ScanOptions& options);
}
