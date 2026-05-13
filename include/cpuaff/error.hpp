/* Copyright (c) 2026 Thomas Rodgers
 * BSD-3-Clause; see LICENSE.
 */

/*!
 * \file error.hpp
 * \brief cpuaff error category and error codes for the try_*-style API.
 *
 * \c sched_setaffinity, \c sched_getaffinity, \c pthread_*affinity_np,
 * and \c CPU_ALLOC failures all surface as POSIX errno values. cpuaff
 * wraps those into \c std::error_code via \ref cpuaff::affinity_errc and
 * \ref cpuaff::affinity_category, so the new \c try_*-style API can
 * return \c cpuaff::expected<T,std::error_code> with a stable
 * diagnostic surface.
 *
 * The legacy bool-returning API on basic_affinity_manager is kept (now
 * \c [[deprecated]]) for source-compat with v1.x consumers; new code
 * should prefer the \c try_* overloads which carry diagnostics through.
 */

#pragma once

#include <cerrno>
#include <string>
#include <system_error>
#include <type_traits>

namespace cpuaff
{

/*!
 * \brief Error codes produced by cpuaff operations.
 *
 * Two flavours, distinguished by their underlying integer value:
 *
 * - **Errno-shaped values** (≤ 200ish) mirror the POSIX errno values
 *   that the underlying \c sched_*affinity / \c pthread_*affinity_np /
 *   \c CPU_ALLOC calls surface. \ref affinity_category's
 *   \c default_error_condition maps them onto
 *   \c std::generic_category, so callers can write
 *   `ec == std::errc::invalid_argument` and have it match.
 *
 * - **Cpuaff-internal values** (≥ 10000) cover conditions the kernel
 *   doesn't have an errno for — `round-robin allocator is empty`,
 *   `affinity stack is empty`. These stay in the
 *   \ref affinity_category for matching; there is no \c std::errc
 *   equivalent worth pretending we map onto. Callers compare with
 *   `ec == cpuaff::affinity_errc::stack_empty` etc.
 *
 * The enum is registered as an `std::error_code` enum (via the
 * `std::is_error_code_enum` specialisation at the bottom of this
 * header), so `std::error_code{cpuaff::affinity_errc::permission_denied}`
 * and equality comparisons against \c std::error_code values both work.
 */
enum class affinity_errc
{
    /*!
     * \brief Mask references CPUs that aren't available, or the call
     * referenced an invalid thread (errno \c EINVAL).
     *
     * Most commonly seen under cgroup / cpuset restriction (systemd
     * `CPUAffinity=`, Docker `--cpuset-cpus`) when the requested mask
     * includes CPUs the cgroup forbids. See
     * `try_get_available_cpus()` on `cpuaff::affinity_manager` for the
     * cgroup-aware accessor.
     */
    invalid_argument = EINVAL,

    /*!
     * \brief Caller lacks the privilege to set the requested affinity
     * (errno \c EPERM).
     *
     * On Linux this typically means the calling process needs
     * \c CAP_SYS_NICE to set affinity for a thread it doesn't own.
     */
    permission_denied = EPERM,

    /*!
     * \brief The target thread no longer exists (errno \c ESRCH).
     *
     * Surfaces from the pthread_t-taking overloads on
     * `cpuaff::affinity_manager` (`try_set_affinity(pthread_t, ...)`,
     * `try_get_affinity(pthread_t)`, `try_pin(pthread_t, ...)`) when
     * the target thread has exited between when the caller obtained
     * the `pthread_t` and when the syscall fires.
     */
    no_such_thread = ESRCH,

    /*! \brief Operation not supported (errno \c ENOSYS). */
    not_supported = ENOSYS,

    /*!
     * \brief \c CPU_ALLOC failed to allocate the dynamic cpu_set_t
     * (errno \c ENOMEM).
     */
    out_of_memory = ENOMEM,

    /*!
     * \brief Cpuaff-internal: \c try_pop_affinity called on an empty
     * affinity_stack.
     *
     * No POSIX errno equivalent. The value (10001) is well outside the
     * POSIX errno range so it doesn't collide with any kernel error.
     */
    stack_empty = 10001,

