#include "MetaScanner.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/SHA256.h"
#include "llvm/Support/raw_ostream.h"

namespace HuaMeta {
namespace {
    namespace fs = std::filesystem;
    constexpr std::string_view ToolVersion = "23.1.2-p6";

    fs::path Utf8Path(std::string_view value) {
        return fs::u8path(value.begin(), value.end());
    }

    std::string Utf8GenericString(const fs::path& value) {
        const auto encoded = value.generic_u8string();
        return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
    }

    std::string Normalize(const fs::path& value) {
        std::error_code error;
        auto path = fs::weakly_canonical(value, error);
        if (error) path = fs::absolute(value, error).lexically_normal();
        return Utf8GenericString(path);
    }

    std::string Identity(const fs::path& value) {
        std::string result = Normalize(value);
#ifdef _WIN32
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
#endif
        return result;
    }

    std::string SourceName(std::string_view filename, const ScanOptions& options) {
        const auto file = Normalize(Utf8Path(filename));
        const auto root = Normalize(Utf8Path(options.RepositoryRoot));
        const auto identity = Identity(Utf8Path(filename));
        const auto rootIdentity = Identity(Utf8Path(options.RepositoryRoot));
        if (file.size() > root.size() && identity.compare(0, rootIdentity.size(), rootIdentity) == 0 && file[root.size()] == '/') {
            return file.substr(root.size() + 1);
        }
        return file;
    }

    std::string Hash(std::string_view input) {
        const auto bytes = llvm::ArrayRef<uint8_t>(reinterpret_cast<const uint8_t*>(input.data()), input.size());
        return llvm::toHex(llvm::SHA256::hash(bytes), true);
    }

    std::optional<std::string> FileHash(const std::string& file) {
        std::ifstream input(Utf8Path(file), std::ios::binary);
        if (!input) return std::nullopt;
        const std::string body(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
        return Hash(body);
    }

    std::string Trim(std::string value) {
        auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
        auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
        if (first >= last) return {};
        return std::string(first, last);
    }

    bool OnlyTrivia(llvm::StringRef text) {
        size_t index = 0;
        while (index < text.size()) {
            if (std::isspace(static_cast<unsigned char>(text[index]))) { ++index; continue; }
            if (index + 1 < text.size() && text[index] == '/' && text[index + 1] == '/') {
                index += 2;
                while (index < text.size() && text[index] != '\n') ++index;
                continue;
            }
            if (index + 1 < text.size() && text[index] == '/' && text[index + 1] == '*') {
                const auto end = text.find("*/", index + 2);
                if (end == llvm::StringRef::npos) return false;
                index = end + 2;
                continue;
            }
            return false;
        }
        return true;
    }

    std::vector<std::string> SplitArguments(std::string_view text) {
        std::vector<std::string> parts;
        size_t begin = 0;
        unsigned parentheses = 0;
        unsigned angles = 0;
        bool quoted = false;
        bool escaped = false;
        for (size_t index = 0; index < text.size(); ++index) {
            const char ch = text[index];
            if (quoted) {
                if (escaped) escaped = false;
                else if (ch == '\\') escaped = true;
                else if (ch == '"') quoted = false;
                continue;
            }
            if (ch == '"') quoted = true;
            else if (ch == '(') ++parentheses;
            else if (ch == ')') { if (parentheses) --parentheses; }
            else if (ch == '<') ++angles;
            else if (ch == '>') { if (angles) --angles; }
            else if (ch == ',' && parentheses == 0 && angles == 0) {
                auto part = Trim(std::string(text.substr(begin, index - begin)));
                if (!part.empty()) parts.push_back(std::move(part));
                begin = index + 1;
            }
        }
        auto tail = Trim(std::string(text.substr(begin)));
        if (!tail.empty()) parts.push_back(std::move(tail));
        return parts;
    }

    std::vector<std::string> MacroArguments(std::string_view raw) {
        const auto open = raw.find('(');
        const auto close = raw.rfind(')');
        if (open == std::string_view::npos || close == std::string_view::npos || close <= open) return {};
        return SplitArguments(raw.substr(open + 1, close - open - 1));
    }

    std::map<std::string, std::string> Metadata(std::string_view raw) {
        std::map<std::string, std::string> result;
        for (const auto& token : MacroArguments(raw)) {
            const auto equal = token.find('=');
            if (equal == std::string::npos) continue;
            auto key = Trim(token.substr(0, equal));
            auto value = Trim(token.substr(equal + 1));
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
            result[std::move(key)] = std::move(value);
        }
        return result;
    }

    std::string MetadataValue(const std::map<std::string, std::string>& values, std::string_view key) {
        auto found = values.find(std::string(key));
        return found == values.end() ? std::string{} : found->second;
    }

    bool ValidGuid(std::string_view value) {
        if (value.size() != 32) return false;
        return std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isxdigit(ch); });
    }

