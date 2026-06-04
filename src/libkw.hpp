#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <meta>
#include <string_view>
#include <tuple>
#include <utility>

namespace libkw {

template <size_t N>
struct S {
  static_assert(N > 0);
  // Extra trailing '\0' (storage[N] vs. the N-1 real chars) isn't read by
  // view(), but it makes gcc's structural-NTTP diagnostic printer (which
  // otherwise drops the last real byte) render the full name.
  char storage[N]{};

  consteval S(const char (&zstr)[N])
  {
    if (zstr[N - 1] != 0) throw;
    std::copy_n(zstr, N - 1, storage);
  }

  [[nodiscard]] consteval std::string_view view() const { return {storage, N - 1}; }
};

template <S Name>
struct Kw {
  [[nodiscard]] static consteval std::string_view name() { return Name.view(); }
};

template <S Name>
inline constexpr Kw<Name> kw{};

namespace detail {

template <typename T>
struct is_kw : std::false_type {};

template <S Name>
struct is_kw<Kw<Name>> : std::true_type {};

template <typename... Ts>
[[nodiscard]] consteval bool all_even_are_kw()
{
  constexpr size_t N = sizeof...(Ts) / 2;
  return []<size_t... Js>(std::index_sequence<Js...>) {
    return (true && ... && is_kw<std::tuple_element_t<2 * Js, std::tuple<Ts...>>>::value);
  }(std::make_index_sequence<N>{});
}

template <typename... Ts>
[[nodiscard]] consteval auto extract_call_names()
{
  constexpr size_t N = sizeof...(Ts) / 2;
  std::array<std::string_view, N> names{};
  [&]<size_t... Js>(std::index_sequence<Js...>) {
    ((names[Js] = std::tuple_element_t<2 * Js, std::tuple<Ts...>>::name()), ...);
  }(std::make_index_sequence<N>{});
  return names;
}

// Non-negative values are errors that correspond to a missing kw name corresponding to function argument ordinal.
//
// TODO: Display missing kw index info in `static_assert` when compilers support constexper `std::format`.
enum ErrorCode : int {
  no_error = -1,
  kw_placement_syntax_error = -2,
};

// identifier_of() throws on a nameless parameter; check first so that case
// gets one clean static_assert instead of an uncaught-exception cascade.
template <std::meta::info Func>
[[nodiscard]] consteval bool all_params_named()
{
  for (auto param : std::meta::parameters_of(Func)) {
    if (!std::meta::has_identifier(param)) return false;
  }
  return true;
}

// Validates count, all names known, no duplicates — any order.
template <std::meta::info Func, typename... Ts>
[[nodiscard]] consteval ErrorCode validate_kw()
{
  if constexpr (!all_even_are_kw<Ts...>()) {
    return kw_placement_syntax_error;
  } else {
    constexpr size_t N = sizeof...(Ts) / 2;
    auto params = std::meta::parameters_of(Func);
    auto call_names = extract_call_names<Ts...>();

    std::array<bool, N> matched{}; // used to detect more than one match
    for (size_t j = 0; j < N; ++j) {
      bool found = false;
      for (size_t i = 0; i < N; ++i) {
        if (!matched[i] && std::meta::identifier_of(params[i]) == call_names[j]) {
          matched[i] = true;
          found = true;
          break;
        }
      }
      if (!found) return static_cast<ErrorCode>(j);
    }
    return no_error;
  }
}

// mapping[i] = call-site pair index for parameter i.
template <std::meta::info Func, typename... Ts>
[[nodiscard]] consteval auto make_mapping()
{
  constexpr size_t N = sizeof...(Ts) / 2;
  auto params = std::meta::parameters_of(Func);
  auto call_names = extract_call_names<Ts...>();

  std::array<size_t, N> mapping{};
  for (size_t i = 0; i < N; ++i) {
    auto param_name = std::meta::identifier_of(params[i]);
    for (size_t j = 0; j < N; ++j) {
      if (call_names[j] == param_name) {
        mapping[i] = j;
        break;
      }
    }
  }
  return mapping;
}

template <std::meta::info Func, size_t N, std::array<size_t, N> Mapping, size_t... Is, typename... Ts>
[[nodiscard, gnu::always_inline]] inline decltype(auto) dispatch(std::index_sequence<Is...>, Ts&&... ts)
{
  return [:Func:](std::forward<Ts...[2 * Mapping[Is] + 1]>(ts...[2 * Mapping[Is] + 1])...);
}

template <std::meta::info MemFunc, size_t N, std::array<size_t, N> Mapping, size_t... Is, typename Obj, typename... Ts>
[[nodiscard, gnu::always_inline]] inline decltype(auto) dispatch_method(
    Obj&& obj, std::index_sequence<Is...>, Ts&&... ts)
{
  return (std::forward<Obj>(obj).[:MemFunc:])(std::forward<Ts...[2 * Mapping[Is] + 1]>(ts...[2 * Mapping[Is] + 1])...);
}

template <std::meta::info refl_, typename... Ts_>
[[nodiscard]] consteval bool allow_dispatch()
{
  constexpr size_t n_ = std::meta::parameters_of(refl_).size();
  constexpr bool arity_match_ = 2 * n_ == sizeof...(Ts_);
  if constexpr (!arity_match_) {
    // TODO: When fmt::format becomes constexpr, can ditch too-few/too-many branching and just print the counts.
    constexpr bool too_few_args_ = 2 * n_ > sizeof...(Ts_);
    if constexpr (too_few_args_) {
      static_assert(false, "Too few named arguments for target function.");
    } else {
      static_assert(false, "Too many named arguments for target function.");
    }
    return false;
  } else if constexpr (!all_params_named<refl_>()) {
    static_assert(false, "All parameters of target function must be named.");
    return false;
  } else {
    constexpr int error_code_ = ::libkw::detail::validate_kw<refl_, std::remove_cvref_t<Ts_>...>();
    if constexpr (error_code_ != ::libkw::detail::ErrorCode::no_error) {
      static_assert(error_code_ != ::libkw::detail::ErrorCode::kw_placement_syntax_error,
          "Keyword tag is syntactically positioned incorrectly.");
      static_assert(error_code_ < 0, "Missing required named argument.");
      return false;
    } else {
      return true;
    }
  }
}

// Allowed to be used in evalidated contexts, but only if that context otherwise produces a compile error.
// Normal `std::declval` is technically UB in such dead code.
template <typename T>
[[nodiscard]] inline T safe_declval()
{
  std::terminate();
}

template <std::meta::info Refl_, typename... Ts_>
[[nodiscard, gnu::always_inline]] inline decltype(auto) kw_call(Ts_&&... ts_)
{
  if constexpr (allow_dispatch<Refl_, Ts_...>()) {
    constexpr size_t n_ = std::meta::parameters_of(Refl_).size();
    constexpr auto mapping_ = make_mapping<Refl_, std::remove_cvref_t<Ts_>...>();
    return dispatch<Refl_, n_, mapping_>(std::make_index_sequence<n_>{}, std::forward<Ts_>(ts_)...);
  } else {
    return safe_declval<typename[:std::meta::return_type_of(Refl_):]>();
  }
}

template <std::meta::info Refl_, typename Obj_, typename... Ts_>
[[nodiscard, gnu::always_inline]] inline decltype(auto) kw_method_call(Obj_&& obj_, Ts_&&... ts_)
{
  if constexpr (allow_dispatch<Refl_, Ts_...>()) {
    constexpr size_t n_ = std::meta::parameters_of(Refl_).size();
    constexpr auto mapping_ = make_mapping<Refl_, std::remove_cvref_t<Ts_>...>();
    return dispatch_method<Refl_, n_, mapping_>(
        std::forward<Obj_>(obj_), std::make_index_sequence<n_>{}, std::forward<Ts_>(ts_)...);
  } else {
    return safe_declval<typename[:std::meta::return_type_of(Refl_):]>();
  }
}

} // namespace detail

} // namespace libkw

// Call a free function or static method with named arguments (any order).
//   KW_CALL(f, kw<"y">, 2, kw<"x">, 1)
#define KW_CALL(f, ...) ::libkw::detail::kw_call<^^f>(__VA_ARGS__)

// Call an instance method with named arguments (any order).
//   KW_METHOD_CALL(obj, method, kw<"y">, 2, kw<"x">, 1)
#define KW_METHOD_CALL(obj, method, ...) \
  ::libkw::detail::kw_method_call<^^std::remove_cvref_t<decltype(obj)>::method>(obj __VA_OPT__(, ) __VA_ARGS__)
