/* Copyright (c) 2026 Thomas Rodgers
 * BSD-3-Clause; see LICENSE.
 */

/*!
 * \file detail/expected.hpp
 * \brief cpuaff::expected — std::expected alias when the toolchain
 * ships it, minimal polyfill otherwise.
 *
 * Aliases \c std::expected when the implementation supports it
 * (`__cpp_lib_expected >= 202202L`, libstdc++ ≥ 12 / libc++ ≥ 16 in
 * C++23 mode); otherwise provides a tiny `std::variant`- /
 * `std::optional`-backed polyfill that covers exactly the surface
 * cpuaff uses internally:
 *
 *   - `expected<T, E>::has_value()` / `operator bool()`
 *   - `expected<T, E>::value()` / `operator*()` / `operator->()`
 *   - `expected<T, E>::error()`
 *   - `expected<void, E>` specialisation
 *   - `unexpected<E>`
 *   - `bad_expected_access<E>`
 *
 * Extras that std::expected provides — `and_then`, `transform`,
 * `or_else`, etc. — are not implemented here. cpuaff's public API
 * treats this as a std::expected-shaped value; callers that need
 * richer composition should keep their own copy of tl::expected or
 * upgrade their toolchain to one that ships std::expected.
 *
 * \warning Failure-mode parity with `std::expected`: `value()` throws
 * `bad_expected_access<E>` on missing-value, while `operator*()` and
 * `operator->()` are unchecked (UB on missing value). The polyfill
 * matches this contract exactly so code is portable across the alias
 * and polyfill paths.
 */

#pragma once

#include <exception>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#if defined(__has_include)
#if __has_include(<expected>)
#include <expected>
#endif
#endif

/*!
 * \def CPUAFF_HAS_STD_EXPECTED
 * \brief Defined to 1 when the toolchain ships a usable `std::expected`,
 * 0 otherwise. Drives whether \ref cpuaff::expected aliases the
 * standard or uses the polyfill.
 */
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#define CPUAFF_HAS_STD_EXPECTED 1
#else
#define CPUAFF_HAS_STD_EXPECTED 0
#endif

namespace cpuaff
{

#if CPUAFF_HAS_STD_EXPECTED

/*!
 * \brief Alias for \c std::expected when the toolchain ships it.
 *
 * See the polyfill block (`#else` branch of CPUAFF_HAS_STD_EXPECTED)
 * for the documentation of the surface cpuaff actually uses; the
 * alias is a strict superset.
 */
template < typename T, typename E >
using expected = std::expected< T, E >;

/*! \brief Alias for \c std::unexpected. */
template < typename E >
using unexpected = std::unexpected< E >;

/*! \brief Alias for \c std::bad_expected_access. */
template < typename E >
using bad_expected_access = std::bad_expected_access< E >;

#else

/*!
 * \brief Exception thrown by \ref expected::value() on the
 * missing-value path. Polyfill-only — when the standard alias is in
 * use, this resolves to \c std::bad_expected_access.
 */
template < typename E >
class bad_expected_access : public std::exception
{
   public:
    /*! \brief Construct with a copy of the error. */
    explicit bad_expected_access(E e) : error_(std::move(e)) {}

    /*! \brief Static identifier; the actual error lives in \ref error(). */
    const char *what() const noexcept override
    {
        return "bad cpuaff::expected access";
    }

    const E &error() const & noexcept { return error_; }   //!< Error accessor.
    E &error() & noexcept { return error_; }               //!< Error accessor.
    E &&error() && noexcept { return std::move(error_); }  //!< Error accessor.

   private:
    E error_;
};

/*!
 * \brief Wrapper that signals \ref expected construction with an
 * error rather than a value. Polyfill-only — when the standard alias
 * is in use, this resolves to \c std::unexpected.
 */
template < typename E >
class unexpected
{
   public:
    /*! \brief Construct with a copy of the error. */
    constexpr explicit unexpected(const E &e) : value_(e) {}
    /*! \brief Construct with a moved error. */
    constexpr explicit unexpected(E &&e) : value_(std::move(e)) {}

    constexpr const E &error() const & noexcept { return value_; }
    constexpr E &error() & noexcept { return value_; }
    constexpr E &&error() && noexcept { return std::move(value_); }