    enum class MarkerKind { Component, Field, Enum };
    struct Marker {
        MarkerKind Kind;
        clang::FileID File;
        unsigned End = 0;
        std::string Raw;
        std::string Source;
        unsigned Line = 0;
        unsigned Column = 0;
        bool Used = false;
    };

    struct State {
        explicit State(const ScanOptions& options) : Options(options), Entry(Identity(Utf8Path(options.EntryHeader))) {}
        const ScanOptions& Options;
        std::string Entry;
        std::vector<Marker> Markers;
        std::set<std::string> Files;
        std::map<std::string, std::string> GuidOwners;
        llvm::json::Array Types;
        llvm::json::Array Enums;
        llvm::json::Array Diagnostics;
        bool Failed = false;

        void Error(std::string code, std::string message, std::string source = {}, unsigned line = 0, unsigned column = 0) {
            llvm::json::Object diagnostic{{"severity", "error"}, {"code", std::move(code)},
                {"message", std::move(message)}, {"source", std::move(source)}, {"line", line}, {"column", column}};
            Diagnostics.push_back(std::move(diagnostic));
            Failed = true;
        }
        void AddFile(std::string_view filename) {
            if (!filename.empty()) Files.insert(Normalize(Utf8Path(filename)));
        }
        bool InEntry(const clang::SourceManager& source, clang::SourceLocation location) const {
            if (location.isInvalid()) return false;
            const auto filename = source.getFilename(source.getExpansionLoc(location));
            return !filename.empty() && Identity(Utf8Path(filename.str())) == Entry;
        }
    };

    class Markers final : public clang::PPCallbacks {
    public:
        Markers(clang::Preprocessor& preprocessor, State& state) : Preprocessor(preprocessor), Data(state) {}
        void FileChanged(clang::SourceLocation location, FileChangeReason reason,
                         clang::SrcMgr::CharacteristicKind, clang::FileID) override {
            if (reason != EnterFile || location.isInvalid()) return;
            auto& source = Preprocessor.getSourceManager();
            Data.AddFile(source.getFilename(location).str());
        }
        void MacroExpands(const clang::Token& token, const clang::MacroDefinition&,
                          clang::SourceRange range, const clang::MacroArgs*) override {
            const auto* identifier = token.getIdentifierInfo();
            if (!identifier) return;
            MarkerKind kind;
            const auto name = identifier->getName();
            if (name == "HE_REFLECT_COMPONENT") kind = MarkerKind::Component;
            else if (name == "HE_REFLECT_FIELD") kind = MarkerKind::Field;
            else if (name == "HE_REFLECT_ENUM") kind = MarkerKind::Enum;
            else return;
            auto& source = Preprocessor.getSourceManager();
            const auto begin = source.getExpansionLoc(range.getBegin());
            const auto end = clang::Lexer::getLocForEndOfToken(source.getExpansionLoc(range.getEnd()), 0,
                source, Preprocessor.getLangOpts());
            if (begin.isInvalid() || end.isInvalid()) return;
            const auto first = source.getDecomposedLoc(begin);
            const auto last = source.getDecomposedLoc(end);
            if (first.first != last.first || first.second > last.second) return;
            const auto buffer = source.getBufferData(first.first);
            const auto presumed = source.getPresumedLoc(begin);
            if (!presumed.isValid()) return;
            Data.AddFile(presumed.getFilename());
            Data.Markers.push_back({kind, first.first, last.second,
                std::string(buffer.slice(first.second, last.second)),
                SourceName(presumed.getFilename(), Data.Options), presumed.getLine(), presumed.getColumn()});
        }
    private:
        clang::Preprocessor& Preprocessor;
        State& Data;
    };

