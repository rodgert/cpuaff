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
 * \file impl/basic_round_robin_allocator.hpp
 * \brief Round-robin cpu allocator that prefers spreading consecutive
 * allocations across distinct cores.
 *
 * \see cpuaff::impl::basic_round_robin_allocator
 */

#pragma once

#include "../config.hpp"
#include "../detail/expected.hpp"
#include "../error.hpp"
#include "basic_cpu.hpp"
#include "basic_cpu_set.hpp"
#include <map>
#include <queue>
#include <system_error>

namespace cpuaff
{
namespace impl
{
/*!
 * \brief Round-robin cpu allocator.
 *
 * basic_round_robin_allocator is a utility class that takes a set of cpus
 * and returns them as requested in a round-robin fashion.  It organizes the
 * cpus such that it returns consecutive cpus from different cores if it can.
 *
 * The legacy \c allocate() surface is \c [[deprecated]]; the
 * Phase 4 \c try_allocate() surface returns
 * \c cpuaff::expected<T,std::error_code> and surfaces
 * \ref cpuaff::affinity_errc::allocator_empty for the
 * empty-allocator case (which the legacy single-cpu \c allocate()
 * was willing to UB on).
 */
template < typename TRAITS >
class basic_round_robin_allocator
{
   public:
    typedef basic_cpu< TRAITS > cpu_type;
    typedef basic_cpu_set< TRAITS > cpu_set_type;

   public:
    /*!
     * \brief Constructs a basic_round_robin_allocator with the given set of cpus.
     *
     * \param cpus the set of cpus that this allocator should iterate over.
     */
    inline basic_round_robin_allocator(const cpu_set_type &cpus)
    {
        initialize(cpus);
    }

    /*!
     * \brief Get the next cpu in the round-robin.
     *
     * \return the next cpu in the round robin.
     *
     * \warning Calls front() / pop() on an internal queue with no
     *          empty-check; calling on an empty allocator is
     *          undefined behaviour.
     * \deprecated Prefer try_allocate() — returns cpuaff::expected
     *             and surfaces \c allocator_empty rather than
     *             UB-ing on an empty allocator.
     */
    [[deprecated("use try_allocate() — returns cpuaff::expected and "
                 "doesn't UB on an empty allocator")]]
    inline cpu_type allocate()
    {
        cpu_type retval = cpu_queue_.front();
        cpu_queue_.pop();
        cpu_queue_.push(retval);
        return retval;
    }

    /*!
     * \brief Get the next \p count cpus in the round-robin.
     *
     * \param cpus [out] the set of cpus drawn from the allocator.
     * \param count [in] the number of cpus to draw; if larger than
     *              the allocator's distinct cpu count the result
     *              caps at the distinct cpu count.
     * \return true if any cpus were drawn; false if the allocator
     *         was empty.
     *
     * \deprecated Prefer try_allocate(uint32_t).
     */
    [[deprecated("use try_allocate()")]]
    inline bool allocate(cpu_set_type &cpus, uint32_t count)
    {
        cpus.clear();

        for (uint32_t i = 0; i < count; ++i)
        {
            if (cpu_queue_.empty()) return !cpus.empty();
            cpu_type retval = cpu_queue_.front();
            cpu_queue_.pop();
            cpu_queue_.push(retval);
            cpus.insert(retval);
        }

        return true;
    }

    /*!
     * \brief Number of cpus in the round-robin queue.
     *
     * \return the count of cpus the allocator can hand out per cycle.
     */
    [[nodiscard]] inline std::size_t size() const noexcept
    {
        return cpu_queue_.size();
    }

    /*!
     * \brief True iff the allocator has no cpus to hand out.
     *
     * \return true if the allocator is empty; false otherwise.
     */
    [[nodiscard]] inline bool empty() const noexcept
    {
        return cpu_queue_.empty();
    }

    // ---------------------------------------------------------------
    // Phase 4 (v2 cycle): error-returning API.
    //
    // Returns std::errc::no_message_available if the allocator is
    // empty (no cpus to hand out). The bool-returning legacy API was
    // willing to UB in this case.
    // ---------------------------------------------------------------

    /*!
     * \brief Get the next cpu in the round-robin.
     *
     * \return the next cpu on success;
     *         \ref cpuaff::affinity_errc::allocator_empty if the
     *         allocator has no cpus to hand out.
     *
     * \since v2.0.0-htaa.beta.1
     */
    [[nodiscard]] inline cpuaff::expected< cpu_type, std::error_code >
    try_allocate()
    {
        if (cpu_queue_.empty())
        {
            return cpuaff::unexpected< std::error_code >(
                cpuaff::make_error_code(
                    cpuaff::affinity_errc::allocator_empty));
        }
        cpu_type retval = cpu_queue_.front();
        cpu_queue_.pop();
        cpu_queue_.push(retval);
        return retval;
    }

    /*!
     * \brief Get up to \p count cpus in round-robin order.
     *
     * \param count the number of cpus to draw.
     * \return a cpu_set_type holding up to \p count cpus; always
     *         a value (returning an empty set if the allocator is
     *         empty, or a smaller set if \p count exceeds the
     *         number of distinct cpus).
     *
     * \note Unlike the single-cpu \c try_allocate(), this overload
     *       never returns an error — an empty allocator yields an
     *       empty set rather than \c allocator_empty.
     * \since v2.0.0-htaa.beta.1
     */
    [[nodiscard]] inline cpuaff::expected< cpu_set_type, std::error_code >
    try_allocate(uint32_t count)
    {
        cpu_set_type out;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (cpu_queue_.empty()) break;
            cpu_type retval = cpu_queue_.front();
            cpu_queue_.pop();
            cpu_queue_.push(retval);
            out.insert(retval);
        }
        return out;
    }

   private:
    /*!
     * Initializes a basic_round_robin_allocator with the given set of cpus.
     *
     * \param cpus the set of cpus that this allocator should iterate over
     * \return true if there are cpus in the cpu set, false otherwise
     */
    inline bool initialize(const cpu_set_type &cpus)
    {
        std::map< processing_unit_type, cpu_set_type > cpus_by_pu;

        typename cpu_set_type::iterator i = cpus.begin();
        typename cpu_set_type::iterator iend = cpus.end();

        for (; i != iend; ++i)
        {
            cpus_by_pu[i->processing_unit()].insert(*i);
        }

        typename std::map< processing_unit_type, cpu_set_type >::const_iterator
            j = cpus_by_pu.begin();
        typename std::map< processing_unit_type, cpu_set_type >::const_iterator
            jend = cpus_by_pu.end();

        for (; j != jend; ++j)
        {
            typename cpu_set_type::iterator k = j->second.begin();
            typename cpu_set_type::iterator kend = j->second.end();

            for (; k != kend; ++k)
            {
                cpu_queue_.push(*k);
            }
        }

        return !!cpus.size();
    }

   private:
    std::queue< cpu_type > cpu_queue_;
};
}  // namespace impl
}  // namespace cpuaff
