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

#include "../../cpu_spec.hpp"

#include <map>
#include <set>
#include <vector>

#include <libgen.h>
#include <sched.h>
#include <unistd.h>

#include "sysfs_reader.hpp"

namespace cpuaff
{
namespace impl
{
namespace linux_impl
{
typedef int cpu_identifier_type;

class cpu_identifier_wrapper
{
   public:
    inline cpu_identifier_wrapper() : id_(-1) {}
    inline cpu_identifier_wrapper(const cpu_identifier_type &id) : id_(id) {}

   public:
    const inline cpu_identifier_type &get() const { return id_; }
    inline bool operator<(const cpu_identifier_wrapper &rhs) const
    {
        return id_ < rhs.id_;
    }

   private:
    cpu_identifier_type id_;
};

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

typedef std::vector< cpu_info > cpu_loader_vector_type;

struct cpu_loader
{
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

// Determine a cpu count to size dynamic cpu_set_t allocations.
// _SC_NPROCESSORS_CONF is the kernel's count of *configured* (not
// online) CPUs and matches what sched_*affinity needs to address.
// CPU_SETSIZE (1024 on glibc) is used as a defensive lower bound.
inline long detect_ncpus_for_affinity()
{
    long n = sysconf(_SC_NPROCESSORS_CONF);
    if (n < CPU_SETSIZE) n = CPU_SETSIZE;
    return n;
}

struct get_affinity
{
    inline bool operator()(std::set< cpu_identifier_wrapper > &cpus)
    {
        const long ncpus = detect_ncpus_for_affinity();
        const size_t mask_size = CPU_ALLOC_SIZE(ncpus);
        cpu_set_t *mask = CPU_ALLOC(ncpus);
        if (mask == nullptr) return false;

        const bool ok = (sched_getaffinity(0, mask_size, mask) == 0);
        if (ok)
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
        return ok;
    }
};

struct set_affinity
{
    inline bool operator()(const std::set< cpu_identifier_wrapper > &cpus)
    {
        long ncpus = detect_ncpus_for_affinity();
        // The cpu set may legitimately reference CPU ids beyond
        // _SC_NPROCESSORS_CONF (e.g. hot-plug setups); grow the
        // allocation to fit the largest id we're being asked to set.
        for (const auto &w : cpus)
        {
            const long id = static_cast< long >(w.get());
            if (id >= ncpus) ncpus = id + 1;
        }

        const size_t mask_size = CPU_ALLOC_SIZE(ncpus);
        cpu_set_t *mask = CPU_ALLOC(ncpus);
        if (mask == nullptr) return false;

        CPU_ZERO_S(mask_size, mask);
        for (const auto &w : cpus)
        {
            CPU_SET_S(static_cast< long >(w.get()), mask_size, mask);
        }

        const bool ok = (sched_setaffinity(0, mask_size, mask) == 0);
        CPU_FREE(mask);
        return ok;
    }
};

struct traits
{
    typedef linux_impl::cpu_identifier_type cpu_identifier_type;
    typedef cpu_identifier_wrapper cpu_identifier_wrapper_type;
    typedef cpu_loader cpu_loader_type;
    typedef linux_impl::cpu_loader_vector_type cpu_loader_vector_type;
    typedef get_affinity get_affinity_type;
    typedef set_affinity set_affinity_type;

    // On Linux the native cpu identifier is the kernel's CPU id, which
    // is exactly what cpuaff already exposes via cpu_identifier_type —
    // basic_native_cpu_mapper can therefore build an identity map
    // without round-tripping through sched_setaffinity for every cpu.
    static constexpr bool has_identity_native_mapping = true;
};

}  // namespace linux_impl
}  // namespace impl
}  // namespace cpuaff