    /*!
     * \brief Cpuaff-internal: \c try_allocate called on an empty
     * round_robin_allocator.
     *
     * No POSIX errno equivalent. The value (10002) is well outside the
     * POSIX errno range so it doesn't collide with any kernel error.
     */
    allocator_empty = 10002,
};

namespace detail
{

/*!
 * \brief std::error_category implementation for cpuaff errors.
 *
 * Implementation detail; access via the \ref cpuaff::affinity_category
 * singleton accessor rather than instantiating directly.
 */
class affinity_category_impl : public std::error_category
{
   public:
    /*! \brief The category name, `"cpuaff::affinity"`. */
    const char *name() const noexcept override { return "cpuaff::affinity"; }

    /*!
     * \brief Human-readable diagnostic for an error value.
     *
     * Recognised \ref affinity_errc values get a tailored message
     * (POSIX errno meaning + cpuaff context where helpful);
     * unrecognised values fall through to
     * `std::generic_category().message(ev)` so any kernel errno cpuaff
     * doesn't have an explicit enum value for (e.g. \c EFAULT from a
     * bad mask pointer) still produces a real strerror(3)-equivalent
     * string.
     */
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
            case affinity_errc::stack_empty:
                return "affinity stack is empty (no recorded affinity to "
                       "pop)";
            case affinity_errc::allocator_empty:
                return "round-robin allocator is empty (no cpus to hand "
                       "out)";
        }
        return std::generic_category().message(ev);
    }

    /*!
     * \brief Map a cpuaff error value to a std::error_condition for
     * cross-category equality.
     *
     * Errno-shaped values (< 10000) map onto \c std::generic_category,
     * so `ec == std::errc::invalid_argument` matches an
     * `EINVAL`-tagged cpuaff error. Cpuaff-internal values (≥ 10000)
     * stay in our category.
     */
    std::error_condition default_error_condition(int ev) const noexcept override
    {
        if (ev >= 10000)
        {
            return std::error_condition(ev, *this);
        }
        return std::error_condition(ev, std::generic_category());
    }
};

}  // namespace detail

/*!
 * \brief Singleton accessor for the cpuaff error_category.
 *
 * \return reference to the process-wide cpuaff::affinity_category
 * instance. Stable identity across all translation units that include
 * this header within a single process image.
 */
inline const std::error_category &affinity_category() noexcept
{
    static const detail::affinity_category_impl instance;
    return instance;
}

/*!
 * \brief ADL hook for `std::error_code{affinity_errc::X}` construction.
 *
 * Found via argument-dependent lookup by the \c std::error_code
 * constructor. Application code typically doesn't call this directly —
 * just construct or compare against the enum value:
 *
 * \code
 *   std::error_code ec = cpuaff::affinity_errc::permission_denied;
 *   if (ec == cpuaff::affinity_errc::stack_empty) { ... }
 * \endcode
 *
 * \param e the affinity_errc value to wrap.
 * \return a `std::error_code` tagged with \ref affinity_category.
 */
inline std::error_code make_error_code(affinity_errc e) noexcept
{
    return std::error_code(static_cast< int >(e), affinity_category());
}

/*!
 * \brief Wrap a bare errno value (from \c sched_* / \c pthread_* /
 * \c CPU_ALLOC) into a cpuaff std::error_code.
 *
 * Always tags with \ref affinity_category so downstream code can
 * disambiguate "this is an affinity-call failure" from a generic POSIX
 * error of the same value. The errno value is preserved, so equality
 * comparison against \c std::errc still works through the
 * `default_error_condition` mapping.
 *
 * \param eno errno value as returned by glibc (positive int).
 * \return a `std::error_code` tagged with \ref affinity_category.
 */
inline std::error_code error_from_errno(int eno) noexcept
{
    return std::error_code(eno, affinity_category());
}

}  // namespace cpuaff

namespace std
{
/*!
 * \brief Register cpuaff::affinity_errc as an `std::error_code` enum.
 *
 * Enables the implicit `std::error_code = cpuaff::affinity_errc::X`
 * construction via ADL and the equality-comparison machinery.
 */
template <>
struct is_error_code_enum< cpuaff::affinity_errc > : true_type
{
};
}  // namespace std
