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

#if !defined(__linux__)
#error \
    "cpuaff v2 is Linux-only. Earlier releases supported macOS/BSD/Windows via the now-removed null_impl and hwloc_impl backends; for non-Linux platforms use v1.0.6-htaa.1 or earlier."
#endif

#include "fwd.hpp"
#include "impl/linux_impl/linux.hpp"

namespace cpuaff
{
template < typename LOADER_TRAITS, typename NATIVE_TRAITS >
struct basic_traits
{
    typedef typename LOADER_TRAITS::cpu_identifier_type cpu_identifier_type;
    typedef typename LOADER_TRAITS::cpu_identifier_wrapper_type
        cpu_identifier_wrapper_type;
    typedef typename LOADER_TRAITS::cpu_loader_type cpu_loader_type;
    typedef
        typename LOADER_TRAITS::cpu_loader_vector_type cpu_loader_vector_type;
    typedef typename LOADER_TRAITS::get_affinity_type get_affinity_type;
    typedef typename LOADER_TRAITS::set_affinity_type set_affinity_type;
    typedef typename NATIVE_TRAITS::cpu_identifier_type native_cpu_type;
    typedef typename NATIVE_TRAITS::cpu_identifier_wrapper_type
        native_cpu_wrapper_type;
    typedef typename NATIVE_TRAITS::get_affinity_type native_get_affinity_type;
};

typedef basic_traits< impl::linux_impl::traits, impl::linux_impl::traits >
    traits;
}  // namespace cpuaff