    class Visitor final : public clang::RecursiveASTVisitor<Visitor> {
    public:
        Visitor(clang::ASTContext& context, State& state) : Context(context), Data(state) {}

        bool VisitCXXRecordDecl(clang::CXXRecordDecl* declaration) {
            if (!declaration->isThisDeclarationADefinition() || declaration->isImplicit() || !declaration->getIdentifier()) return true;
            const auto qualified = declaration->getQualifiedNameAsString();
            auto* marker = Associate(*declaration, MarkerKind::Component);
            if (!marker) return true;
            const auto meta = Metadata(marker->Raw);
            auto guid = MetadataValue(meta, "Guid");
            guid.erase(std::remove(guid.begin(), guid.end(), '-'), guid.end());
            std::transform(guid.begin(), guid.end(), guid.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (!ValidGuid(guid) || guid == std::string(32, '0')) {
                Data.Error("component.invalid_guid", "HE_REFLECT_COMPONENT requires a nonzero 128-bit Guid literal", marker->Source, marker->Line, marker->Column);
            } else if (const auto found = Data.GuidOwners.find(guid); found != Data.GuidOwners.end() && found->second != qualified) {
                Data.Error("component.duplicate_guid", "Duplicate component Guid: " + guid, marker->Source, marker->Line, marker->Column);
            } else Data.GuidOwners.emplace(guid, qualified);
            const auto display = MetadataValue(meta, "DisplayName");
            const auto category = MetadataValue(meta, "Category");
            const auto typeName = MetadataValue(meta, "TypeName");
            if (display.empty()) Data.Error("component.missing_display_name", "Component DisplayName is required", marker->Source, marker->Line, marker->Column);
            if (category.empty()) Data.Error("component.missing_category", "Component Category is required", marker->Source, marker->Line, marker->Column);
            if (!Data.InEntry(Context.getSourceManager(), declaration->getLocation())) return true;
            llvm::json::Array fields;
            for (const auto* field : declaration->fields()) {
                auto* fieldMarker = Associate(*field, MarkerKind::Field);
                if (!fieldMarker) continue;
                const auto fieldMeta = Metadata(fieldMarker->Raw);
                std::string type = SpelledType(*field);
                if (type.empty()) type = field->getType().getAsString();
                std::string enumType;
                if (const auto* enumeration = field->getType().getCanonicalType()->getAs<clang::EnumType>()) {
                    enumType = enumeration->getDecl()->getQualifiedNameAsString();
                }
                std::string runtimeType = type;
                const auto canonical = field->getType().getCanonicalType();
                if (canonical->isBooleanType()) runtimeType = "bool";
                else if (canonical->isSpecificBuiltinType(clang::BuiltinType::Int)) runtimeType = "int";
                else if (canonical->isSpecificBuiltinType(clang::BuiltinType::UInt)) runtimeType = "unsigned int";
                else if (canonical->isSpecificBuiltinType(clang::BuiltinType::Float)) runtimeType = "float";
                else if (canonical->isSpecificBuiltinType(clang::BuiltinType::Double)) runtimeType = "double";
                else if (const auto* record = canonical->getAsCXXRecordDecl()) {
                    if (const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
                        const auto templateName = specialization->getSpecializedTemplate()->getQualifiedNameAsString();
                        const auto& arguments = specialization->getTemplateArgs();
                        if (templateName == "std::basic_string" && arguments.size() >= 1 &&
                            arguments[0].getKind() == clang::TemplateArgument::Type &&
                            arguments[0].getAsType()->isCharType()) runtimeType = "std::string";
                        if (templateName == "glm::vec" && arguments.size() >= 2 &&
                            arguments[0].getKind() == clang::TemplateArgument::Integral &&
                            arguments[1].getKind() == clang::TemplateArgument::Type &&
                            arguments[1].getAsType()->isSpecificBuiltinType(clang::BuiltinType::Float)) {
                            const auto size = arguments[0].getAsIntegral().getSExtValue();
                            if (size >= 2 && size <= 4) runtimeType = "glm::vec" + std::to_string(size);
                        }
                    }
                }
                fields.push_back(llvm::json::Object{
                    {"name", field->getNameAsString()}, {"type", std::move(type)},
                    {"canonical_type", field->getType().getCanonicalType().getAsString()},
                    {"enum_type", std::move(enumType)},
                    {"runtime_type", std::move(runtimeType)},
                    {"source", fieldMarker->Source}, {"line", fieldMarker->Line}, {"column", fieldMarker->Column},
                    {"display_name", MetadataValue(fieldMeta, "DisplayName")},
                    {"category", MetadataValue(fieldMeta, "Category")},
                    {"read_only", MetadataValue(fieldMeta, "ReadOnly") == "true"},
                });
            }
            const auto tag = MetadataValue(meta, "Tag") == "true";
            Data.Types.push_back(llvm::json::Object{
                {"name", declaration->getNameAsString()}, {"qualified_name", qualified}, {"kind", "component"},
                {"declaration_kind", declaration->isStruct() ? "struct" : "class"},
                {"source", marker->Source}, {"line", marker->Line}, {"column", marker->Column},
                {"guid", guid}, {"type_name", typeName.empty() ? declaration->getNameAsString() : typeName},
                {"display_name", display}, {"category", category}, {"tag", tag},
                {"fields", std::move(fields)},
            });
            return true;
        }

        bool VisitEnumDecl(clang::EnumDecl* declaration) {
            if (!declaration->isCompleteDefinition() || !declaration->getIdentifier()) return true;
            const auto qualified = declaration->getQualifiedNameAsString();
            auto* marker = Associate(*declaration, MarkerKind::Enum);
            if (!marker || !Data.InEntry(Context.getSourceManager(), declaration->getLocation())) return true;
            const auto meta = Metadata(marker->Raw);
            llvm::json::Array values;
            for (const auto* value : declaration->enumerators()) {
                values.push_back(llvm::json::Object{
                    {"name", value->getNameAsString()}, {"value", value->getInitVal().getSExtValue()},
                    {"display_name", ""},
                });
            }
            Data.Enums.push_back(llvm::json::Object{
                {"name", declaration->getNameAsString()}, {"qualified_name", qualified},
                {"underlying_type", declaration->getIntegerType().getAsString()},
                {"source", marker->Source}, {"line", marker->Line}, {"column", marker->Column},
                {"display_name", MetadataValue(meta, "DisplayName")}, {"values", std::move(values)},
            });
            return true;
        }

    private:

        Marker* Associate(const clang::Decl& declaration, MarkerKind kind) {
            auto& source = Context.getSourceManager();
            const auto location = source.getExpansionLoc(declaration.getBeginLoc());
            if (location.isInvalid()) return nullptr;
            const auto begin = source.getDecomposedLoc(location);
            const auto buffer = source.getBufferData(begin.first);
            Marker* closest = nullptr;
            for (auto& marker : Data.Markers) {
                if (marker.Used || marker.Kind != kind || marker.File != begin.first || marker.End > begin.second) continue;
                if (!OnlyTrivia(buffer.slice(marker.End, begin.second))) continue;
                if (!closest || marker.End > closest->End) closest = &marker;
            }
            if (closest) closest->Used = true;
            return closest;
        }

        std::string SpelledType(const clang::FieldDecl& field) const {
            const auto* info = field.getTypeSourceInfo();
            if (!info) return {};
            const auto text = clang::Lexer::getSourceText(clang::CharSourceRange::getTokenRange(info->getTypeLoc().getSourceRange()),
                Context.getSourceManager(), Context.getLangOpts());
            return Trim(text.str());
        }

        clang::ASTContext& Context;
        State& Data;
    };

