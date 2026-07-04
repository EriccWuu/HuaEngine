#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace HE {
class ComponentRegistry;
} // namespace HE

namespace HE::Generated {

struct ReflectedFieldInfo {
    std::string_view Name;
    std::string_view Type;
};

struct ReflectedTypeInfo {
    std::string_view Name;
    std::string_view QualifiedName;
    std::string_view Kind;
    std::string_view DisplayName;
    std::string_view Category;
    std::span<const ReflectedFieldInfo> Fields;
};

struct ReflectedEnumValueInfo {
    std::string_view Name;
    int64_t Value;
    std::string_view DisplayName;
};

struct ReflectedEnumInfo {
    std::string_view Name;
    std::string_view QualifiedName;
    std::string_view UnderlyingType;
    std::span<const ReflectedEnumValueInfo> Values;
};

std::span<const ReflectedTypeInfo> GetReflectedTypes();
const ReflectedTypeInfo* FindReflectedType(std::string_view qualifiedName);
std::span<const ReflectedEnumInfo> GetReflectedEnums();
const ReflectedEnumInfo* FindReflectedEnum(std::string_view qualifiedName);
void RegisterGeneratedComponents(ComponentRegistry& registry);

} // namespace HE::Generated
