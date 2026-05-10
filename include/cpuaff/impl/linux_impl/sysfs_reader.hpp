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
 * \file sysfs_reader.hpp
 * \brief CPU topology discovery via \c /sys.
 * \internal
 *
 * Free functions in the \c sysfs_reader namespace walk
 * \c /sys/devices/system/node and \c /sys/devices/system/cpu to
 * enumerate processing units and their socket / core / NUMA-node
 * attributes. Used by
 * \ref cpuaff::impl::linux_impl::cpu_loader; not part of the
 * public API.
 */

#pragma once
#include "../../cpu_spec.hpp"
#include "set_reader.hpp"
#include <charconv>
#include <dirent.h>
#include <fstream>
#include <set>
#include <sstream>
#include <stdint.h>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace cpuaff
{
namespace impl
{
namespace linux_impl
{
namespace sysfs_reader
{
/*!
 * \brief Aggregate describing one processing unit discovered in
 * \c /sys.
 * \internal
 */
struct pu
{
    int32_t node;
    int32_t socket;
    int32_t core;
    int32_t native;
};

/*! \brief Read a kernel cpulist file into \p set. \internal */
inline bool read_list(std::set< int32_t > &set, const std::string &_file)
{
    set.clear();
    std::ifstream infile(_file.c_str());

    if (infile.good())
    {
        std::string line;
        std::getline(infile, line);

        set_reader::read_int_set(set, line);
        infile.close();
    }

    return !!set.size();
}

/*! \brief Read the set of online NUMA nodes. \internal */
inline bool read_nodes(std::set< int32_t > &nodes)
{
    return read_list(nodes, "/sys/devices/system/node/online");
}

/*! \brief Read the cpulist for \p node. \internal */
inline bool read_cpus(std::set< int32_t > &cpus, int32_t node)
{
    std::ostringstream buf;
    buf << "/sys/devices/system/node/node" << node << "/cpulist";
    return read_list(cpus, buf.str());
}

/*! \brief Read the system-wide online cpu list. \internal */
inline bool read_cpus(std::set< int32_t > &cpus)
{
    return read_list(cpus, "/sys/devices/system/cpu/online");
}

/*! \brief Read the physical_package_id for \p cpu. \internal */
inline int32_t read_socket(int32_t cpu)
{
    std::ostringstream buf;
    std::set< int32_t > sockets;

    buf << "/sys/devices/system/cpu/cpu" << cpu
        << "/topology/physical_package_id";

    read_list(sockets, buf.str());

    if (sockets.size() == 1)
    {
        return *sockets.begin();
    }
    else
    {
        return -1;
    }
}

/*! \brief Read the physical_package_id for \p cpu under \p node. \internal */
inline int32_t read_socket(int32_t node, int32_t cpu)
{
    std::ostringstream buf;
    std::set< int32_t > sockets;

    buf << "/sys/devices/system/node/node" << node << "/cpu" << cpu
        << "/topology/physical_package_id";

    read_list(sockets, buf.str());

    if (sockets.size() == 1)
    {
        return *sockets.begin();
    }
    else
    {
        return -1;
    }
}

/*! \brief Read the core_id for \p cpu. \internal */
inline int32_t read_core(int32_t cpu)
{
    std::ostringstream buf;
    std::set< int32_t > cores;

    buf << "/sys/devices/system/cpu/cpu" << cpu << "/topology/core_id";

    read_list(cores, buf.str());

    if (cores.size() == 1)
    {
        return *cores.begin();
    }
    else
    {
        return -1;
    }
}

/*! \brief Read the core_id for \p cpu under \p node. \internal */
inline int32_t read_core(int32_t node, int32_t cpu)
{
    std::ostringstream buf;
    std::set< int32_t > cores;

    buf << "/sys/devices/system/node/node" << node << "/cpu" << cpu
        << "/topology/core_id";

    read_list(cores, buf.str());

    if (cores.size() == 1)
    {
        return *cores.begin();
    }
    else
    {
        return -1;
    }
}

/*! \brief Append one cpu's topology (no NUMA node) to \p pus. \internal */
inline bool read_cpu(std::vector< pu > &pus, int32_t cpu)
{
    int32_t socket = read_socket(cpu);
    int32_t core = read_core(cpu);

    pu u;
    u.node = -1;
    u.native = cpu;
    u.socket = (socket == -1) ? 0 : socket;
    u.core = (core == -1) ? 0 : core;
    pus.push_back(u);
    return true;
}

/*! \brief Append one cpu's topology under \p node to \p pus. \internal */
inline bool read_cpu(std::vector< pu > &pus, int32_t node, int32_t cpu)
{
    int32_t socket = read_socket(node, cpu);
    int32_t core = read_core(node, cpu);

    if (socket >= 0 && core >= 0)
    {
        pu u;
        u.node = node;
        u.native = cpu;
        u.socket = socket;
        u.core = core;
        pus.push_back(u);
        return true;
    }

    return false;
}

/*! \brief Append every cpu under \p node to \p pus. \internal */
inline bool read_node(std::vector< pu > &pus, int32_t node)
{
    std::set< int32_t > cpus;
    read_cpus(cpus, node);

    std::set< int32_t >::iterator i = cpus.begin();
    std::set< int32_t >::iterator iend = cpus.end();

    for (; i != iend; ++i)
    {
        read_cpu(pus, node, *i);
    }

    return !!pus.size();
}

/*!
 * \brief Enumerate every processing unit on the system into \p pus.
 * \internal
 *
 * Tries NUMA-aware enumeration first (online nodes -> per-node
 * cpulist); falls back to the global online cpu list, then to a
 * directory walk of \c /sys/devices/system/cpu/cpuN.
 */
inline bool load_cpus(std::vector< pu > &pus)
{
    pus.clear();

    std::set< int32_t > nodes;

    if (read_nodes(nodes))
    {
        std::set< int32_t >::iterator i = nodes.begin();
        std::set< int32_t >::iterator iend = nodes.end();

        for (; i != iend; ++i)
        {
            read_node(pus, *i);
        }
    }
    else
    {
        // we don't have nodes to read, so let's just set node to
        // -1 for everything and read the cpus
        std::set< int32_t > cpus;

        if (read_cpus(cpus))
        {
            std::set< int32_t >::iterator i = cpus.begin();
            std::set< int32_t >::iterator iend = cpus.end();

            for (; i != iend; ++i)
            {
                read_cpu(pus, *i);
            }
        }
        else
        {
            // Last-resort fallback: enumerate /sys/devices/system/cpu/cpuN
            // entries directly. Strictly require "cpu<digits>" so we
            // don't pick up cpufreq, cpuidle, or other sibling entries
            // that the previous implementation accepted as phantom
            // CPU 0 via atoi("freq") == 0. Also closes the DIR* on
            // every exit path.
            DIR *dir = opendir("/sys/devices/system/cpu");
            if (dir != nullptr)
            {
                struct dirent *ent;
                while ((ent = readdir(dir)) != nullptr)
                {
                    std::string_view name(ent->d_name);
                    if (name.size() < 4
                        || name.substr(0, 3) != std::string_view("cpu"))
                    {
                        continue;
                    }
                    std::string_view suffix = name.substr(3);

                    int32_t cpu = -1;
                    auto [p, ec] = std::from_chars(
                        suffix.data(), suffix.data() + suffix.size(), cpu);
                    if (ec != std::errc{}
                        || p != suffix.data() + suffix.size() || cpu < 0)
                    {
                        continue;
                    }

                    read_cpu(pus, cpu);
                }
                closedir(dir);
            }
        }
    }

    return !!pus.size();
}
}  // namespace sysfs_reader

}  // namespace linux_impl
}  // namespace impl
}  // namespace cpuaff
