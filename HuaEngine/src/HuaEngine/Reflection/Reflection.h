#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "HuaEngine/ECS/ComponentType.h"
#include "HuaEngine/ECS/EntityId.h"

#include "TypeList.h"
#include "FieldTraits.h"
#include "misc.h"
#include "ConstStr.h"

namespace HE {
class World;

namespace Serialization {
    class SerializationBackend;
}

namespace Refl {
// --------------------------------------------------------------------------------
//                              Runtime descriptors
// --------------------------------------------------------------------------------

enum class RuntimeFieldFlags : uint32_t {
    None = 0,
    Serializable = 1u << 0,
    Editable = 1u << 1,
    ReadOnly = 1u << 2,
    ComponentField = 1u << 3,
};

inline constexpr RuntimeFieldFlags operator|(RuntimeFieldFlags lhs, RuntimeFieldFlags rhs) {
    return static_cast<RuntimeFieldFlags>(
        static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline constexpr RuntimeFieldFlags operator&(RuntimeFieldFlags lhs, RuntimeFieldFlags rhs) {
    return static_cast<RuntimeFieldFlags>(
        static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline constexpr bool HasRuntimeFieldFlag(RuntimeFieldFlags flags, RuntimeFieldFlags flag) {
    return static_cast<uint32_t>(flags & flag) != 0;
}

struct RuntimeFieldDescriptor {
    std::string_view Name;
    std::string_view Type;
    std::string_view DisplayName;
    std::string_view Category;
    size_t Offset;
    size_t Size;
    RuntimeFieldFlags Flags;
    const void* (*GetConst)(const void*);
    void* (*GetMutable)(void*);
    void (*Serialize)(Serialization::SerializationBackend&, const std::string&, const void*);
    bool (*Deserialize)(Serialization::SerializationBackend&, const std::string&, void*);
};

struct RuntimeTypeDescriptor {
    std::string_view Name;
    std::string_view QualifiedName;
    std::string_view Kind;
    std::string_view DisplayName;
    std::string_view Category;
    ComponentTypeId TypeId;
    size_t Size;
    std::span<const RuntimeFieldDescriptor> Fields;
    void* (*ConstructDefault)();
    void (*Destroy)(void*);
    void* (*Copy)(const void*);
    void (*Serialize)(Serialization::SerializationBackend&, const std::string&, const void*);
    bool (*Deserialize)(Serialization::SerializationBackend&, const std::string&, void*);
    void (*AddCopyToWorld)(World&, EntityId, const void*);
};

std::span<const RuntimeTypeDescriptor> GetRuntimeTypes();
const RuntimeTypeDescriptor* FindRuntimeType(std::string_view qualifiedName);
const RuntimeTypeDescriptor* FindRuntimeType(ComponentTypeId typeId);
void SerializeRuntimeObject(
    const RuntimeTypeDescriptor& type,
    Serialization::SerializationBackend& backend,
    const std::string& name,
    const void* object);
bool DeserializeRuntimeObject(
    const RuntimeTypeDescriptor& type,
    Serialization::SerializationBackend& backend,
    const std::string& name,
    void* object);

// --------------------------------------------------------------------------------
//                                    srefl
// --------------------------------------------------------------------------------

/**
    * @brief attributes that attach to field/function
    */
template <typename... Attrs>
using attr_list = type_list<Attrs...>;

namespace Detail {

    template <typename T, bool>
    struct basic_field_traits;

    template <typename T>
    struct basic_field_traits<T, true> : function_traits<T> {
        constexpr bool is_const_member() const noexcept {
            return function_traits<T>::is_const;
        }

        constexpr bool is_member() const noexcept {
            return function_traits<T>::is_member;
        }

        constexpr bool is_function() const noexcept {
            return true;
        }

        constexpr bool is_variable() const noexcept {
            return false;
        }
    };

    template <typename T>
    struct basic_field_traits<T, false> : variable_traits<T> {
        constexpr bool is_const_member() const noexcept {
            return false;
        }

        constexpr bool is_member() const noexcept {
            return variable_traits<T>::is_member;
        }

        constexpr bool is_function() const noexcept {
            return false;
        }

        constexpr bool is_variable() const noexcept {
            return true;
        }
    };

}  // namespace Detail

/**
    * @brief strip class/function/variable name from namespace/class prefix to pure name
    */
inline constexpr std::string_view strip_name(std::string_view name) {
    std::string_view result = name;

    if (auto idx = name.find_last_of('&'); idx != std::string_view::npos) {
        name = name.substr(idx + 1, name.length());
    }
    if (auto idx = name.find_last_of(':'); idx != std::string_view::npos) {
        name = name.substr(idx + 1, name.length());
    }
    if (auto idx = name.find_first_of(')'); idx != std::string_view::npos) {
        name = name.substr(0, idx);
    }

    return name;
}

/**
    * @brief extract class field(member variable, member function) info
    *
    * @tparam T type
    * @tparam Attrs attributes
    */
template <typename T, typename... Attrs>
struct member_traits : Detail::basic_field_traits<T, is_function_v<T>> {
    constexpr member_traits(T&& pointer, std::string_view name, size_t offset, Attrs&&... attrs)
        : pointer_(std::forward<T>(pointer)),
        name_(name),
        offset_(offset),
        attrs_(std::forward<Attrs>(attrs)...) {}

    /**
        * @brief check whether field is a const member(class const function)
        */
    constexpr bool is_const_member() const noexcept {
        return base::is_const_member();
    }

    /**
        * @brief check whether field is class member or static/global
        */
    constexpr bool is_member() const noexcept {
        return base::is_member();
    }

    /**
        * @brief get field name
        */
    constexpr std::string_view name() const noexcept {
        return name_;
    }

    /**
        * @brief get offset
        */
    constexpr auto& offset() const noexcept {
        return offset_;
    }

    /**
        * @brief get pointer
        */
    constexpr auto pointer() const noexcept {
        return pointer_;
    }

    /**
        * @brief get attributes
        */
    constexpr auto& attrs() const noexcept {
        return attrs_;
    }

    template <typename... Args>
    decltype(auto) invoke(Args&&... args) {
        if constexpr (!is_function_v<T>) {
            if constexpr (variable_traits<T>::is_member) {
                return std::invoke(this->pointer_, std::forward<Args>(args)...);
            }
            else {
                return *(this->pointer_);
            }
        }
        else {
            return std::invoke(this->pointer_, std::forward<Args>(args)...);
        }
    }

private:
    using base = Detail::basic_field_traits<T, is_function_v<T>>;

    T pointer_;
    size_t offset_;
    std::string_view name_;
    std::tuple<Attrs...> attrs_;
};


template <typename T, typename... Attrs>
struct field_traits : variable_traits<T> {
    constexpr field_traits(T&& pointer, std::string_view name, size_t offset, Attrs&&... attrs)
        : pointer_(pointer),
        name_(name),
        offset_(offset),
        attrs_(std::forward<Attrs>(attrs)...) {}

    /**
        * @brief check whether field is a const member(class const function)
        */
    constexpr bool is_const() const noexcept {
        return base::is_const;
    }

    /**
        * @brief get field name
        */
    constexpr std::string_view name() const noexcept {
        return name_;
    }

    /**
        * @brief get offset
        */
    constexpr auto& offset() const noexcept {
        return offset_;
    }

    /**
        * @brief get attributes
        */
    constexpr auto& attrs() const noexcept {
        return attrs_;
    }

    // template <typename... Args>
    // decltype(auto) invoke(Args&&... args) {
    //     return std::invoke(this->pointer_, std::forward<Args>(args)...);
    // }

    template<typename ClassType>
    const auto& GetValue(const ClassType* obj) const {
        return *reinterpret_cast<const base::type*>(reinterpret_cast<const char*>(obj) + offset_);
    }

    template<typename ClassType, typename FieldType>
    void SetValue(ClassType* obj, const FieldType& value) {
        if constexpr (base::is_const)
            return;
        else {
            *reinterpret_cast<FieldType*>(reinterpret_cast<char*>(obj) + offset_) = value;
        }

    }

private:
    using base = variable_traits<T>;

    T pointer_;
    size_t offset_;
    std::string_view name_;
    std::tuple<Attrs...> attrs_;
};

/**
    * @brief store class constructor
    */
template <typename... Args>
struct ctor {
    using args = type_list<Args...>;
};

/**
    * @brief store base classes
    */
template <typename... Bases>
struct base {
    using bases = type_list<Bases...>;
};

template <typename T>
struct base_type_info {
    using type = T;
    static constexpr bool is_final = std::is_final_v<T>;
};

template <typename T>
struct enum_value {
    using value_type = T;
    // using underlying_type = std::underlying_type_t<T>;

    constexpr enum_value(value_type value, std::string_view name)
        : value{ value }, name{ name } {}

    T value;
    std::string_view name;
};

/**
    * @brief store class type info
    *
    * @tparam T type
    * @tparam AttrList attributes
    */
template <typename T>
struct type_info;

// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
//                                    Reflection
// --------------------------------------------------------------------------------

// some type_info traits
namespace Detail {

    template <typename TypeInfo, typename = std::void_t<>>
    struct has_bases final : std::false_type {};

    template <typename TypeInfo>
    struct has_bases<TypeInfo, std::void_t<typename TypeInfo::bases>> {
        static constexpr bool value =
            !is_list_empty_v<typename TypeInfo::bases>;
    };

    template <typename TypeInfo, typename = std::void_t<>>
    struct has_fields final : std::false_type {};

    template <typename TypeInfo>
    struct has_fields<TypeInfo, std::void_t<decltype(TypeInfo::fields)>> {
        static constexpr bool value = !is_list_empty_v<
            std::remove_cv_t<std::remove_const_t<decltype(TypeInfo::fields)>>>;
    };

    template <typename TypeInfo, typename = std::void_t<>>
    struct has_ctors final : std::false_type {};

    template <typename TypeInfo>
    struct has_ctors<TypeInfo, std::void_t<typename TypeInfo::ctors>> {
        static constexpr bool value =
            !is_list_empty_v<typename TypeInfo::ctors>;
    };

}  // namespace Detail

namespace Detail {

    template <size_t... Idx, typename TupleType>
    constexpr auto pick_tuple_elements(TupleType&& tuple,
        std::index_sequence<Idx...>) {
        return std::make_tuple(std::get<Idx>(tuple)...);
    }

    template <size_t... Idx>
    constexpr auto inc_seq_elem(std::index_sequence<Idx...> seq) {
        return std::index_sequence<(Idx + 1)...>{};
    }

}  // namespace Detail

/**
    * @brief get tail of tuple(the elems without first elem)
    */
template <typename TupleType>
constexpr auto tuple_tail(TupleType&& tuple) {
    using tuple_type = remove_cvref_t<TupleType>;

    if constexpr (list_size_v<tuple_type> >= 1) {
        return Detail::pick_tuple_elements(
            std::forward<TupleType>(tuple),
            Detail::inc_seq_elem(
                std::make_index_sequence<list_size_v<tuple_type> -1>{}));
    }
    else {
        return std::tuple<>{};
    }
}

/**
    * @brief check whether a type_info has bases classes
    */
template <typename TypeInfo>
constexpr bool has_bases_v = Detail::has_bases<TypeInfo>::value;

/**
    * @brief check whether a type_info has ctors
    */
template <typename TypeInfo>
constexpr bool has_ctors_v = Detail::has_ctors<TypeInfo>::value;

/**
    * @brief check whether a type_info has field
    */
template <typename TypeInfo>
constexpr bool has_fields_v = Detail::has_fields<TypeInfo>::value;

template <typename T, typename Field>
struct field_descriptor {
    using clazz = T;
    using field = Field;
};

template <typename T>
class reflect_info final {
public:
    using type = type_info<T>;

    /**
        * @brief construct a instance
        */
    template <typename... Args>
    T construct(Args&&... args) {
        return T{ std::forward<Args>(args)... };
    }

    /**
        * @brief check whether the type is class
        */
    constexpr bool is_class() const noexcept {
        return std::is_class_v<T>;
    }

    /**
        * @brief check whether the type is enum
        */
    constexpr bool is_enum() const noexcept {
        return std::is_enum_v<T>;
    }

    /**
        * @brief check whether class has base classes
        */
    constexpr bool has_bases() const noexcept { return has_bases_v<type>; }

    /**
        * @brief check whether class has constructors
        */
    constexpr bool has_ctors() const noexcept { return has_ctors_v<type>; }

    /**
        * @brief check whether class has fields
        */
    constexpr bool has_fields() const noexcept { return has_fields_v<type>; }

    constexpr decltype(auto) enum_values() const noexcept {
        if constexpr (std::is_enum_v<T>) {
            return type::enums;
        }
        else {
            return std::array<enum_value<int>, 0>{};
        }
    }

    /**
    * @brief runtime tool: visit all fields
    */
    template <typename Function>
    void visit_fields(Function&& func) {
        if constexpr (has_fields_v<type>) {
            std::apply(
                [&func](auto&&... args) {
                    (func(std::forward<decltype(args)>(args)), ...);
                },
                type::fields);
        }
    }

    /**
    * @brief runtime tool: visit all member variables
    */
    template <typename Function>
    void visit_member_variables(Function&& func) {
        if constexpr (has_fields_v<type>) {
            do_visit_member_variables<0>(std::forward<Function>(func));
        }
    }

    constexpr std::string_view name() const noexcept {
        return type::name();
    }

    /**
    * @brief runtime tool: visit all member functions
    */
    template <typename Function>
    void visit_member_functions(Function&& func) {
        if constexpr (has_fields_v<type>) {
            do_visit_member_functions<0>(std::forward<Function>(func));
        }
    }

private:
    template <size_t Idx, typename Function>
    void do_visit_member_variables(Function&& func) {
        auto fields = type::fields;
        if constexpr (Idx < list_size_v<std::remove_cv_t<
            std::remove_reference_t<decltype(fields)>>>) {
            auto field = std::get<Idx>(fields);
            func(std::get<Idx>(fields));
            do_visit_member_variables<Idx + 1>(std::forward<Function>(func));
        }
    }

    template <size_t Idx, typename Function>
    void do_visit_member_functions(Function&& func) {
        constexpr auto fields = type::fields;
        if constexpr (Idx < list_size_v<std::remove_cv_t<
            std::remove_reference_t<decltype(fields)>>>) {
            constexpr auto field = std::get<Idx>(fields);
            if constexpr (field.is_function() && field.is_member()) {
                func(std::get<Idx>(fields));
            }
            do_visit_member_functions<Idx + 1>(std::forward<Function>(func));
        }
    }
};

/**
* @brief get reflected class info
*/
template <typename T>
constexpr auto reflect() {
    return reflect_info<remove_cvref_t<T>>{};
}

// --------------------------------------------------------------------------------

} // namespace HE::Refl

#define srefl_class(type, ...)                      \
namespace HE::Refl {                                \
    template <>                                     \
    struct ::HE::Refl::type_info<type> : ::HE::Refl::base_type_info<type> { \
        using type_ = type;                         \
        static constexpr std::string_view name() {  \
            return #type;                           \
        }                                           \
        __VA_ARGS__                                 \
    };                                              \
}

#define fields(...) \
    inline static constexpr auto fields = std::make_tuple(__VA_ARGS__);

#define field(name, ...)              \
    ::HE::Refl::field_traits {                       \
        &type_::name, #name, offsetof(type_, name), ##__VA_ARGS__ \
    }

#define bases(...) using bases = ::HE::Refl::type_list<__VA_ARGS__>;

// #define ctors(...) using ctors = ::HE::Refl::type_list<__VA_ARGS__>;
// 
// #define ctor(...) ctor<__VA_ARGS__>

#define srefl_enum(type, ...)                               \
    template <>                                             \
    struct ::HE::Refl::type_info<type> : ::HE::Refl::base_type_info<type> {         \
        static constexpr std::string_view name() {          \
            return #type;                                   \
        }                                                   \
        static constexpr std::array enums = {__VA_ARGS__}; \
    };

#define enum_value(value, name) \
    ::HE::Refl::enum_value {                \
        value, name             \
    }
}
