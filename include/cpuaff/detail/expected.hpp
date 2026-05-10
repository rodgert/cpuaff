/* Copyright (c) 2026 Thomas Rodgers
 * BSD-3-Clause; see LICENSE.
 *
 * Minimal cpuaff::expected.
 *
 * Aliases to std::expected when the implementation supports it
 * (libstdc++ ≥ 12 or libc++ ≥ 16 in C++23 mode); otherwise provides a
 * tiny std::variant- / std::optional-backed polyfill that covers
 * exactly the surface cpuaff uses internally:
 *
 *   * expected<T, E>::has_value() / operator bool()
 *   * expected<T, E>::value() / operator*() / operator->()
 *   * expected<T, E>::error()
 *   * expected<void, E> specialisation
 *   * unexpected<E>
 *
 * Extras that std::expected provides — and_then / transform /
 * or_else / bad_expected_access / etc. — are not implemented here.
 * cpuaff's public API treats this as a std::expected-shaped value;
 * callers that need richer composition should keep their own copy of
 * tl::expected or upgrade their toolchain to one that ships
 * std::expected.
 */

#pragma once

#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#if defined(__has_include)
#if __has_include(<expected>)
#include <expected>
#endif
#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#define CPUAFF_HAS_STD_EXPECTED 1
#else
#define CPUAFF_HAS_STD_EXPECTED 0
#endif

namespace cpuaff
{

#if CPUAFF_HAS_STD_EXPECTED

template < typename T, typename E >
using expected = std::expected< T, E >;

template < typename E >
using unexpected = std::unexpected< E >;

#else

template < typename E >
class unexpected
{
   public:
    constexpr explicit unexpected(const E &e) : value_(e) {}
    constexpr explicit unexpected(E &&e) : value_(std::move(e)) {}

    constexpr const E &error() const & noexcept { return value_; }
    constexpr E &error() & noexcept { return value_; }
    constexpr E &&error() && noexcept { return std::move(value_); }

   private:
    E value_;
};

template < typename T, typename E >
class expected
{
    static_assert(!std::is_same_v< T, E >,
                  "cpuaff::expected polyfill requires T != E "
                  "(distinct std::variant alternatives). Upgrade to a "
                  "toolchain with std::expected for full generality.");

   public:
    constexpr expected() : storage_(std::in_place_index< 0 >) {}
    constexpr expected(const T &v) : storage_(std::in_place_index< 0 >, v) {}
    constexpr expected(T &&v)
        : storage_(std::in_place_index< 0 >, std::move(v))
    {
    }
    constexpr expected(const unexpected< E > &u)
        : storage_(std::in_place_index< 1 >, u.error())
    {
    }
    constexpr expected(unexpected< E > &&u)
        : storage_(std::in_place_index< 1 >, std::move(u).error())
    {
    }

    constexpr bool has_value() const noexcept { return storage_.index() == 0; }
    constexpr explicit operator bool() const noexcept { return has_value(); }

    constexpr T &value() & { return std::get< 0 >(storage_); }
    constexpr const T &value() const & { return std::get< 0 >(storage_); }
    constexpr T &&value() && { return std::move(std::get< 0 >(storage_)); }

    constexpr T &operator*() & noexcept { return std::get< 0 >(storage_); }
    constexpr const T &operator*() const & noexcept
    {
        return std::get< 0 >(storage_);
    }
    constexpr T &&operator*() && noexcept
    {
        return std::move(std::get< 0 >(storage_));
    }

    constexpr T *operator->() noexcept { return std::get_if< 0 >(&storage_); }
    constexpr const T *operator->() const noexcept
    {
        return std::get_if< 0 >(&storage_);
    }

    constexpr E &error() & noexcept { return std::get< 1 >(storage_); }
    constexpr const E &error() const & noexcept
    {
        return std::get< 1 >(storage_);
    }
    constexpr E &&error() && noexcept
    {
        return std::move(std::get< 1 >(storage_));
    }

   private:
    std::variant< T, E > storage_;
};

template < typename E >
class expected< void, E >
{
   public:
    constexpr expected() noexcept = default;
    constexpr expected(const unexpected< E > &u) : error_(u.error()) {}
    constexpr expected(unexpected< E > &&u) : error_(std::move(u).error()) {}

    constexpr bool has_value() const noexcept { return !error_.has_value(); }
    constexpr explicit operator bool() const noexcept { return has_value(); }

    constexpr void value() const & {}

    constexpr E &error() & noexcept { return *error_; }
    constexpr const E &error() const & noexcept { return *error_; }
    constexpr E &&error() && noexcept { return std::move(*error_); }

   private:
    std::optional< E > error_;
};

#endif  // CPUAFF_HAS_STD_EXPECTED

}  // namespace cpuaff
