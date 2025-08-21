#pragma

namespace HE {
namespace Refl {
// --------------------------------------------------------------------------------
//                                    variable_traits
// --------------------------------------------------------------------------------

// variable_type
namespace Detail {

template <typename T>
struct variable_type {
	using type = T;
};

template <typename Class, typename T>
struct variable_type<T Class::*> {
	using type = T;
};

}  // namespace Detail

/**
* @brief get variable type
* simple variable: same as T
* class variable: variable type
*
* @tparam T a variable type
*/
template <typename T>
using variable_type_t = typename Detail::variable_type<T>::type;

namespace Detail {

template <typename T>
auto variable_pointer_to_type(long, T*)->T;

template <typename Class, typename T>
auto variable_pointer_to_type(char, T Class::*)->T;

} // namespace Detail

/**
* @brief get a variable type from it's variable pointer
*/
template <auto V>
using variable_pointer_to_type_t = decltype(Detail::variable_pointer_to_type(0, V));

namespace Detail {

template <typename T>
struct basic_variable_traits {
	using type = variable_type_t<T>;
	static constexpr bool is_member = std::is_member_pointer_v<T>;
	static constexpr bool is_const = std::is_const_v<std::remove_reference_t<std::remove_pointer_t<type>>>;
};

}  // namespace Detail

/**
* @brief extract variable info from variable type
*
* @tparam Func
*/
template <typename T>
struct variable_traits;

template <typename T>
struct variable_traits<T*> : Detail::basic_variable_traits<T> {
	using pointer = T*;
};

template <typename Class, typename T>
struct variable_traits<T Class::*> : Detail::basic_variable_traits<T Class::*> {
	using pointer = T Class::*;
	using clazz = Class;
};

namespace Detail {

template <auto V>
struct variable_pointer_traits : variable_traits<decltype(V)> {};

} // namespace Detail

/**
* @brief extract variable info from variable pointer
*
* @tparam F
*/
template <auto V>
using variable_pointer_traits = Detail::variable_pointer_traits<V>;

// --------------------------------------------------------------------------------

// --------------------------------------------------------------------------------
//                                    function_traits
// --------------------------------------------------------------------------------

// function_type_t
namespace Detail {

	template <typename F>
	struct function_type;

	template <typename Ret, typename... Args>
	struct function_type<Ret(*)(Args...)> {
		using type = Ret(Args...);
	};

	template <typename Ret, typename Class, typename... Args>
	struct function_type<Ret(Class::*)(Args...)> {
		using type = Ret(Class::*)(Args...);
	};

	template <typename Ret, typename Class, typename... Args>
	struct function_type<Ret(Class::*)(Args...) const> {
		using type = Ret(Class::*)(Args...) const;
	};

} // namespace HE::Refl::Detail

/**
 * @brief get function type from function pointer type
 *
 * @tparam F
 */
template <typename F>
using function_type_t = typename Detail::function_type<F>::type;

// function_point_type_t
namespace Detail {

	template <typename Ret, typename... Args>
	auto function_pointer_to_type(int, Ret(*)(Args...))->Ret(*)(Args...);

	template <typename Ret, typename Class, typename... Args>
	auto function_pointer_to_type(char, Ret(Class::*)(Args...))->Ret(Class::*)(Args...);

	template <typename Ret, typename Class, typename... Args>
	auto function_pointer_to_type(char, Ret(Class::*)(Args...) const)->Ret(Class::*)(Args...) const;

} // namespace HE::Refl::Detail

/**
 * @brief get function type from function pointer type
 *
 * @tparam F
 */
template <auto F>
using function_point_type_t = decltype(Detail::function_pointer_to_type(0, F));

/**
 * @brief get a function type from it's function pointer
 *
 * @tparam F
 */
template <auto F>
using function_type_from_pointer_t = function_type_t<decltype(Detail::function_pointer_to_type<F>)>;

// function_traits
namespace Detail {

	template <typename F>
	struct base_func_traits;

	template <typename Ret, typename... Args>
	struct base_func_traits<Ret(Args...)> {
		using args = type_list<Args...>;
		using return_type = Ret;
	};

}

/**
 * @brief extract function info from function type
 *
 * @tparam Func
 */
template <typename F>
struct function_traits;

template <typename Ret, typename... Args>
struct function_traits<Ret(Args...)> : public Detail::base_func_traits<Ret(Args...)> {
	using type = Ret(Args...);
	using args_with_class = type_list<Args...>;
	using pointer = Ret(*)(Args...);
	static constexpr bool is_member = false;
	static constexpr bool is_const = false;
};

template <typename Ret, typename Class, typename... Args>
struct function_traits<Ret(Class::*)(Args...)> : public Detail::base_func_traits<Ret(Args...)> {
	using type = Ret(Class::*)(Args...);
	using args_with_class = type_list<Class*, Args...>;
	using pointer = Ret(Class::*)(Args...);
	static constexpr bool is_member = true;
	static constexpr bool is_const = false;
};

template <typename Ret, typename Class, typename... Args>
struct function_traits<Ret(Class::*)(Args...)const> : public Detail::base_func_traits <Ret(Args...)> {
	using type = Ret(Class::*)(Args...) const;
	using args_with_class = type_list<Class*, Args...>;
	using pointer = Ret(Class::*)(Args...) const;
	static constexpr bool is_member = true;
	static constexpr bool is_const = true;
};

namespace Detail {
	template<auto F>
	struct function_pointer_traits : function_type_from_pointer_t<F> {};
}

/**
 * @brief extract function info from function pointer
 *
 * @tparam F
 */
template <auto F>
using function_pointer_traits = Detail::function_pointer_traits<F>;

/**
 * @brief check a type is a function or function pointer
 */
template <typename T>
constexpr bool is_function_v = std::is_function_v<T> || std::is_member_function_pointer_v<T>;

// --------------------------------------------------------------------------------


} // namespace Refl
} // namespace HE