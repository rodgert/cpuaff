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
 * \file cpuaff.hpp
 * \brief Umbrella header — single include surface for cpuaff.
 *
 * Pulls in \ref config.hpp (Linux-only platform check + the
 * \ref cpuaff::traits assembly), the \ref cpuaff::expected /
 * \ref cpuaff::affinity_errc diagnostic surface, \ref cpuaff::cpu_spec, and
 * the user-facing \c basic_* class templates from \c impl/, then
 * binds them against \ref cpuaff::traits to expose the public type
 * names (\ref cpuaff::affinity_manager, \ref cpuaff::cpu, etc.).
 *
 * Application code should include this header rather than the
 * individual \c impl/ headers so the trait-binding stays consistent.
 */

#pragma once

#include "config.hpp"
#include "detail/expected.hpp"
#include "error.hpp"

#include "cpu_spec.hpp"
#include "impl/basic_affinity_manager.hpp"
#include "impl/basic_affinity_stack.hpp"
#include "impl/basic_cpu.hpp"
#include "impl/basic_cpu_set.hpp"
#include "impl/basic_native_cpu_mapper.hpp"
#include "impl/basic_round_robin_allocator.hpp"

/*!
 * \brief Namespace for all cpuaff functionality.
 */
namespace cpuaff
{
/*!
 * \brief RAII-friendly affinity save/restore stack.
 *
 * Wraps an \ref affinity_manager and records affinities as they are
 * read so they can be restored later (push / pop semantics). Useful
 * for short-lived affinity changes that need to leave the thread's
 * original affinity intact.
 *
 * \see impl::basic_affinity_stack
 */
typedef impl::basic_affinity_stack< traits > affinity_stack;

/*!
 * \brief Representation of a single CPU on the system.
 *
 * Carries everything needed to address the CPU for affinity calls
 * (the kernel-side identifier) along with topology metadata
 * (socket / core / processing_unit / numa node) so callers can make
 * structure-aware pinning decisions.
 *
 * \see impl::basic_cpu
 */
typedef impl::basic_cpu< traits > cpu;

/*!
 * \brief Top-level entry point for the cpuaff API.
 *
 * Enumerates the system's CPUs at construction time and exposes
 * get/set affinity, lookup-by-spec, and classification helpers.
 * Both the legacy bool-returning API and the modern
 * \c try_*-prefixed \ref expected -returning API live here.
 *
 * \see impl::basic_affinity_manager
 */
typedef impl::basic_affinity_manager< traits > affinity_manager;

/*!
 * \brief Bridge between native (kernel) CPU identifiers and cpuaff
 * \ref cpu objects.
 *
 * On Linux the native identifier is the kernel CPU id and an
 * identity mapping is used; the type is preserved as a public
 * surface for source-compat with the v1.x hwloc-backed builds.
 *
 * \see impl::basic_native_cpu_mapper
 */
typedef impl::basic_native_cpu_mapper< traits > native_cpu_mapper;

/*!
 * \brief Comparable wrapper around the platform's native CPU
 * identifier.
 *
 * Comparable with \c < so it can be used as a key in associative
 * containers.
 */
typedef traits::native_cpu_wrapper_type native_cpu_wrapper;

/*!
 * \brief Set holding unique \ref cpu values.
 *
 * \see impl::basic_cpu_set
 */
typedef impl::basic_cpu_set< traits > cpu_set;

/*!
 * \brief Set holding unique \ref cpu_spec values.
 */
typedef std::set< cpu_spec > cpu_spec_set;

/*!
 * \brief Round-robin allocator over a set of CPUs.
 *
 * Hands out CPUs one at a time, ordered so consecutive allocations
 * land on different cores where the topology allows.
 *
 * \see impl::basic_round_robin_allocator
 */
typedef impl::basic_round_robin_allocator< traits > round_robin_allocator;
}  // namespace cpuaff
