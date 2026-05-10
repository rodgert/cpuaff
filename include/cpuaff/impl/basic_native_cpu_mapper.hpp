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

#pragma once

#include "../config.hpp"
#include "basic_affinity_manager.hpp"
#include "basic_cpu.hpp"
#include "basic_cpu_set.hpp"
#include <map>
#include <type_traits>

namespace cpuaff
{
namespace impl
{
namespace detail
{
// Trait detector: true iff TRAITS exposes a static constexpr bool
// `has_identity_native_mapping` and it evaluates to true. Backends
// that set this flag are asserting that their native cpu id type is
// the same as the cpu identifier type, so the mapper can be built
// without sched_setaffinity round-trips.
template < typename T, typename = void >
struct has_identity_native_mapping_v : std::false_type
{
};

template < typename T >
struct has_identity_native_mapping_v<
    T,
    std::void_t< decltype(T::has_identity_native_mapping) > >
    : std::bool_constant< T::has_identity_native_mapping >
{
};
}  // namespace detail

/*!
 * basic_native_cpu_mapper is a utility class that maps native cpu
 * representations to cpus from a basic_cpu_manager.  It may not be available
 * for every platform.
 */
template < typename TRAITS >
class basic_native_cpu_mapper
{
   public:
    typedef typename TRAITS::cpu_identifier_type cpu_identifier_type;
    typedef typename TRAITS::cpu_identifier_wrapper_type
        cpu_identifier_wrapper_type;

    typedef typename TRAITS::native_cpu_type native_cpu_type;
    typedef typename TRAITS::native_cpu_wrapper_type native_cpu_wrapper_type;
    typedef typename TRAITS::native_get_affinity_type native_get_affinity_type;

    typedef basic_affinity_manager< TRAITS > affinity_manager_type;
    typedef basic_cpu< TRAITS > cpu_type;
    typedef basic_cpu_set< TRAITS > cpu_set_type;

   public:
    /*!
     * Constructs an uninitialized basic_native_cpu_mapper.
     */
    inline basic_native_cpu_mapper() {}

    /*!
     * Initializes a basic_native_cpu_mapper from the given affinity_manager.
     *
     * If TRAITS declares `static constexpr bool has_identity_native_mapping
     * = true` (the linux_impl backend does), the mapper is built directly
     * from the affinity_manager's enumerated cpus — no sched_setaffinity
     * round-trip required. This is the path used in production: the old
     * walk-by-pinning behaviour is a footgun on a live trading box (it
     * temporarily migrates the calling thread to every CPU in turn) and is
     * preserved only as the fallback for backends that lack the trait.
     *
     * For backends without the trait, falls back to the original walk: pin
     * the calling thread to each cpu in turn, query the native affinity,
     * and record the (native, cpu) pair. Restores the original affinity at
     * the end. May fail or hang if pinning isn't possible.
     *
     * \param affinity_manager the affinity_manager to load configured cpus from
     * \return true if initialization succeeds, false otherwise
     */
    inline bool initialize(const affinity_manager_type &affinity_manager)
    {
        cpu_set_type cpus;
        if (!affinity_manager.get_cpus(cpus)) return false;

        if constexpr (detail::has_identity_native_mapping_v< TRAITS >::value)
        {
            static_assert(
                std::is_same< native_cpu_type, cpu_identifier_type >::value,
                "TRAITS::has_identity_native_mapping requires "
                "native_cpu_type == cpu_identifier_type");

            for (const auto &cpu : cpus)
            {
                native_cpu_wrapper_type native(
                    static_cast< native_cpu_type >(cpu.id().get()));
                cpu_by_native_[native] = cpu;
                native_by_cpu_[cpu] = native;
            }
            return true;
        }
        else
        {
            bool retval = true;
            cpu_set_type orig;

            if (affinity_manager.get_affinity(orig))
            {
                typename cpu_set_type::iterator i = cpus.begin();
                typename cpu_set_type::iterator iend = cpus.end();

                for (; i != iend; ++i)
                {
                    cpu_set_type affinity;
                    affinity.insert(*i);

                    if (affinity_manager.set_affinity(affinity))
                    {
                        std::set< native_cpu_wrapper_type > ids;

                        if (native_get_affinity_type()(ids) && ids.size() == 1)
                        {
                            cpu_by_native_[*ids.begin()] = *i;
                            native_by_cpu_[*i] = *ids.begin();
                        }
                        else
                        {
                            retval = false;
                            break;
                        }
                    }
                    else
                    {
                        retval = false;
                    }
                }

                affinity_manager.set_affinity(orig);
            }
            else
            {
                retval = false;
            }

            return retval;
        }
    }

    /*!
     * Get the cpu with the given native identifier.
     *
     * \param cpu [out] the cpu with the given native identifier
     * \param native [in] the native cpu identifier
     * \return true if the cpu is found, false otherwise.
     */
    inline bool get_cpu_from_native(cpu_type &cpu,
                                    const native_cpu_wrapper_type &native) const
    {
        typename std::map< native_cpu_wrapper_type, cpu_type >::const_iterator
            i = cpu_by_native_.find(native);

        if (i != cpu_by_native_.end())
        {
            cpu = i->second;
            return true;
        }

        return false;
    }

    /*!
     * Get the cpu with the given native identifier.
     *
     * \param cpu [out] the cpu with the given native identifier
     * \param native [in] the native cpu identifier
     * \return true if the cpu is found, false otherwise.
     */
    inline bool get_cpu_from_native(cpu_type &cpu,
                                    const native_cpu_type &native) const
    {
        return get_cpu_from_native(cpu, native_cpu_wrapper_type(native));
    }

    /*!
     * Get the native identifier for the given cpu.
     *
     * \param native [out] native cpu identifier wrapper
     * \param cpu [in] the cpu
     * \return true if the native identifier is found, false otherwise.
     */

    inline bool get_native_from_cpu(native_cpu_wrapper_type &native,
                                    const cpu_type &cpu) const
    {
        typename std::map< cpu_type, native_cpu_wrapper_type >::const_iterator
            i = native_by_cpu_.find(cpu);

        if (i != native_by_cpu_.end())
        {
            native = i->second;
            return true;
        }

        return false;
    }

   private:
    std::map< native_cpu_wrapper_type, cpu_type > cpu_by_native_;
    std::map< cpu_type, native_cpu_wrapper_type > native_by_cpu_;
};
}  // namespace impl
}  // namespace cpuaff
