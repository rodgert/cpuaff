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
 * \file linux.hpp
 * \brief Linux backend trait pack for cpuaff.
 * \internal
 *
 * Implements the loader (\ref cpuaff::impl::linux_impl::cpu_loader,
 * which calls into \ref cpuaff::impl::linux_impl::sysfs_reader to
 * walk \c /sys) and the affinity get/set functors that drive
 * \c sched_*affinity / \c pthread_*affinity_np. The functors expose
 * both the legacy bool-returning operator() form (kept for v1.x
 * source-compat) and the newer \c std::error_code -returning
 * \c query() / \c apply() entry points used by the \c try_*
 * basic_affinity_manager API.
 *
 * Everything in this file is implementation detail — application
 * code shouldn't reach in here directly. The trait struct at the
 * bottom is what \ref cpuaff::traits binds to.
 */

#pragma once

#include "../../cpu_spec.hpp"
#include "../../error.hpp"

#include <map>
#include <set>
#include <system_error>
#include <vector>

#include <libgen.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

#include "sysfs_reader.hpp"

namespace cpuaff
{
namespace impl
{
namespace linux_impl
{
/*!
 * \brief Native CPU identifier on Linux — the kernel CPU id.
 * \internal
 */
typedef int cpu_identifier_type;

/*!
 * \brief Comparable wrapper around \ref cpu_identifier_type.
 * \internal
 *
 * Wraps the bare \c int so it can serve as a key in associative
 * containers and so the type system distinguishes a CPU id from a
 * generic integer.
 */
class cpu_identifier_wrapper
{
   public:
    /*!
     * \brief Default-construct with the sentinel value \c -1.
     */
    inline cpu_identifier_wrapper() : id_(-1) {}

    /*!
     * \brief Wrap an existing kernel CPU id.
     * \param id the kernel CPU id.
     */
    inline cpu_identifier_wrapper(const cpu_identifier_type &id) : id_(id) {}

   public:
    /*!
     * \brief Read the wrapped kernel CPU id.
     * \return the kernel CPU id.
     */
    const inline cpu_identifier_type &get() const { return id_; }

    /*!
     * \brief Order by wrapped id.
     * \param rhs other wrapper to compare against.
     * \return \c true if this id is less than \p rhs's id.
     */
    inline bool operator<(const cpu_identifier_wrapper &rhs) const
    {
        return id_ < rhs.id_;
    }

   private:
    cpu_identifier_type id_;
};

/*!
 * \brief Loader-side aggregate: spec + native id + NUMA node for
 * one CPU as discovered by \ref cpu_loader.
 * \internal
 */
struct cpu_info
{
    cpu_spec spec;
    cpu_identifier_wrapper id;
    numa_type numa;

    inline cpu_info(const cpu_spec &s,
                    const cpu_identifier_type &i,
                    const numa_type &n)
        : spec(s), id(i), numa(n)
    {
    }
};

/*!
 * \brief Vector of \ref cpu_info populated by \ref cpu_loader.
 * \internal
 */
typedef std::vector< cpu_info > cpu_loader_vector_type;

/*!
 * \brief CPU enumeration functor.
 * \internal
 *
 * Walks \c /sys via \ref sysfs_reader to enumerate processing
 * units, then assigns dense (socket, core, processing_unit)
 * coordinates so the kernel-side topology numbers don't leak into
 * \ref cpuaff::cpu_spec.
 */
struct cpu_loader
{
    /*!
     * \brief Enumerate the system's CPUs into \p v.
     *
     * \param v destination vector; cleared on entry.
     * \return \c true if at least one CPU was discovered, \c false
     *         otherwise.
     */
    inline bool operator()(cpu_loader_vector_type &v)
    {
        v.clear();

        std::vector< sysfs_reader::pu > pus;
        std::map< int32_t, std::map< int32_t, int32_t > > cores_by_socket;
        std::map< int32_t, std::map< int32_t, std::vector< int32_t > > >
            pus_by_socket_by_core;

        if (sysfs_reader::load_cpus(pus))
        {
            std::vector< sysfs_reader::pu >::iterator pu = pus.begin();
            std::vector< sysfs_reader::pu >::iterator puend = pus.end();

            for (; pu != puend; ++pu)
            {
                core_type core;

                std::map< int32_t, int32_t >::iterator i =
                    cores_by_socket[pu->socket].find(pu->core);

                if (i != cores_by_socket[pu->socket].end())
                {
                    core = core_type(i->second);
                }
                else
                {
                    core = core_type(cores_by_socket[pu->socket].size());
                    cores_by_socket[pu->socket][pu->core] = int32_t(core);
                }

                processing_unit_type pu_id;

                pu_id = pus_by_socket_by_core[pu->socket][core].size();
                pus_by_socket_by_core[pu->socket][core].push_back(0);

                v.push_back(cpu_info(
                    cpu_spec(socket_type(pu->socket), core_type(core),
                             processing_unit_type(pu_id)),
                    cpu_identifier_type(pu->native), numa_type(pu->node)));
            }
        }

        return !!v.size();
    }
};

/*!
 * \brief Decide how large to size a dynamic \c cpu_set_t.
 * \internal
 *
 * \c _SC_NPROCESSORS_CONF is the kernel's count of *configured*
 * (not online) CPUs and matches what \c sched_*affinity needs to
 * address. \c CPU_SETSIZE (1024 on glibc) is used as a defensive
 * lower bound so the mask always covers at least the static
 * cpu_set_t size.
 *
 * \return CPU count suitable for \c CPU_ALLOC / \c CPU_ALLOC_SIZE.
 */
inline long detect_ncpus_for_affinity()
{
    long n = sysconf(_SC_NPROCESSORS_CONF);
    if (n < CPU_SETSIZE)
        n = CPU_SETSIZE;
    return n;
}

/*!
 * \brief Affinity reader functor.
 * \internal
 *
 * Wraps \c sched_getaffinity / \c pthread_getaffinity_np. Exposes
 * a legacy bool-returning \c operator() (preserved for v1.x
 * source-compat) and a modern \c query() pair returning
 * \c std::error_code. The error-returning form is what
 * \ref cpuaff::impl::basic_affinity_manager 's \c try_* API uses.
 */
struct get_affinity
{
    /*!
     * \brief Legacy bool API for the calling thread.
     *
     * \warning Preserved verbatim from alpha.3 for source-compat
     * with v1.x consumers; new code should prefer \ref query().
     *
     * \param cpus destination set, populated on success.
     * \return \c true on success, \c false on any failure.
     */
    inline bool operator()(std::set< cpu_identifier_wrapper > &cpus) const
    {
        return !query(cpus);
    }

