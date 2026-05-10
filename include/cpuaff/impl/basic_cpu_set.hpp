/* Copyright (c) 2015-2017, Daniel C. Dillon
 * Modifications copyright (c) 2026 Thomas Rodgers
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
#include "basic_cpu.hpp"
#include <iostream>
#include <set>

namespace cpuaff
{
namespace impl
{
/*!
 * A set that can hold unique cpus.
 *
 * Privately inherits std::set and re-exports the relevant surface via
 * using-declarations. The earlier `public std::set` form was a known
 * UB hazard (std::set has a non-virtual destructor, so deleting
 * through a `std::set*` was UB for any derived holder); private
 * inheritance prevents the implicit derived-to-base pointer
 * conversion that made that misuse possible, while keeping all
 * iterators / lookup / mutation operations available with their
 * familiar names.
 */
template < typename TRAITS >
class basic_cpu_set : private std::set< basic_cpu< TRAITS > >
{
   private:
    using base_t = std::set< basic_cpu< TRAITS > >;

   public:
    using typename base_t::const_iterator;
    using typename base_t::const_pointer;
    using typename base_t::const_reference;
    using typename base_t::const_reverse_iterator;
    using typename base_t::difference_type;
    using typename base_t::iterator;
    using typename base_t::key_type;
    using typename base_t::pointer;
    using typename base_t::reference;
    using typename base_t::reverse_iterator;
    using typename base_t::size_type;
    using typename base_t::value_type;

    using base_t::base_t;  // inherit constructors

    using base_t::begin;
    using base_t::cbegin;
    using base_t::cend;
    using base_t::crbegin;
    using base_t::crend;
    using base_t::end;
    using base_t::rbegin;
    using base_t::rend;

    using base_t::empty;
    using base_t::max_size;
    using base_t::size;

    using base_t::clear;
    using base_t::emplace;
    using base_t::emplace_hint;
    using base_t::erase;
    using base_t::insert;
    using base_t::swap;

    using base_t::contains;  // C++20
    using base_t::count;
    using base_t::equal_range;
    using base_t::find;
    using base_t::lower_bound;
    using base_t::upper_bound;

    friend std::ostream &operator<<(std::ostream &s, const basic_cpu_set &obj)
    {
        bool first = true;
        for (const auto &cpu : obj)
        {
            if (!first) s << ", ";
            first = false;
            s << cpu;
        }
        return s;
    }
};

}  // namespace impl
}  // namespace cpuaff