    class Consumer final : public clang::ASTConsumer {
    public:
        explicit Consumer(State& state) : Data(state) {}
        void HandleTranslationUnit(clang::ASTContext& context) override {
            Visitor visitor(context, Data);
            visitor.TraverseDecl(context.getTranslationUnitDecl());
        }
    private:
        State& Data;
    };

    class DiagnosticCapture final : public clang::DiagnosticConsumer {
    public:
        DiagnosticCapture(State& state, clang::SourceManager& source) : Data(state), Source(source) {}
        void HandleDiagnostic(clang::DiagnosticsEngine::Level level, const clang::Diagnostic& diagnostic) override {
            clang::DiagnosticConsumer::HandleDiagnostic(level, diagnostic);
            if (level != clang::DiagnosticsEngine::Error && level != clang::DiagnosticsEngine::Fatal) return;
            llvm::SmallString<256> message;
            diagnostic.FormatDiagnostic(message);
            const auto presumed = Source.getPresumedLoc(diagnostic.getLocation());
            const std::string filename = presumed.isValid() ? SourceName(presumed.getFilename(), Data.Options) : std::string{};
            const unsigned line = presumed.isValid() ? presumed.getLine() : 0;
            const unsigned column = presumed.isValid() ? presumed.getColumn() : 0;
            Data.Error("clang.error", std::string(message.str()), filename, line, column);
            llvm::errs() << filename << ':' << line << ':' << column << ": error: " << message << '\n';
        }
    private:
        State& Data;
        clang::SourceManager& Source;
    };

