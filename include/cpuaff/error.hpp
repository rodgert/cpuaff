/* Copyright (c) 2026 Thomas Rodgers
 * BSD-3-Clause; see LICENSE.
 *
 * cpuaff error category and error codes.
 *
 * sched_setaffinity, sched_getaffinity, pthread_*affinity_np, and
 * CPU_ALLOC failures all surface as POSIX errno values. cpuaff wraps
 * those into std::error_code via cpuaff::affinity_errc and
 * cpuaff::affinity_category(), so the new try_*-style API can return
 * `cpuaff::expected<T, std::error_code>` with a stable diagnostic
 * surface.
 *
 * The legacy bool-returning API on basic_affinity_manager is kept
 * (now [[deprecated]]) for source-compat with v1.x consumers; new code
 * should prefer the try_* overloads which carry diagnostics through.
 */

#pragma once

#include <cerrno>
#include <string>
#include <system_error>
#include <type_traits>

namespace cpuaff
{

/*!
 * Error categories produced by cpuaff operations.
 *
 * Values mirror the POSIX errno values that the underlying
 * sched_*affinity / pthread_*affinity_np / CPU_ALLOC calls surface, so
 * existing code that checks `ec == std::errc::invalid_argument` etc.
 * continues to work via the std::error_code generic-category mapping.
 */
enum class affinity_errc
{
    invalid_argument = EINVAL,
    permission_denied = EPERM,
    no_such_thread = ESRCH,
    not_supported = ENOSYS,
    out_of_memory = ENOMEM,
};

namespace detail
{

class affinity_category_impl : public std::error_category
{
   public:
    const char *name() const noexcept override { return "cpuaff::affinity"; }

    std::string message(int ev) const override
    {
        switch (static_cast< affinity_errc >(ev))
        {
            case affinity_errc::invalid_argument:
                return "invalid argument (EINVAL): mask references CPUs that "
                       "aren't available, or the call referenced an invalid "
                       "thread";
            case affinity_errc::permission_denied:
                return "permission denied (EPERM): caller lacks the privilege "
                       "to set the requested affinity";
            case affinity_errc::no_such_thread:
                return "no such thread (ESRCH): the target thread no longer "
                       "exists";
            case affinity_errc::not_supported:
                return "operation not supported (ENOSYS)";
            case affinity_errc::out_of_memory:
                return "out of memory (ENOMEM): CPU_ALLOC failed";
        }
        return "unknown cpuaff::affinity error";
    }

    // Map cpuaff::affinity_errc onto the std::generic_category (errno),
    // so callers can write `ec == std::errc::invalid_argument` and have
    // it match regardless of which category produced the code.
    std::error_condition default_error_condition(
        int ev) const noexcept override
    {
        return std::error_condition(ev, std::generic_category());
    }
};

}  // namespace detail

inline const std::error_category &affinity_category() noexcept
{
    static const detail::affinity_category_impl instance;
    return instance;
}

inline std::error_code make_error_code(affinity_errc e) noexcept
{
    return std::error_code(static_cast< int >(e), affinity_category());
}

// Helper: turn an errno value (from sched_*/pthread_*) into a cpuaff
// std::error_code. We always wrap with cpuaff::affinity_category so
// downstream code can disambiguate "this is an affinity-call failure"
// from a generic POSIX error of the same value.
inline std::error_code error_from_errno(int eno) noexcept
{
    return std::error_code(eno, affinity_category());
}

}  // namespace cpuaff

namespace std
{
template <>
struct is_error_code_enum< cpuaff::affinity_errc > : true_type
{
};
}  // namespace std
