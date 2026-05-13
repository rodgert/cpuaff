/* Copyright (c) 2015-2017, Daniel C. Dillon
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*!
 * \file impl/basic_affinity_stack.hpp
 * \brief LIFO-style affinity scoping helper: push the current
 * affinity, do scoped work, pop to restore.
 *
 * \see cpuaff::impl::basic_affinity_stack
 * \see cpuaff::impl::basic_affinity_manager
 */

#pragma once

#include "../config.hpp"
#include "../detail/expected.hpp"
#include "../error.hpp"
#include "basic_affinity_manager.hpp"
#include "basic_cpu.hpp"
#include "basic_cpu_set.hpp"
#include <stack>
#include <system_error>

namespace cpuaff
{
namespace impl
{
/*!
 * \brief LIFO-style helper for scoped affinity changes.
 *
 * basic_affinity_stack is used to keep track of affinities as you get and set
 * them.  It is basically a wrapper for an affinity_manager that gives you the
 * ability to keep track of what affinities have been and reset them later.
 *
 * Both the legacy bool-returning surface (\c [[deprecated]]) and the
 * Phase 4 \c try_* surface returning
 * \c cpuaff::expected<T,std::error_code> are provided. The \c try_*
 * surface additionally surfaces \ref cpuaff::affinity_errc::stack_empty
 * when \c try_pop_affinity is called on an empty stack.
 *
 * \see cpuaff::affinity_errc
 */
template < typename TRAITS >
class basic_affinity_stack
{
   public:
    typedef basic_affinity_manager< TRAITS > affinity_manager_type;
    typedef basic_cpu_set< TRAITS > cpu_set_type;

   public:
    /*!
     * \brief Construct a basic_affinity_stack for the given affinity_manager.
     *
     * \param affinity_manager the affinity_manager that this affinity stack
     *                          should use for getting and setting affinities.
     */
    inline basic_affinity_stack(affinity_manager_type &affinity_manager)
        : affinity_manager_(affinity_manager)
    {
    }

    /*!
     * \brief Push the current cpu affinity onto the stack.
     *
     * \return true if the current affinity was successfully captured and
     *         pushed; false otherwise.
     *
     * \deprecated Prefer try_push_affinity() — returns
     * cpuaff::expected with errno diagnostics. Will be removed in v3.
     */
    [[deprecated("use try_push_affinity()")]]
    inline bool push_affinity()
    {
        auto current = affinity_manager_.try_get_affinity();
        if (!current)
            return false;
        affinity_stack_.push(*std::move(current));
        return true;
    }

    /*!
     * \brief Pop a previously pushed affinity off the stack and set the
     * current thread's affinity to the popped affinity.
     *
     * \return true if a recorded affinity was successfully popped and
     *         applied; false if the stack was empty or the
     *         set-affinity call failed.
     *
     * \deprecated Prefer try_pop_affinity() — returns
     * cpuaff::expected with errno diagnostics. Will be removed in v3.
     */
    [[deprecated("use try_pop_affinity()")]]
    inline bool pop_affinity()
    {
        if (affinity_stack_.empty())
            return false;
        cpu_set_type cpus = affinity_stack_.top();
        affinity_stack_.pop();
        return affinity_manager_.try_set_affinity(cpus).has_value();
    }

    /*!
     * \brief Get the affinity of the calling thread.
     *
     * \param cpus [out] the set of cpus the calling thread can run on.
     * \return true on success; false if the kernel query failed.
     *
     * \deprecated Prefer try_get_affinity() — returns
     * cpuaff::expected with errno diagnostics. Will be removed in v3.
     */
    [[deprecated("use try_get_affinity()")]]
    inline bool get_affinity(cpu_set_type &cpus)
    {
        auto result = affinity_manager_.try_get_affinity();
        if (!result)
            return false;
        cpus = *std::move(result);
        return true;
    }

    /*!
     * \brief Set the affinity of the calling thread.
     *
     * \param cpus [in] the set of cpus the calling thread should run on.
     * \return true on success; false if the kernel call failed.
     *
     * \deprecated Prefer try_set_affinity() — returns
     * cpuaff::expected with errno diagnostics. Will be removed in v3.
     */
    [[deprecated("use try_set_affinity()")]]
    inline bool set_affinity(const cpu_set_type &cpus)
    {
        return affinity_manager_.try_set_affinity(cpus).has_value();
    }

    // ---------------------------------------------------------------
    // Phase 4 (v2 cycle): error-returning API mirroring
    // basic_affinity_manager's try_* family.
    // ---------------------------------------------------------------

    /*!
     * \brief Push the current cpu affinity onto the stack.
     *
     * \return cpuaff::expected<void, std::error_code>; the error
     *         carries the underlying errno from sched_getaffinity
     *         (commonly EINVAL / ENOMEM).
     *
     * \since v2.0.0-htaa.beta.1
     */
    [[nodiscard]] inline cpuaff::expected< void, std::error_code >
        try_push_affinity()
    {
        auto current = affinity_manager_.try_get_affinity();
        if (!current)
        {
            return cpuaff::unexpected< std::error_code >(current.error());
        }
        affinity_stack_.push(*std::move(current));
        return {};
    }

    /*!
     * \brief Pop a previously pushed affinity off the stack and restore it.
     *
     * \return cpuaff::expected<void, std::error_code>;
     *         \ref cpuaff::affinity_errc::stack_empty when the stack
     *         has no recorded affinity to restore; otherwise
     *         propagates the error code from try_set_affinity().
     *
     * \warning Pairs with try_push_affinity(); calling this without a
     *          prior push yields stack_empty rather than UB.
     * \since v2.0.0-htaa.beta.1
     */
    [[nodiscard]] inline cpuaff::expected< void, std::error_code >
        try_pop_affinity()
    {
        if (affinity_stack_.empty())
        {
            return cpuaff::unexpected< std::error_code >(
                cpuaff::make_error_code(cpuaff::affinity_errc::stack_empty));
        }
        cpu_set_type cpus = affinity_stack_.top();
        affinity_stack_.pop();
        return affinity_manager_.try_set_affinity(cpus);
    }

    /*!
     * \brief Get the affinity of the calling thread.
     *
     * \return the cpu_set_type the calling thread is permitted to run
     *         on, or a std::error_code carrying the underlying errno.
     *
     * \note Forwards directly to the underlying affinity_manager;
     *       does not consult the stack.
     * \since v2.0.0-htaa.beta.1
     */
    [[nodiscard]] inline cpuaff::expected< cpu_set_type, std::error_code >
        try_get_affinity()
    {
        return affinity_manager_.try_get_affinity();
    }

    /*!
     * \brief Set the affinity of the calling thread.
     *
     * \param cpus [in] the set of cpus the calling thread should run on.
     * \return cpuaff::expected<void, std::error_code>; the error
     *         carries the underlying errno.
     *
     * \note Forwards directly to the underlying affinity_manager;
     *       does not push onto the stack.
     * \since v2.0.0-htaa.beta.1
     */
    [[nodiscard]] inline cpuaff::expected< void, std::error_code >
        try_set_affinity(const cpu_set_type &cpus)
    {
        return affinity_manager_.try_set_affinity(cpus);
    }

   private:
    affinity_manager_type &affinity_manager_;
    std::stack< cpu_set_type > affinity_stack_;
};
}  // namespace impl
}  // namespace cpuaff