    class Action final : public clang::ASTFrontendAction {
    public:
        explicit Action(State& state) : Data(state) {}
        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler, llvm::StringRef) override {
            compiler.getDiagnostics().setClient(new DiagnosticCapture(Data, compiler.getSourceManager()), true);
            compiler.getPreprocessor().addPPCallbacks(std::make_unique<Markers>(compiler.getPreprocessor(), Data));
            return std::make_unique<Consumer>(Data);
        }
    private:
        State& Data;
    };

    class Factory final : public clang::tooling::FrontendActionFactory {
    public:
        explicit Factory(State& state) : Data(state) {}
        std::unique_ptr<clang::FrontendAction> create() override { return std::make_unique<Action>(Data); }
    private:
        State& Data;
    };

    void SortByName(llvm::json::Array& values) {
        std::sort(values.begin(), values.end(), [](const llvm::json::Value& left, const llvm::json::Value& right) {
            const auto* a = left.getAsObject();
            const auto* b = right.getAsObject();
            return a->getString("qualified_name").value_or("") < b->getString("qualified_name").value_or("");
        });
    }

    std::string EscapeDepfile(std::string value) {
        std::replace(value.begin(), value.end(), '\\', '/');
        std::string result;
        result.reserve(value.size() + 8);
        for (char ch : value) {
            if (ch == ' ' || ch == '#' || ch == '$') result += '\\';
            result += ch;
        }
        return result;
    }

