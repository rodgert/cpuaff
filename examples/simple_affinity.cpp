/* Copyright (c) 2015, Daniel C. Dillon
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

#include <cpuaff/cpuaff.hpp>
#include <iostream>

int main(int argc, char *argv[])
{
    cpuaff::affinity_manager manager;

    if (!manager.has_cpus())
    {
        std::cerr << "cpuaff: unable to load cpus." << std::endl;
        return -1;
    }

    auto initial = manager.try_get_affinity();
    if (!initial)
    {
        std::cerr << "cpuaff: try_get_affinity failed: "
                  << initial.error().message() << std::endl;
        return -1;
    }

    std::cout << "Initial Affinity:" << std::endl;
    for (const auto &cpu : *initial)
    {
        std::cout << "  " << cpu << std::endl;
    }
    std::cout << std::endl;

    // Set the affinity to all the processing units on the first core.
    cpuaff::cpu_set core_0;
    manager.get_cpus_by_core(core_0, 0);

    if (auto r = manager.try_set_affinity(core_0); !r)
    {
        std::cerr << "cpuaff: try_set_affinity failed: " << r.error().message()
                  << std::endl;
        return -1;
    }

    auto after = manager.try_get_affinity();
    if (!after)
    {
        std::cerr << "cpuaff: try_get_affinity failed: "
                  << after.error().message() << std::endl;
        return -1;
    }

    std::cout << "Affinity After Calling try_set_affinity():" << std::endl;
    for (const auto &cpu : *after)
    {
        std::cout << "  " << cpu << std::endl;
    }

    return 0;
}
