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
#include "clang/AST/Attr.h"
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
    constexpr std::string_view ToolVersion = "23.1.2-p10";
    constexpr std::string_view SattrPrefix = "hua.sattr:";

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

    std::string MetadataValue(const std::map<std::string, std::string>& values, std::string_view key) {
        auto found = values.find(std::string(key));
        return found == values.end() ? std::string{} : found->second;
    }

    bool ValidGuid(std::string_view value) {
        if (value.size() != 32) return false;
        return std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isxdigit(ch); });
    }

    bool Identifier(std::string_view value) {
        if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value.front())) || value.front() == '_')) return false;
        return std::all_of(value.begin() + 1, value.end(), [](unsigned char ch) { return std::isalnum(ch) || ch == '_'; });
    }

    bool AttributeName(std::string_view value) {
        while (!value.empty()) {
            const auto dot = value.find('.');
            if (!Identifier(value.substr(0, dot))) return false;
            if (dot == std::string_view::npos) return true;
            value.remove_prefix(dot + 1);
        }
        return false;
    }

    std::optional<std::vector<std::string>> SplitTopLevel(std::string_view input, char delimiter) {
        std::vector<std::string> result;
        size_t begin = 0;
        unsigned brackets = 0;
        bool quoted = false;
        bool escaped = false;
        for (size_t index = 0; index < input.size(); ++index) {
            const char ch = input[index];
            if (quoted) {
                if (escaped) escaped = false;
                else if (ch == '\\') escaped = true;
                else if (ch == '"') quoted = false;
                continue;
            }
            if (ch == '"') quoted = true;
            else if (ch == '[') ++brackets;
            else if (ch == ']') {
                if (brackets == 0) return std::nullopt;
                --brackets;
            } else if (ch == delimiter && brackets == 0) {
                result.push_back(Trim(std::string(input.substr(begin, index - begin))));
                begin = index + 1;
            }
        }
        if (quoted || brackets != 0) return std::nullopt;
        result.push_back(Trim(std::string(input.substr(begin))));
        return result;
    }

    std::optional<std::string> QuotedValue(std::string_view input) {
        if (input.size() < 2 || input.front() != '"' || input.back() != '"') return std::nullopt;
        std::string result;
        for (size_t index = 1; index + 1 < input.size(); ++index) {
            char ch = input[index];
            if (ch == '\\') {
                if (++index + 1 >= input.size()) return std::nullopt;
                ch = input[index];
                if (ch == 'n') ch = '\n';
                else if (ch == 't') ch = '\t';
                else if (ch != '"' && ch != '\\') return std::nullopt;
            } else if (ch == '"') return std::nullopt;
            result += ch;
        }
        return result;
    }

    std::optional<std::vector<std::string>> StringList(std::string_view input) {
        if (input.size() < 2 || input.front() != '[' || input.back() != ']') return std::nullopt;
        const auto content = input.substr(1, input.size() - 2);
        if (Trim(std::string(content)).empty()) return std::vector<std::string>{};
        const auto parts = SplitTopLevel(content, ',');
        if (!parts) return std::nullopt;
        std::vector<std::string> result;
        for (const auto& part : *parts) {
            const auto value = QuotedValue(part);
            if (!value) return std::nullopt;
            result.push_back(*value);
        }
        return result;
    }

    struct ReflectedMetadata {
        std::string Guid;
        std::string Scope;
        std::vector<std::string> Flags;
        std::map<std::string, std::string> Attributes;
    };

    bool HasFlag(const ReflectedMetadata& metadata, std::string_view flag) {
        return std::find(metadata.Flags.begin(), metadata.Flags.end(), flag) != metadata.Flags.end();
    }

    std::string AttributeValue(const ReflectedMetadata& metadata, std::string_view name) {
        return MetadataValue(metadata.Attributes, name);
    }

    llvm::json::Array FlagsJson(const ReflectedMetadata& metadata) {
        llvm::json::Array result;
        for (const auto& flag : metadata.Flags) result.push_back(flag);
        return result;
    }

    llvm::json::Array AttributesJson(const ReflectedMetadata& metadata) {
        llvm::json::Array result;
        for (const auto& [name, value] : metadata.Attributes) {
            result.push_back(llvm::json::Object{{"name", name}, {"value", value}});
        }
        return result;
    }

    std::optional<ReflectedMetadata> ParseSattr(std::string_view input, std::string& error) {
        ReflectedMetadata result;
        const auto clauses = SplitTopLevel(input, ';');
        if (!clauses) { error = "Malformed sattr clauses"; return std::nullopt; }
        if (clauses->size() == 1 && clauses->front().empty()) return result;
        std::set<std::string> seen;
        for (const auto& clause : *clauses) {
            const auto equal = clause.find('=');
            if (equal == std::string::npos) { error = "sattr clauses require key=value"; return std::nullopt; }
            const auto key = Trim(clause.substr(0, equal));
            const auto value = Trim(clause.substr(equal + 1));
            if (!seen.insert(key).second) { error = "Duplicate sattr clause: " + key; return std::nullopt; }
            if (key == "guid") {
                const auto guid = QuotedValue(value);
                if (!guid) { error = "sattr guid requires a quoted string"; return std::nullopt; }
                result.Guid = *guid;
            } else if (key == "reflect") {
                if (value == "@marked") result.Scope = "marked";
                else if (value == "@full") result.Scope = "full";
                else if (value == "@disable") result.Scope = "disable";
                else { error = "sattr reflect requires @marked, @full or @disable"; return std::nullopt; }
            } else if (key == "flags" || key == "attrs") {
                const auto values = StringList(value);
                if (!values) { error = "sattr " + key + " requires a list of quoted strings"; return std::nullopt; }
                for (const auto& item : *values) {
                    if (key == "flags") {
                        if (!Identifier(item) || std::find(result.Flags.begin(), result.Flags.end(), item) != result.Flags.end()) {
                            error = "Invalid or duplicate sattr flag: " + item; return std::nullopt;
                        }
                        result.Flags.push_back(item);
                    } else {
                        const auto itemEqual = item.find('=');
                        if (itemEqual == std::string::npos || !AttributeName(std::string_view(item).substr(0, itemEqual)) ||
                            !result.Attributes.emplace(item.substr(0, itemEqual), item.substr(itemEqual + 1)).second) {
                            error = "Invalid or duplicate sattr attribute: " + item; return std::nullopt;
                        }
                    }
                }
            } else { error = "Unknown sattr clause: " + key; return std::nullopt; }
        }
        return result;
    }

    bool NormalizeGuid(std::string& guid) {
        if (guid.size() != 32 && guid.size() != 36) return false;
        if (guid.size() == 36 &&
            (guid[8] != '-' || guid[13] != '-' || guid[18] != '-' || guid[23] != '-')) return false;
        guid.erase(std::remove(guid.begin(), guid.end(), '-'), guid.end());
        std::transform(guid.begin(), guid.end(), guid.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return ValidGuid(guid) && guid != std::string(32, '0');
    }

    struct MarkedDeclaration {
        ReflectedMetadata Metadata;
        std::string Source;
        unsigned Line = 0;
        unsigned Column = 0;
    };

    struct State {
        explicit State(const ScanOptions& options) : Options(options), Entry(Identity(Utf8Path(options.EntryHeader))) {}
        const ScanOptions& Options;
        std::string Entry;
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

    class DependencyFiles final : public clang::PPCallbacks {
    public:
        DependencyFiles(clang::Preprocessor& preprocessor, State& state) : Preprocessor(preprocessor), Data(state) {}
        void FileChanged(clang::SourceLocation location, FileChangeReason reason,
                         clang::SrcMgr::CharacteristicKind, clang::FileID) override {
            if (reason != EnterFile || location.isInvalid()) return;
            auto& source = Preprocessor.getSourceManager();
            Data.AddFile(source.getFilename(location).str());
        }
    private:
        clang::Preprocessor& Preprocessor;
        State& Data;
    };

    class Visitor final : public clang::RecursiveASTVisitor<Visitor> {
    public:
        Visitor(clang::ASTContext& context, State& state) : Context(context), Data(state) {}

        bool VisitClassTemplateDecl(clang::ClassTemplateDecl* declaration) {
            if (const auto marker = Marked(*declaration)) {
                Data.Error("type.unsupported_declaration", "Reflection requires a non-template struct or class definition",
                    marker->Source, marker->Line, marker->Column);
            }
            return true;
        }

        bool VisitEmptyDecl(clang::EmptyDecl* declaration) {
            if (const auto marker = Marked(*declaration)) {
                Data.Error("sattr.unbound", "sattr cannot annotate an empty declaration",
                    marker->Source, marker->Line, marker->Column);
            }
            return true;
        }

        bool VisitCXXRecordDecl(clang::CXXRecordDecl* declaration) {
            if (!declaration->isThisDeclarationADefinition() || declaration->isImplicit() || !declaration->getIdentifier()) return true;
            const auto qualified = declaration->getQualifiedNameAsString();
            auto marker = Marked(*declaration);
            if (!marker) return true;
            const bool component = HasFlag(marker->Metadata, "Component");
            if (declaration->isUnion() || declaration->getDescribedClassTemplate() ||
                llvm::isa<clang::ClassTemplateSpecializationDecl>(declaration) || declaration->isDependentContext()) {
                Data.Error("type.unsupported_declaration", "Reflection requires a non-template struct or class definition",
                    marker->Source, marker->Line, marker->Column);
                return true;
            }
            std::string guid = marker->Metadata.Guid;
            if (!NormalizeGuid(guid)) {
                Data.Error(component ? "component.invalid_guid" : "type.invalid_guid",
                    "Reflected declaration requires a nonzero 128-bit Guid literal", marker->Source, marker->Line, marker->Column);
            } else if (const auto found = Data.GuidOwners.find(guid); found != Data.GuidOwners.end() && found->second != qualified) {
                Data.Error(component ? "component.duplicate_guid" : "type.duplicate_guid",
                    "Duplicate reflected Guid: " + guid, marker->Source, marker->Line, marker->Column);
            } else Data.GuidOwners.emplace(guid, qualified);
            if (marker->Metadata.Scope.empty()) {
                Data.Error("sattr.missing_reflect", "Reflected declaration requires a reflect scope", marker->Source, marker->Line, marker->Column);
            }
            if (HasFlag(marker->Metadata, "Tag") && !component) {
                Data.Error("sattr.invalid_flags", "Tag requires Component", marker->Source, marker->Line, marker->Column);
            }
            if (component && HasFlag(marker->Metadata, "Tag") &&
                (!declaration->field_empty() || declaration->getNumBases() != 0)) {
                Data.Error("component.nonempty_tag", "Tag components cannot contain stored fields or bases",
                    marker->Source, marker->Line, marker->Column);
            }
            auto display = AttributeValue(marker->Metadata, "DisplayName");
            const auto category = AttributeValue(marker->Metadata, "Category");
            if (component && display.empty()) Data.Error("component.missing_display_name", "Component DisplayName is required",
                marker->Source, marker->Line, marker->Column);
            if (!component && display.empty()) display = declaration->getNameAsString();
            if (component && category.empty()) Data.Error("component.missing_category", "Component Category is required",
                marker->Source, marker->Line, marker->Column);
            if (!Data.InEntry(Context.getSourceManager(), declaration->getLocation())) return true;
            llvm::json::Array fields;
            if (marker->Metadata.Scope != "disable") for (const auto* field : declaration->fields()) {
                auto fieldMarker = Marked(*field);
                if (!fieldMarker && marker->Metadata.Scope != "full") continue;
                if (!fieldMarker) fieldMarker = SourceOf(*field);
                if (field->getAccess() != clang::AS_public) {
                    Data.Error("field.inaccessible", "Reflected fields must be public",
                        fieldMarker->Source, fieldMarker->Line, fieldMarker->Column);
                    continue;
                }
                if (field->isBitField()) {
                    Data.Error("field.bit_field", "Bit-fields cannot be reflected",
                        fieldMarker->Source, fieldMarker->Line, fieldMarker->Column);
                    continue;
                }
                std::string type = SpelledType(*field);
                if (type.empty()) type = field->getType().getAsString();
                std::string enumType;
                llvm::json::Object enumMetadata;
                if (const auto* enumeration = field->getType().getCanonicalType()->getAs<clang::EnumType>()) {
                    enumType = enumeration->getDecl()->getQualifiedNameAsString();
                    if (const auto* definition = enumeration->getDecl()->getDefinition()) {
                        if (const auto enumMarker = Marked(*definition)) {
                            ValidateEnum(*definition, *enumMarker);
                            enumMetadata = DescribeEnum(*definition, *enumMarker);
                        }
                    }
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
                    {"enum_type", std::move(enumType)}, {"enum_metadata", std::move(enumMetadata)},
                    {"runtime_type", std::move(runtimeType)},
                    {"source", fieldMarker->Source}, {"line", fieldMarker->Line}, {"column", fieldMarker->Column},
                    {"display_name", AttributeValue(fieldMarker->Metadata, "DisplayName")},
                    {"category", AttributeValue(fieldMarker->Metadata, "Category")},
                    {"read_only", HasFlag(fieldMarker->Metadata, "ReadOnly")},
                    {"flags", FlagsJson(fieldMarker->Metadata)}, {"attributes", AttributesJson(fieldMarker->Metadata)},
                });
            }
            llvm::json::Object reflectedType{
                {"name", declaration->getNameAsString()}, {"qualified_name", qualified}, {"kind", component ? "component" : "type"},
                {"declaration_kind", declaration->isStruct() ? "struct" : "class"},
                {"source", marker->Source}, {"line", marker->Line}, {"column", marker->Column},
                {"display_name", display}, {"category", category},
                {"reflection_scope", marker->Metadata.Scope},
                {"flags", FlagsJson(marker->Metadata)}, {"attributes", AttributesJson(marker->Metadata)},
                {"fields", std::move(fields)},
            };
            reflectedType["guid"] = guid;
            if (component) {
                const auto typeName = AttributeValue(marker->Metadata, "TypeName");
                reflectedType["type_name"] = typeName.empty() ? declaration->getNameAsString() : typeName;
                reflectedType["tag"] = HasFlag(marker->Metadata, "Tag");
            }
            Data.Types.push_back(std::move(reflectedType));
            return true;
        }

        bool VisitEnumDecl(clang::EnumDecl* declaration) {
            if (!declaration->isCompleteDefinition() || !declaration->getIdentifier()) return true;
            auto marker = Marked(*declaration);
            if (!marker || !Data.InEntry(Context.getSourceManager(), declaration->getLocation())) return true;
            ValidateEnum(*declaration, *marker);
            Data.Enums.push_back(DescribeEnum(*declaration, *marker));
            return true;
        }

    private:

        void ValidateEnum(const clang::EnumDecl& declaration, const MarkedDeclaration& marker) {
            if (!ValidatedEnums.insert(&declaration).second) return;
            std::string guid = marker.Metadata.Guid;
            if (!NormalizeGuid(guid)) {
                Data.Error("enum.invalid_guid", "Reflected enum requires a nonzero 128-bit Guid literal",
                    marker.Source, marker.Line, marker.Column);
            } else if (const auto found = Data.GuidOwners.find(guid);
                found != Data.GuidOwners.end() && found->second != declaration.getQualifiedNameAsString()) {
                Data.Error("enum.duplicate_guid", "Duplicate reflected Guid: " + guid,
                    marker.Source, marker.Line, marker.Column);
            } else Data.GuidOwners.emplace(guid, declaration.getQualifiedNameAsString());
            if (marker.Metadata.Scope.empty()) {
                Data.Error("sattr.missing_reflect", "Reflected enum requires a reflect scope",
                    marker.Source, marker.Line, marker.Column);
            }
            if (HasFlag(marker.Metadata, "Component") || HasFlag(marker.Metadata, "Tag")) {
                Data.Error("sattr.invalid_flags", "Enums cannot be components or tags",
                    marker.Source, marker.Line, marker.Column);
            }
        }

        llvm::json::Object DescribeEnum(const clang::EnumDecl& declaration, const MarkedDeclaration& marker) {
            llvm::json::Array values;
            if (marker.Metadata.Scope != "disable") for (const auto* value : declaration.enumerators()) {
                auto valueMarker = Marked(*value);
                if (!valueMarker && marker.Metadata.Scope != "full") continue;
                if (!valueMarker) valueMarker = SourceOf(*value);
                values.push_back(llvm::json::Object{
                    {"name", value->getNameAsString()}, {"value", value->getInitVal().getSExtValue()},
                    {"display_name", AttributeValue(valueMarker->Metadata, "DisplayName")},
                    {"flags", FlagsJson(valueMarker->Metadata)}, {"attributes", AttributesJson(valueMarker->Metadata)},
                });
            }
            llvm::json::Object result{
                {"name", declaration.getNameAsString()}, {"qualified_name", declaration.getQualifiedNameAsString()},
                {"underlying_type", declaration.getIntegerType().getAsString()},
                {"source", marker.Source}, {"line", marker.Line}, {"column", marker.Column},
                {"display_name", AttributeValue(marker.Metadata, "DisplayName")},
                {"reflection_scope", marker.Metadata.Scope},
                {"flags", FlagsJson(marker.Metadata)}, {"attributes", AttributesJson(marker.Metadata)},
                {"values", std::move(values)},
            };
            std::string guid = marker.Metadata.Guid;
            if (NormalizeGuid(guid)) result["guid"] = std::move(guid);
            return result;
        }

        MarkedDeclaration SourceAt(clang::SourceLocation location) const {
            const auto& source = Context.getSourceManager();
            const auto presumed = source.getPresumedLoc(source.getExpansionLoc(location));
            if (!presumed.isValid()) return {};
            return {{}, SourceName(presumed.getFilename(), Data.Options), presumed.getLine(), presumed.getColumn()};
        }

        MarkedDeclaration SourceOf(const clang::Decl& declaration) const {
            return SourceAt(declaration.getLocation());
        }

        std::optional<MarkedDeclaration> Marked(const clang::Decl& declaration) {
            std::optional<MarkedDeclaration> result;
            for (const auto* attribute : declaration.specific_attrs<clang::AnnotateAttr>()) {
                const llvm::StringRef annotation = attribute->getAnnotation();
                if (!annotation.starts_with(SattrPrefix)) continue;
                auto location = SourceAt(attribute->getLocation());
                if (result) {
                    Data.Error("sattr.duplicate", "A declaration has multiple sattr annotations",
                        location.Source, location.Line, location.Column);
                    continue;
                }
                std::string error;
                const auto metadata = ParseSattr(annotation.drop_front(SattrPrefix.size()).str(), error);
                if (!metadata) {
                    Data.Error("sattr.invalid", error, location.Source, location.Line, location.Column);
                    continue;
                }
                location.Metadata = *metadata;
                result = std::move(location);
            }
            return result;
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
        std::set<const clang::EnumDecl*> ValidatedEnums;
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
            compiler.getPreprocessor().addPPCallbacks(std::make_unique<DependencyFiles>(compiler.getPreprocessor(), Data));
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
    tool.appendArgumentsAdjuster(clang::tooling::getInsertArgumentAdjuster(
        "-DHE_META_SCANNING=1", clang::tooling::ArgumentInsertPosition::BEGIN));
    Factory factory(state);
    const int parseExit = tool.run(&factory);
    if (parseExit != 0 && !state.Failed) {
        state.Error("clang.parse_failed", "Clang failed to parse the module translation unit", SourceName(sourcePaths.front(), options));
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