    bool WriteIfChanged(const std::string& filename, std::string_view body) {
        std::ifstream current(Utf8Path(filename), std::ios::binary);
        if (current && std::string(std::istreambuf_iterator<char>{current}, std::istreambuf_iterator<char>{}) == body) return true;
        std::error_code error;
        fs::create_directories(Utf8Path(filename).parent_path(), error);
        if (error) return false;
        std::ofstream output(Utf8Path(filename), std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output.write(body.data(), static_cast<std::streamsize>(body.size()));
        return output.good();
    }
}

int Scan(const clang::tooling::CompilationDatabase& database,
         const std::vector<std::string>& sourcePaths, const ScanOptions& options) {
    if (sourcePaths.size() != 1 || options.Module.empty() || options.Configuration.empty() ||
        options.EntryHeader.empty() || options.RepositoryRoot.empty()) {
        llvm::errs() << "HuaMeta requires one scan translation unit and a complete module configuration.\n";
        return 2;
    }
    State state(options);
    state.AddFile(sourcePaths.front());
    state.AddFile(options.EntryHeader);
    const auto commands = database.getCompileCommands(sourcePaths.front());
    if (commands.size() != 1) {
        llvm::errs() << "HuaMeta requires exactly one compile command for the scan translation unit.\n";
        return 2;
    }
    std::string fingerprint = options.Module + "\n" + options.Configuration + "\n" + options.EntryHeader + "\n" + std::string(ToolVersion);
    for (const auto& argument : commands.front().CommandLine) fingerprint += "\n" + argument;
    clang::tooling::ClangTool tool(database, sourcePaths);
    const std::string resourceArgument = "-resource-dir=" + options.ResourceDirectory;
    tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
        resourceArgument.c_str(), clang::tooling::ArgumentInsertPosition::BEGIN));
    Factory factory(state);
    const int parseExit = tool.run(&factory);
    if (parseExit != 0 && !state.Failed) {
        state.Error("clang.parse_failed", "Clang failed to parse the module translation unit", SourceName(sourcePaths.front(), options));
    }
    for (const auto& marker : state.Markers) {
        if (marker.Used || Identity(Utf8Path(options.RepositoryRoot) / Utf8Path(marker.Source)) != state.Entry) continue;
        state.Error("marker.unbound", "A reflection marker has no matching declaration", marker.Source, marker.Line, marker.Column);
    }
    SortByName(state.Types);
    SortByName(state.Enums);
    llvm::json::Array dependencies;
    for (const auto& file : state.Files) {
        if (const auto hash = FileHash(file)) {
            dependencies.push_back(llvm::json::Object{{"path", SourceName(file, options)}, {"sha256", *hash}});
        }
    }
    llvm::json::Object report{
        {"schema_version", 2},
        {"producer", llvm::json::Object{{"name", "HuaMeta"}, {"version", std::string(ToolVersion)}}},
        {"module", options.Module}, {"configuration", options.Configuration},
        {"translation_unit", SourceName(sourcePaths.front(), options)},
        {"compile_fingerprint", Hash(fingerprint)}, {"dependencies", std::move(dependencies)},
        {"types", std::move(state.Types)}, {"enums", std::move(state.Enums)},
        {"queries", llvm::json::Array{}}, {"diagnostics", std::move(state.Diagnostics)},
    };
    const std::string output = llvm::formatv("{0:2}\n", llvm::json::Value(std::move(report))).str();
    if (!WriteIfChanged(options.Output, output)) {
        llvm::errs() << "HuaMeta failed to write manifest: " << options.Output << '\n';
        return 3;
    }
    if (!options.Depfile.empty() && parseExit == 0 && !state.Failed) {
        std::string depfile = EscapeDepfile(options.Output) + ':';
        for (const auto& file : state.Files) depfile += " " + EscapeDepfile(file);
        depfile += '\n';
        if (!WriteIfChanged(options.Depfile, depfile)) {
            llvm::errs() << "HuaMeta failed to write dependency file: " << options.Depfile << '\n';
            return 3;
        }
    }
    if (state.Failed) {
        llvm::errs() << "HuaMeta rejected the module; inspect manifest diagnostics.\n";
        return 1;
    }
    llvm::outs() << "HuaMeta produced manifest v2 for " << options.Module << "/" << options.Configuration << ".\n";
    return 0;
}
}
