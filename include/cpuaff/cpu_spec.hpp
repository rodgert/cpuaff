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
 * \file cpu_spec.hpp
 * \brief Topological CPU coordinate (socket, core, processing unit).
 *
 * \ref cpuaff::cpu_spec is the structural address of a CPU on the
 * system, distinct from the kernel-side native identifier carried
 * by \ref cpuaff::cpu. It is what callers use to ask "the second
 * hardware thread of the third core on socket 0" without having to
 * know which kernel CPU id that resolves to on this particular
 * machine.
 *
 * \see cpuaff::cpu
 */

#pragma once

#include "fwd.hpp"
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>

namespace cpuaff
{
/*!
 * \brief Structural address of a CPU as (socket, core,
 * processing_unit).
 *
 * For instance, the first processing unit on the first core of the
 * first socket is represented as socket = 0, core = 0,
 * processing_unit = 0. Comparable with \c < and \c == so it can be
 * used as a key in associative containers.
 *
 * \see cpuaff::cpu
 */
class cpu_spec
{
   public:
    /*!
     * \brief Default-construct an "invalid" cpu_spec
     * (socket = -1, core = -1, processing_unit = -1).
     *
     * \note No real CPU has this triple, so the default value is
     * safe as a sentinel.
     */
    inline cpu_spec() : socket_(-1), core_(-1), processing_unit_(-1) {}

    /*!
     * \brief Construct with explicit topology coordinates.
     *
     * \param s zero-based socket.
     * \param c zero-based core within the socket.
     * \param h zero-based processing unit (hardware thread) within
     *        the core.
     */
    inline cpu_spec(const socket_type &s,
                    const core_type &c,
                    const processing_unit_type &h)
        : socket_(s), core_(c), processing_unit_(h)
    {
    }

   public:
    /*!
     * \brief Parse a "socket,core,processing_unit" triplet into a
     * cpu_spec.
     *
     * \param rhs the string to parse.
     * \return the parsed cpu_spec; on malformed input the result
     *         carries whatever the underlying \c istream extraction
     *         was able to fill (may be the default-constructed
     *         sentinel triple).
     */
    static inline cpu_spec parse(const std::string &rhs)
    {
        std::istringstream buf(rhs);
        cpu_spec spec;
        buf >> spec;
        return spec;
    }

    /*!
     * \brief Read the zero-based socket identifier.
     * \return the socket identifier.
     */
    const inline socket_type &socket() const { return socket_; }

    /*!
     * \brief Read the zero-based core identifier (within the socket).
     * \return the core identifier.
     */
    const inline core_type &core() const { return core_; }

    /*!
     * \brief Read the zero-based processing unit identifier (within
     * the core).
     * \return the processing unit identifier.
     */
    const inline processing_unit_type &processing_unit() const
    {
        return processing_unit_;
    }

    /*!
     * \brief Set the zero-based socket identifier.
     * \param socket new socket identifier.
     */
    inline void socket(const socket_type &socket) { socket_ = socket; }

    /*!
     * \brief Set the zero-based core identifier.
     * \param core new core identifier.
     */
    inline void core(const core_type &core) { core_ = core; }

    /*!
     * \brief Set the zero-based processing unit identifier.
     * \param processing_unit new processing unit identifier.
     */
    inline void processing_unit(const processing_unit_type &processing_unit)
    {
        processing_unit_ = processing_unit;
    }

    /*!
     * \brief Lexicographic less-than over (socket, core,
     * processing_unit).
     *
     * Lets cpu_spec be a key in STL associative containers.
     *
     * \param rhs the cpu_spec to compare against.
     * \return \c true if \c *this orders before \c rhs, \c false
     *         otherwise.
     */
    inline bool operator<(const cpu_spec &rhs) const
    {
        if (socket_ < rhs.socket_)
        {
            return true;
        }
        else if (rhs.socket_ < socket_)
        {
            return false;
        }
        else
        {
            if (core_ < rhs.core_)
            {
                return true;
            }
            else if (rhs.core_ < core_)
            {
                return false;
            }
            else
            {
                return processing_unit_ < rhs.processing_unit_;
            }
        }
    }

    /*!
     * \brief Component-wise equality.
     *
     * \param rhs the cpu_spec to compare to.
     * \return \c true if both cpu_specs share socket, core, and
     *         processing unit; \c false otherwise.
     */
    inline bool operator==(const cpu_spec &rhs) const
    {
        return (socket_ == rhs.socket_ && core_ == rhs.core_ &&
                processing_unit_ == rhs.processing_unit_);
    }

    /*!
     * \brief Extract a comma-separated triplet
     * "socket,core,processing_unit" from a stream into \p rhs.
     *
     * The separator between fields is consumed via \c s.get() and
     * isn't required to be specifically a comma — any single
     * delimiter character works.
     *
     * \param s input stream.
     * \param rhs cpu_spec to populate.
     * \return reference to \p s.
     */
    friend inline std::istream &operator>>(std::istream &s, cpu_spec &rhs)
    {
        s >> rhs.socket_;
        s.get();
        s >> rhs.core_;
        s.get();
        s >> rhs.processing_unit_;

        return s;
    }

    /*!
     * \brief Insert a "socket,core,processing_unit" triplet into a
     * stream.
     *
     * \param s output stream.
     * \param rhs cpu_spec to format.
     * \return reference to \p s.
     */
    friend inline std::ostream &operator<<(std::ostream &s, const cpu_spec &rhs)
    {
        s << rhs.socket_ << "," << rhs.core_ << "," << rhs.processing_unit_;
        return s;
    }

   private:
    socket_type socket_;
    core_type core_;
    processing_unit_type processing_unit_;
};

}  // namespace cpuaff
