#include "MetaScanner.h"

#include <string>

#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/raw_ostream.h"

namespace {
    llvm::cl::OptionCategory Options("HuaMeta manifest v2 scanner");
    llvm::cl::opt<std::string> Output("output", llvm::cl::Required, llvm::cl::cat(Options));
    llvm::cl::opt<std::string> Depfile("depfile", llvm::cl::cat(Options));
    llvm::cl::opt<std::string> ResourceDirectory("resource-dir", llvm::cl::Required, llvm::cl::cat(Options));
    llvm::cl::opt<std::string> Module("module", llvm::cl::Required, llvm::cl::cat(Options));
    llvm::cl::opt<std::string> Configuration("configuration", llvm::cl::Required, llvm::cl::cat(Options));
    llvm::cl::opt<std::string> RepositoryRoot("repository-root", llvm::cl::Required, llvm::cl::cat(Options));
    llvm::cl::opt<std::string> EntryHeader("entry-header", llvm::cl::Required, llvm::cl::cat(Options));
}

int main(int argc, const char** argv) {
    llvm::InitLLVM llvm(argc, argv);
    if (argc == 2 && std::string(argv[1]) == "--version") {
        llvm::outs() << "HuaMeta 23.1.2-p6 manifest v2\n";
        return 0;
    }
    auto parsed = clang::tooling::CommonOptionsParser::create(argc, argv, Options);
    if (!parsed) {
        llvm::errs() << parsed.takeError();
        return 2;
    }
    HuaMeta::ScanOptions options;
    options.Output = Output;
    options.Depfile = Depfile;
    options.ResourceDirectory = ResourceDirectory;
    options.Module = Module;
    options.Configuration = Configuration;
    options.RepositoryRoot = RepositoryRoot;
    options.EntryHeader = EntryHeader;
    return HuaMeta::Scan(parsed->getCompilations(), parsed->getSourcePathList(), options);
}