   private:
    E value_;
};

/*!
 * \brief Polyfill of \c std::expected for cpuaff's internal use.
 *
 * Backed by `std::variant<T, E>`. Implements the subset of the
 * std::expected surface that cpuaff itself uses (see file header).
 * `[[nodiscard]]` to match std::expected.
 *
 * \warning Requires `T != E` (distinct std::variant alternatives).
 * Enforced via \c static_assert; if you need T == E generality,
 * upgrade to a toolchain with std::expected.
 */
template < typename T, typename E >
class [[nodiscard]] expected
{
    static_assert(!std::is_same_v< T, E >,
                  "cpuaff::expected polyfill requires T != E "
                  "(distinct std::variant alternatives). Upgrade to a "
                  "toolchain with std::expected for full generality.");

   public:
    /*! \brief Default-construct holding a default-constructed T. */
    constexpr expected() : storage_(std::in_place_index< 0 >) {}
    /*! \brief Construct holding a copy of \p v. */
    constexpr expected(const T &v) : storage_(std::in_place_index< 0 >, v) {}
    /*! \brief Construct holding a moved \p v. */
    constexpr expected(T &&v) : storage_(std::in_place_index< 0 >, std::move(v))
    {
    }
    /*! \brief Construct holding the error from \p u. */
    constexpr expected(const unexpected< E > &u)
        : storage_(std::in_place_index< 1 >, u.error())
    {
    }
    /*! \brief Construct holding the moved error from \p u. */
    constexpr expected(unexpected< E > &&u)
        : storage_(std::in_place_index< 1 >, std::move(u).error())
    {
    }

    /*! \brief True iff this holds a value (rather than an error). */
    constexpr bool has_value() const noexcept { return storage_.index() == 0; }
    /*! \brief Same as has_value(). Explicit to prevent surprise. */
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /*!
     * \brief Checked value accessor.
     * \throws bad_expected_access<E> if !has_value() (carries a copy
     * of the error).
     */
    constexpr T &value() &
    {
        if (!has_value())
            throw bad_expected_access< E >(std::get< 1 >(storage_));
        return std::get< 0 >(storage_);
    }
    constexpr const T &value() const &
    {
        if (!has_value())
            throw bad_expected_access< E >(std::get< 1 >(storage_));
        return std::get< 0 >(storage_);
    }
    constexpr T &&value() &&
    {
        if (!has_value())
            throw bad_expected_access< E >(std::move(std::get< 1 >(storage_)));
        return std::move(std::get< 0 >(storage_));
    }

    /*!
     * \brief Unchecked dereference (UB if !has_value()).
     *
     * Matches `std::expected::operator*`. Use \ref value() for the
     * throwing variant, or check \ref has_value() / \c operator \c bool
     * first.
     */
    constexpr T &operator*() & noexcept { return *std::get_if< 0 >(&storage_); }
    constexpr const T &operator*() const & noexcept
    {
        return *std::get_if< 0 >(&storage_);
    }
    constexpr T &&operator*() && noexcept
    {
        return std::move(*std::get_if< 0 >(&storage_));
    }

    /*! \brief Unchecked member access (UB if !has_value()). */
    constexpr T *operator->() noexcept { return std::get_if< 0 >(&storage_); }
    constexpr const T *operator->() const noexcept
    {
        return std::get_if< 0 >(&storage_);
    }

    /*!
     * \brief Error accessor. UB if has_value() — std::expected has the
     * same precondition.
     */
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

/*!
 * \brief `expected<void, E>` specialisation for status-only operations
 * (try_set_affinity, try_pin, try_push_affinity, try_pop_affinity).
 *
 * Backed by `std::optional<E>` — empty optional means success, set
 * means error. `[[nodiscard]]` to match std::expected.
 */
template < typename E >
class [[nodiscard]] expected< void, E >
{
   public:
    /*! \brief Default-construct in the success state. */
    constexpr expected() noexcept = default;
    /*! \brief Construct in the error state, copying from \p u. */
    constexpr expected(const unexpected< E > &u) : error_(u.error()) {}
    /*! \brief Construct in the error state, moving from \p u. */
    constexpr expected(unexpected< E > &&u) : error_(std::move(u).error()) {}

    /*! \brief True iff this is in the success state. */
    constexpr bool has_value() const noexcept { return !error_.has_value(); }
    /*! \brief Same as has_value(). */
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /*!
     * \brief Validates that this is in the success state.
     * \throws bad_expected_access<E> if !has_value().
     */
    constexpr void value() const &
    {
        if (!has_value())
            throw bad_expected_access< E >(*error_);
    }

    /*! \brief Error accessor. UB if has_value(). */
    constexpr E &error() & noexcept { return *error_; }
    constexpr const E &error() const & noexcept { return *error_; }
    constexpr E &&error() && noexcept { return std::move(*error_); }

   private:
    std::optional< E > error_;
};

#endif  // CPUAFF_HAS_STD_EXPECTED

}  // namespace cpuaff