    /*!
     * \brief Read the calling thread's affinity into \p cpus.
     *
     * \param cpus destination set; populated on success and left
     *        unchanged on failure.
     * \return default-constructed \c std::error_code on success;
     *         a cpuaff-tagged code on failure (e.g.
     *         \ref cpuaff::affinity_errc::out_of_memory if
     *         \c CPU_ALLOC fails).
     */
    inline std::error_code query(
        std::set< cpu_identifier_wrapper > &cpus) const noexcept
    {
        const long ncpus = detect_ncpus_for_affinity();
        const size_t mask_size = CPU_ALLOC_SIZE(ncpus);
        cpu_set_t *mask = CPU_ALLOC(ncpus);
        if (mask == nullptr)
        {
            return cpuaff::make_error_code(
                cpuaff::affinity_errc::out_of_memory);
        }

        std::error_code ec;
        if (sched_getaffinity(0, mask_size, mask) != 0)
        {
            ec = cpuaff::error_from_errno(errno);
        }
        else
        {
            for (long i = 0; i < ncpus; ++i)
            {
                if (CPU_ISSET_S(i, mask_size, mask))
                {
                    cpus.insert(cpu_identifier_wrapper(
                        static_cast< cpu_identifier_type >(i)));
                }
            }
        }
        CPU_FREE(mask);
        return ec;
    }

    /*!
     * \brief Read the affinity of the given pthread into \p cpus.
     *
     * Uses \c pthread_getaffinity_np instead of
     * \c sched_getaffinity so the caller can target a specific
     * pthread without needing its kernel TID.
     *
     * \param t target pthread.
     * \param cpus destination set; populated on success and left
     *        unchanged on failure.
     * \return default-constructed \c std::error_code on success;
     *         a cpuaff-tagged code on failure.
     */
    inline std::error_code query(
        pthread_t t, std::set< cpu_identifier_wrapper > &cpus) const noexcept
    {
        const long ncpus = detect_ncpus_for_affinity();
        const size_t mask_size = CPU_ALLOC_SIZE(ncpus);
        cpu_set_t *mask = CPU_ALLOC(ncpus);
        if (mask == nullptr)
        {
            return cpuaff::make_error_code(
                cpuaff::affinity_errc::out_of_memory);
        }

        std::error_code ec;
        const int rc = pthread_getaffinity_np(t, mask_size, mask);
        if (rc != 0)
        {
            ec = cpuaff::error_from_errno(rc);
        }
        else
        {
            for (long i = 0; i < ncpus; ++i)
            {
                if (CPU_ISSET_S(i, mask_size, mask))
                {
                    cpus.insert(cpu_identifier_wrapper(
                        static_cast< cpu_identifier_type >(i)));
                }
            }
        }
        CPU_FREE(mask);
        return ec;
    }
};

/*!
 * \brief Affinity writer functor.
 * \internal
 *
 * Wraps \c sched_setaffinity / \c pthread_setaffinity_np. Exposes
 * the same legacy / modern split as \ref get_affinity.
 */
struct set_affinity
{
    /*!
     * \brief Legacy bool API for the calling thread.
     *
     * \warning Preserved verbatim from alpha.3 for source-compat
     * with v1.x consumers; new code should prefer \ref apply().
     *
     * \param cpus mask to apply.
     * \return \c true on success, \c false on any failure.
     */
    inline bool operator()(const std::set< cpu_identifier_wrapper > &cpus) const
    {
        return !apply(cpus);
    }

