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

/*!
 * \file impl/basic_cpu_set.hpp
 * \brief Ordered set of unique cpus, implemented as a private-inheritance
 * wrapper over \c std::set with the familiar surface re-exported.
 *
 * \see cpuaff::impl::basic_cpu_set
 */

#pragma once

#include "../config.hpp"
#include "basic_cpu.hpp"
#include <compare>
#include <iostream>
#include <set>
#include <type_traits>
#include <utility>

namespace cpuaff
{
namespace impl
{
/*!
 * \brief A set that can hold unique cpus.
 *
 * Privately inherits std::set and re-exports the relevant surface via
 * using-declarations. The earlier `public std::set` form was a known
 * UB hazard (std::set has a non-virtual destructor, so deleting
 * through a `std::set*` was UB for any derived holder); private
 * inheritance prevents the implicit derived-to-base pointer
 * conversion that made that misuse possible, while keeping all
 * iterators / lookup / mutation operations available with their
 * familiar names.
 *
 * \warning Do not delete a \c basic_cpu_set through a
 *          \c std::set* pointer — the v1 \c public std::set form
 *          made that possible (and UB). The private-inheritance
 *          form here statically rejects the pointer conversion.
 */
template < typename TRAITS >
class basic_cpu_set : private std::set< basic_cpu< TRAITS > >
{
   private:
    using base_t = std::set< basic_cpu< TRAITS > >;

   public:
    // ---- type aliases ----
    using typename base_t::allocator_type;
    using typename base_t::const_iterator;
    using typename base_t::const_pointer;
    using typename base_t::const_reference;
    using typename base_t::const_reverse_iterator;
    using typename base_t::difference_type;
    using typename base_t::insert_return_type;
    using typename base_t::iterator;
    using typename base_t::key_compare;
    using typename base_t::key_type;
    using typename base_t::node_type;
    using typename base_t::pointer;
    using typename base_t::reference;
    using typename base_t::reverse_iterator;
    using typename base_t::size_type;
    using typename base_t::value_compare;
    using typename base_t::value_type;

    using base_t::base_t;  // inherit constructors

    // ---- iterators ----
    using base_t::begin;
    using base_t::cbegin;
    using base_t::cend;
    using base_t::crbegin;
    using base_t::crend;
    using base_t::end;
    using base_t::rbegin;
    using base_t::rend;

    // ---- capacity ----
    using base_t::empty;
    using base_t::max_size;
    using base_t::size;

    // ---- modifiers ----
    using base_t::clear;
    using base_t::emplace;
    using base_t::emplace_hint;
    using base_t::erase;
    using base_t::extract;
    using base_t::insert;

    /*!
     * \brief Swap contents with another basic_cpu_set.
     *
     * \param other the basic_cpu_set to swap contents with.
     *
     * \note Forwards to \c std::set::swap on the base. A using-decl
     *       (\c base_t::swap) would expose a \c swap(base_t&)
     *       signature external callers can't satisfy under the
     *       private-inheritance scheme; this explicit overload
     *       takes \c basic_cpu_set& instead.
     */
    void swap(basic_cpu_set &other) noexcept(
        noexcept(std::declval< base_t & >().swap(std::declval< base_t & >())))
    {
        base_t::swap(other);
    }

    /*!
     * \brief Merge nodes from another set-like source into this set.
     *
     * \param source the set-like source to drain. May be another
     *               \c basic_cpu_set or any \c std::set-compatible
     *               container; the in-tree case casts through to
     *               the base because the derived-to-base conversion
     *               isn't visible to non-friends under private
     *               inheritance.
     *
     * \note Forwards to \c std::set::merge on the base.
     */
    template < typename Source >
    void merge(Source &source)
    {
        if constexpr (std::is_same_v< std::remove_cv_t< Source >,
                                      basic_cpu_set >)
        {
            base_t::merge(static_cast< base_t & >(source));
        }
        else
        {
            base_t::merge(source);
        }
    }

    // ---- lookup ----
    using base_t::contains;  // C++20
    using base_t::count;
    using base_t::equal_range;
    using base_t::find;
    using base_t::lower_bound;
    using base_t::upper_bound;

    // ---- observers ----
    using base_t::get_allocator;
    using base_t::key_comp;
    using base_t::value_comp;

    // ---- comparison ----
    // The synthesised operator==/operator<=> on std::set works through
    // ADL on the base type, but with private inheritance the
    // derived-to-base conversion isn't visible to non-friends. Define
    // the operators as hidden friends that explicitly cast through to
    // the base — restoring the source-compat that v1's
    // `public std::set` derivation gave for free.

    /*!
     * \brief Equality comparison; forwards to \c std::set::operator==.
     *
     * \param a left-hand operand.
     * \param b right-hand operand.
     * \return true iff both sets contain the same cpus.
     */
    friend bool operator==(const basic_cpu_set &a,
                           const basic_cpu_set &b) noexcept
    {
        return static_cast< const base_t & >(a) ==
               static_cast< const base_t & >(b);
    }

    /*!
     * \brief Three-way comparison; forwards to \c std::set::operator<=>.
     *
     * \param a left-hand operand.
     * \param b right-hand operand.
     * \return \c std::weak_ordering — basic_cpu's ordering routes
     *         through cpu_spec's \c operator< so weak_ordering is
     *         the strongest category we can offer.
     *
     * \note Explicit return type rather than \c auto — clang rejects
     *       deduced return types on inline friend definitions.
     */
    friend std::weak_ordering operator<=>(const basic_cpu_set &a,
                                          const basic_cpu_set &b) noexcept
    {
        return static_cast< const base_t & >(a) <=>
               static_cast< const base_t & >(b);
    }

    /*!
     * \brief ADL-found swap for \c std::swap(a, b) and generic swap users.
     *
     * \param a first operand.
     * \param b second operand.
     */
    friend void swap(basic_cpu_set &a,
                     basic_cpu_set &b) noexcept(noexcept(a.swap(b)))
    {
        a.swap(b);
    }

    // ---- streaming ----
    /*!
     * \brief Stream operator: emits a comma-separated list of cpus.
     *
     * \param s the destination stream.
     * \param obj the cpu set to print.
     * \return the stream \p s after printing.
     */
    friend std::ostream &operator<<(std::ostream &s, const basic_cpu_set &obj)
    {
        bool first = true;
        for (const auto &cpu : obj)
        {
            if (!first)
                s << ", ";
            first = false;
            s << cpu;
        }
        return s;
    }
};

}  // namespace impl
}  // namespace cpuaff
