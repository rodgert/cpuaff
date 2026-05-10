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

#include <charconv>
#include <set>
#include <stdint.h>
#include <string>
#include <string_view>
#include <system_error>

namespace cpuaff
{
namespace impl
{
namespace linux_impl
{
namespace set_reader
{
/*
 * Parse a Linux kernel cpulist (RFC 1374-ish): comma-separated numbers
 * and dash-ranges, e.g. "0-3,5,7-11". Returns false if any chunk is
 * malformed; the result set is cleared on entry, so a partial parse
 * leaves it empty. Whitespace within chunks is rejected (the kernel
 * never produces it).
 */
inline bool read_int_set(std::set< int32_t > &result, std::string_view input)
{
    result.clear();

    while (!input.empty())
    {
        auto comma = input.find(',');
        std::string_view chunk =
            (comma == std::string_view::npos) ? input : input.substr(0, comma);

        // Trim only trailing newline / whitespace at the very end of the
        // input — sysfs cpulist files end with a '\n' that getline strips,
        // but be defensive.
        while (!chunk.empty()
               && (chunk.back() == '\n' || chunk.back() == '\r'
                   || chunk.back() == ' ' || chunk.back() == '\t'))
        {
            chunk.remove_suffix(1);
        }

        if (chunk.empty())
        {
            // Tolerate an empty trailing chunk (e.g. trailing comma).
        }
        else if (auto dash = chunk.find('-'); dash == std::string_view::npos)
        {
            int32_t value = 0;
            const char *first = chunk.data();
            const char *last = first + chunk.size();
            auto [p, ec] = std::from_chars(first, last, value);
            if (ec != std::errc{} || p != last)
            {
                result.clear();
                return false;
            }
            result.insert(value);
        }
        else
        {
            std::string_view lhs = chunk.substr(0, dash);
            std::string_view rhs = chunk.substr(dash + 1);

            int32_t begin = 0;
            int32_t end = 0;

            auto [p1, ec1] =
                std::from_chars(lhs.data(), lhs.data() + lhs.size(), begin);
            auto [p2, ec2] =
                std::from_chars(rhs.data(), rhs.data() + rhs.size(), end);

            if (ec1 != std::errc{} || ec2 != std::errc{}
                || p1 != lhs.data() + lhs.size()
                || p2 != rhs.data() + rhs.size() || begin > end)
            {
                result.clear();
                return false;
            }

            for (int32_t j = begin; j <= end; ++j)
            {
                result.insert(j);
            }
        }

        if (comma == std::string_view::npos) break;
        input.remove_prefix(comma + 1);
    }

    return true;
}

// Backwards-compatible overload — the previous signature took
// `const std::string&`, and the implicit conversion to string_view
// covers existing callers.
inline bool read_int_set(std::set< int32_t > &result, const std::string &str)
{
    return read_int_set(result, std::string_view(str));
}
}  // namespace set_reader
}  // namespace linux_impl
}  // namespace impl
}  // namespace cpuaff
