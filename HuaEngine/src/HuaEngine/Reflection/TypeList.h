#pragma once

#include <tuple>

namespace HE {
namespace Refl {

// --------------------------------------------------------------------------------
//                                    type_list
// --------------------------------------------------------------------------------
template<typename... Ts>
struct type_list {
using self_type = type_list<Ts...>;
static constexpr size_t size = sizeof...(Ts);
};

namespace Detail {

// list_element
template<typename, size_t>
struct list_element;

template <template <typename...> typename ListType, typename T, typename... Ts, size_t N>
struct list_element<ListType<T, Ts...>, N>
	: list_element<ListType<Ts...>, N - 1> {
};

template <template <typename...> typename ListType, typename T, typename... Ts>
struct list_element<ListType<T, Ts...>, 0> {
	using type = T;
};

// list_size
template<typename>
struct list_size;

template<template <typename...> typename ListType, typename... Ts>
struct list_size<ListType<Ts...>> {
	static constexpr size_t size = sizeof...(Ts);
};

// list_head
template<typename>
struct list_head;

template<template <typename...> typename ListType, typename T, typename... Ts>
struct list_head<ListType<T, Ts...>> {
	using type = T;
};

// list_tail
template<typename>
struct list_tail;

template<template <typename...> typename ListType, typename T, typename... Ts>
struct list_tail<ListType<T, Ts...>> {
	using type = ListType<Ts...>;
};

// list_add_to_first
template<typename List, typename T>
struct list_add_to_first;

template<template <typename...> typename ListType, typename T, typename... Ts>
struct list_add_to_first<ListType<Ts...>, T> {
	using type = ListType<T, Ts...>;
};

// list_add_to_tail
template<typename List, typename T>
struct list_add_to_end;

template<template <typename...> typename ListType, typename T, typename... Ts>
struct list_add_to_end<ListType<Ts...>, T> {
	using type = ListType<Ts..., T>;
};

} // namespace Refl::Detail

/**
* @brief get the N-th element in type list(from type_list or std::tuple or others)
*
* @tparam List
* @tparam N the element index
*/
template<typename List, size_t N>
using list_element_t = typename Detail::list_element<List, N>::type;

/**
* @brief get the size of element list (from type_list or std::tuple or others)
*
* @tparam List
*/
template<typename List>
constexpr size_t list_size_v = typename Detail::list_size<List>::size;

/**
* @brief check if the element list is empty (from type_list or std::tuple or others)
*
* @tparam List
*/
template<typename List>
constexpr bool is_list_empty_v = list_size_v<List> == 0;

/**
* @brief get the firet element of element list (from type_list or std::tuple or others)
*
* @tparam List
*/
template<typename List>
using list_head_t = typename Detail::list_head<List>::type;

/**
* @brief get the elements of element list exclude the first (from type_list or std::tuple or others)
*
* @tparam List
*/
template<typename List>
using list_tail_t = typename Detail::list_tail<List>::type;

/**
* @brief add element to element list at the first (from type_list or std::tuple or others)
*
* @tparam List
* @tparam T
*/
template<typename List, typename T>
using list_add_to_first_t = typename Detail::list_add_to_first<List, T>::type;

/**
* @brief add element to element list at the end (from type_list or std::tuple or others)
*
* @tparam List
* @tparam T
*/
template<typename List, typename T>
using list_add_to_end_t = typename Detail::list_add_to_end<List, T>::type;

// apply_to_element_t
namespace Detail {

template<typename List, size_t N, template <typename> typename F>
struct apply_to_element {
	using type = F<list_element_t<List, N>>;
};

} // namespace Refl::Detail

/**
* @brief add element to element list at the end (from type_list or std::tuple or others)
*
* @tparam List
* @tparam T
*/
template<typename List, size_t N, template <typename> typename F>
using apply_to_element_t = typename Detail::apply_to_element<List, N, F>::type;

// list_foreach
namespace Detail {

template<typename List, template <typename> typename F>
struct list_foreach {};

template<template <typename...> typename ListType, template <typename> typename F, typename... Ts>
struct list_foreach<ListType<Ts...>, F> {
	using type = ListType<typename F<Ts>::type...>;
};

} // namespace Refl::Detail

/**
* @brief add element to element list at the end (from type_list or std::tuple or others)
*
* @tparam List
* @tparam T
*/
template<typename List, template <typename> typename F>
using list_foreach_t = typename Detail::list_foreach<List, F>::type;

// disjunction
namespace Detail {

// typelist_to_tuple
template<typename TypeList>
struct typelist_to_tuple;

template<typename... Ts>
struct typelist_to_tuple<type_list<Ts...>> {
	using type = std::tuple<Ts...>;
};

// tuple_to_typelist
template<typename TypeList>
struct tuple_to_typelist;

template<typename... Ts>
struct tuple_to_typelist<std::tuple<Ts...>> {
	using type = type_list<Ts...>;
};

// disjunction
template<typename List, template <typename> typename F>
struct disjunction {
	static constexpr bool value =
		F<list_head_t<List>>::value ||
		disjunction<list_tail_t<List>, F>::value;
};

template<template <typename...> typename ListType, template <typename> typename F>
struct disjunction<ListType<>, F> {
	static constexpr bool value = false;
};

// conjunction
template<typename List, template <typename> typename F>
struct conjunction {
	static constexpr bool value =
		F<list_head_t<List>>::value &&
		conjunction<list_tail_t<List>, F>::value;
};

template<template <typename...> typename ListType, template <typename> typename F>
struct conjunction<ListType<>, F> {
	static constexpr bool value = true;
};

// concat
template<typename List1, typename List2>
struct concat;

template<template <typename...> typename ListType, typename... Ts1, typename... Ts2>
struct concat<ListType<Ts1...>, ListType<Ts2...>> {
	using type = ListType<Ts1..., Ts2...>;
};

// filter
template<typename List, template <typename> typename F>
struct filter;

template<template <typename...> typename ListType, typename T, template <typename> typename F, typename... Ts>
struct filter<ListType<T, Ts...>, F> {
	using type = std::conditional_t<
		F<T>::value,
		list_add_to_first_t<typename filter<ListType<Ts...>, F>::type, T>,
		typename filter<ListType<Ts...>, F>::type
	>;
};

} // namespace Refl::Detail

/**
* @brief work as std::disjunction: use function struct F to check any of
* element is true
*
* @tparam List
* @tparam F  receive a type and give a constexpr static bool value;
*/
template<typename List, template <typename> typename F>
using disjunction_v = typename Detail::disjunction<List, F>::value;

/**
* @brief work as std::conjunction: use function struct F to check all of
* element is true
*
* @tparam List
* @tparam F  receive a type and give a constexpr static bool value;
*/
template<typename List, template <typename> typename F>
using conjunction_v = typename Detail::conjunction<List, F>::value;

/**
* @brief concat two type list
*
* @tparam List1
* @tparam List2
*/
template<typename List1, typename List2>
using concat_t = typename Detail::concat<List1, List2>::type;

/**
* @brief use function struct F to filter all of
* element is true
*
* @tparam List
* @tparam F  receive a type and give a constexpr static bool value;
*/
template<typename List, template <typename> typename F>
using filter_t = typename Detail::filter<List, F>::type;

/**
* @brief convert type list to std::tuple
*
* @tparam List
*/
template<typename List>
using typelist_to_tuple_t = typename Detail::typelist_to_tuple<List>::type;

/**
* @brief convert std::tuple to type list
*
* @tparam std::tuple
*/
template<typename List>
using tuple_to_typelist_t = typename Detail::tuple_to_typelist<List>::type;

// --------------------------------------------------------------------------------

}
}