    /*!
     * \brief Apply \p cpus as the calling thread's affinity.
     *
     * \param cpus mask to apply.
     * \return default-constructed \c std::error_code on success;
     *         a cpuaff-tagged code on failure.
     */
    inline std::error_code apply(
        const std::set< cpu_identifier_wrapper > &cpus) const noexcept
    {
        cpu_set_t *mask = nullptr;
        size_t mask_size = 0;
        const std::error_code alloc_ec = build_mask(cpus, mask, mask_size);
        if (alloc_ec)
            return alloc_ec;

        std::error_code ec;
        if (sched_setaffinity(0, mask_size, mask) != 0)
        {
            ec = cpuaff::error_from_errno(errno);
        }
        CPU_FREE(mask);
        return ec;
    }

    /*!
     * \brief Apply \p cpus as the affinity of the given pthread.
     *
     * Uses \c pthread_setaffinity_np instead of
     * \c sched_setaffinity so the caller can target a specific
     * pthread without needing its kernel TID.
     *
     * \param t target pthread.
     * \param cpus mask to apply.
     * \return default-constructed \c std::error_code on success;
     *         a cpuaff-tagged code on failure.
     */
    inline std::error_code apply(
        pthread_t t,
        const std::set< cpu_identifier_wrapper > &cpus) const noexcept
    {
        cpu_set_t *mask = nullptr;
        size_t mask_size = 0;
        const std::error_code alloc_ec = build_mask(cpus, mask, mask_size);
        if (alloc_ec)
            return alloc_ec;

        std::error_code ec;
        const int rc = pthread_setaffinity_np(t, mask_size, mask);
        if (rc != 0)
        {
            ec = cpuaff::error_from_errno(rc);
        }
        CPU_FREE(mask);
        return ec;
    }

   private:
    /*!
     * \brief Allocate and populate a dynamic \c cpu_set_t covering
     * every CPU id in \p cpus.
     * \internal
     *
     * The returned mask is sized to fit the highest id in
     * \p cpus (or \c _SC_NPROCESSORS_CONF, whichever is larger) and
     * populated, ready to hand to \c sched_setaffinity /
     * \c pthread_setaffinity_np. The caller owns the \c CPU_FREE.
     *
     * \param cpus CPU ids to set in the mask.
     * \param mask out-parameter receiving the allocated mask
     *        pointer (left \c nullptr on failure).
     * \param mask_size out-parameter receiving the mask byte size.
     * \return default-constructed \c std::error_code on success;
     *         \ref cpuaff::affinity_errc::out_of_memory on alloc
     *         failure.
     */
    static inline std::error_code build_mask(
        const std::set< cpu_identifier_wrapper > &cpus,
        cpu_set_t *&mask,
        size_t &mask_size) noexcept
    {
        long ncpus = detect_ncpus_for_affinity();
        for (const auto &w : cpus)
        {
            const long id = static_cast< long >(w.get());
            if (id >= ncpus)
                ncpus = id + 1;
        }

        mask_size = CPU_ALLOC_SIZE(ncpus);
        mask = CPU_ALLOC(ncpus);
        if (mask == nullptr)
        {
            return cpuaff::make_error_code(
                cpuaff::affinity_errc::out_of_memory);
        }

        CPU_ZERO_S(mask_size, mask);
        for (const auto &w : cpus)
        {
            CPU_SET_S(static_cast< long >(w.get()), mask_size, mask);
        }
        return {};
    }
};

/*!
 * \brief Linux backend trait pack consumed by
 * \ref cpuaff::basic_traits.
 * \internal
 *
 * Aggregates the loader / wrapper / get_affinity / set_affinity
 * types so they can be plugged into the \c basic_* class templates
 * via \ref cpuaff::traits.
 *
 * \note \c has_identity_native_mapping is \c true on Linux: the
 * native CPU identifier is the kernel's CPU id, which is exactly
 * what cpuaff already exposes via \ref cpu_identifier_type, so
 * \ref cpuaff::impl::basic_native_cpu_mapper can build an identity
 * map without round-tripping through \c sched_setaffinity for
 * every CPU.
 */
struct traits
{
    typedef linux_impl::cpu_identifier_type cpu_identifier_type;
    typedef cpu_identifier_wrapper cpu_identifier_wrapper_type;
    typedef cpu_loader cpu_loader_type;
    typedef linux_impl::cpu_loader_vector_type cpu_loader_vector_type;
    typedef get_affinity get_affinity_type;
    typedef set_affinity set_affinity_type;

    static constexpr bool has_identity_native_mapping = true;
};

}  // namespace linux_impl
}  // namespace impl
}  // namespace cpuaff
