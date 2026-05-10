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
 * \file fwd.hpp
 * \brief Forward declarations and topology integer typedefs.
 *
 * Pulled in by every cpuaff header that needs to refer to a class
 * template before its full definition is available, plus the small
 * integer typedefs used to identify positions in the CPU topology.
 * Including this header rather than the full \ref cpuaff.hpp keeps
 * compile-time cost down for code that only needs to name cpuaff
 * types in declarations (e.g. function signatures).
 */

#pragma once

#include <stdint.h>

namespace cpuaff
{
class cpu_spec;

namespace impl
{
template < typename TRAITS >
class basic_cpu;

template < typename TRAITS >
class basic_affinity_manager;

template < typename TRAITS >
class basic_cpu_set;

template < typename TRAITS >
class basic_affinity_stack;
}  // namespace impl

/*!
 * \brief Zero-based socket (physical package) identifier.
 */
typedef int32_t socket_type;

/*!
 * \brief Zero-based core identifier within its socket.
 */
typedef int32_t core_type;

/*!
 * \brief Zero-based processing unit (hardware thread / hyperthread)
 * identifier within its core.
 */
typedef int32_t processing_unit_type;

/*!
 * \brief Zero-based NUMA node identifier.
 *
 * \c -1 indicates the system did not report NUMA topology and the
 * CPU is treated as belonging to a single (anonymous) node.
 */
typedef int32_t numa_type;
}  // namespace cpuaff
