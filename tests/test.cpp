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

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "../include/cpuaff/cpuaff.hpp"

#include <atomic>
#include <chrono>
#include <compare>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

TEST_CASE("affinity_manager", "[affinity_manager]")
{
    cpuaff::affinity_manager manager;

    SECTION("affinity_manager member functions")
    {
        REQUIRE(manager.has_cpus());

        cpuaff::cpu first_cpu;

        // We can get the first CPU by index
        REQUIRE(manager.get_cpu_from_index(first_cpu, 0));
        REQUIRE(first_cpu.socket() >= 0);
        REQUIRE(first_cpu.core() >= 0);
        REQUIRE(first_cpu.processing_unit() >= 0);

        // We can get this same CPU by its native handle wrapper
        {
            cpuaff::cpu cpu;
            REQUIRE(manager.get_cpu_from_id(cpu, first_cpu.id()));
            REQUIRE(cpu == first_cpu);
            WARN(first_cpu << " == " << cpu);
        }

        // We can get this came CPU by its raw native id
        {
            cpuaff::cpu cpu;
            REQUIRE(manager.get_cpu_from_id(cpu, first_cpu.id().get()));
            REQUIRE(cpu == first_cpu);
            WARN(first_cpu << " == " << cpu);
        }

        // We can get this same CPU by its cpu_spec
        {
            cpuaff::cpu cpu;
            REQUIRE(manager.get_cpu_from_spec(
                cpu, cpuaff::cpu_spec(first_cpu.socket(), first_cpu.core(),
                                      first_cpu.processing_unit())));
            WARN(first_cpu << " == " << cpu);
        }

        // We can get all the CPUs
        {
            cpuaff::cpu_set cpus;
            REQUIRE(manager.get_cpus(cpus));
            REQUIRE(!cpus.empty());

            WARN("All cpus: " << cpus);
        }

        // We can get all the CPUs for the NUMA of the first CPU
        {
            cpuaff::cpu_set cpus;
            REQUIRE(manager.get_cpus_by_numa(cpus, first_cpu.numa()));
            REQUIRE(!cpus.empty());

            WARN("NUMA cpus: " << cpus);
        }

        // We can get all the CPUs for the socket of the first CPU
        {
            cpuaff::cpu_set cpus;
            REQUIRE(manager.get_cpus_by_socket(cpus, first_cpu.socket()));
            REQUIRE(!cpus.empty());

            WARN("Socket cpus: " << cpus);
        }

        // We can get all the CPUs with the same core as the first CPU
        {
            cpuaff::cpu_set cpus;
            REQUIRE(manager.get_cpus_by_core(cpus, first_cpu.core()));
            REQUIRE(!cpus.empty());

            WARN("Core cpus: " << cpus);
        }

        // We can get all the CPUs with the same processing unit as the first
        // CPU
        {
            cpuaff::cpu_set cpus;
            REQUIRE(manager.get_cpus_by_processing_unit(
                cpus, first_cpu.processing_unit()));
            REQUIRE(!cpus.empty());

            WARN("Processing uint cpus: " << cpus);
        }

        // We can get all the CPUs on the same socket and core as the first cpu
        {
            cpuaff::cpu_set cpus;
            REQUIRE(manager.get_cpus_by_socket_and_core(
                cpus, first_cpu.socket(), first_cpu.core()));
            REQUIRE(!cpus.empty());

            WARN("Socket/core cpus: " << cpus);
        }

        // We can get the affinity of the current thread
        {
            auto result = manager.try_get_affinity();
            REQUIRE(result.has_value());
            REQUIRE(!result->empty());

            WARN("Current affinity: " << *result);
        }

        // We can set the affinity of the current thread
        {
            cpuaff::cpu_set new_affinity;
            new_affinity.insert(first_cpu);

            WARN("Setting affinity to: " << new_affinity);

            REQUIRE(manager.try_set_affinity(new_affinity).has_value());

            auto result = manager.try_get_affinity();
            REQUIRE(result.has_value());

            cpuaff::cpu_set cpus = *std::move(result);

            cpuaff::cpu_set::iterator i = cpus.begin();
            cpuaff::cpu_set::iterator iend = cpus.end();
            cpuaff::cpu_set::iterator j = new_affinity.begin();
            cpuaff::cpu_set::iterator jend = new_affinity.end();

            for (; i != iend && j != jend; ++i, ++j)
            {
                REQUIRE((*i) == (*j));
            }

            WARN("Affinity is: " << cpus);
        }

        // We can set the affinity of the current thread back to all cpus
        {
            cpuaff::cpu_set new_affinity;
            REQUIRE(manager.get_cpus(new_affinity));

            WARN("Setting affinity to: " << new_affinity);

            REQUIRE(manager.try_set_affinity(new_affinity).has_value());

            auto result = manager.try_get_affinity();
            REQUIRE(result.has_value());

            cpuaff::cpu_set cpus = *std::move(result);

            cpuaff::cpu_set::iterator i = cpus.begin();
            cpuaff::cpu_set::iterator iend = cpus.end();
            cpuaff::cpu_set::iterator j = new_affinity.begin();
            cpuaff::cpu_set::iterator jend = new_affinity.end();

            for (; i != iend && j != jend; ++i, ++j)
            {
                REQUIRE((*i) == (*j));
            }

            WARN("Affinity is: " << cpus);
        }

        // We can pin the affinity of the current thread to a particular CPU
        {
            WARN("Setting affinity to: " << first_cpu);

            REQUIRE(manager.try_pin(first_cpu).has_value());

            auto result = manager.try_get_affinity();
            REQUIRE(result.has_value());

            cpuaff::cpu_set cpus = *std::move(result);

            REQUIRE(cpus.size() == 1);
            REQUIRE(*cpus.begin() == first_cpu);

            WARN("Affinty is: " << cpus);
        }
    }
}

TEST_CASE("affinity_stack", "[affinity_stack]")
{
    SECTION("affinity_stack member functions")
    {
        cpuaff::affinity_manager manager;

        REQUIRE(manager.has_cpus());

        cpuaff::affinity_stack stack(manager);

        // get the current affinity of this thread
        auto orig_result = stack.try_get_affinity();
        REQUIRE(orig_result.has_value());
        REQUIRE(!orig_result->empty());
        cpuaff::cpu_set original_affinity = *std::move(orig_result);

        // push the current affinity of this thread onto the stack
        REQUIRE(stack.try_push_affinity().has_value());

        // verify that the affinities are the same (as we didn't change them)
        {
            auto result = stack.try_get_affinity();
            REQUIRE(result.has_value());
            cpuaff::cpu_set cpus = *std::move(result);
            REQUIRE(cpus.size() == original_affinity.size());

            cpuaff::cpu_set::iterator i = cpus.begin();
            cpuaff::cpu_set::iterator iend = cpus.end();
            cpuaff::cpu_set::iterator j = original_affinity.begin();
            cpuaff::cpu_set::iterator jend = original_affinity.end();

            for (; i != iend && j != jend; ++i, ++j)
            {
                REQUIRE((*i) == (*j));
            }
        }

        cpuaff::cpu_set new_affinity;
        new_affinity.insert(*original_affinity.begin());

        // set the thread's affinity to a single core
        REQUIRE(stack.try_set_affinity(new_affinity).has_value());

        {
            auto result = stack.try_get_affinity();
            REQUIRE(result.has_value());
            cpuaff::cpu_set cpus = *std::move(result);

            cpuaff::cpu_set::iterator i = cpus.begin();
            cpuaff::cpu_set::iterator iend = cpus.end();
            cpuaff::cpu_set::iterator j = new_affinity.begin();
            cpuaff::cpu_set::iterator jend = new_affinity.end();

            for (; i != iend && j != jend; ++i, ++j)
            {
                REQUIRE((*i) == (*j));
            }
        }

        // pop the affinity off the top of the stack and test that it is the
        // same as the original affinity
        REQUIRE(stack.try_pop_affinity().has_value());
        {
            auto result = stack.try_get_affinity();
            REQUIRE(result.has_value());
            cpuaff::cpu_set cpus = *std::move(result);

            cpuaff::cpu_set::iterator i = cpus.begin();
            cpuaff::cpu_set::iterator iend = cpus.end();
            cpuaff::cpu_set::iterator j = original_affinity.begin();
            cpuaff::cpu_set::iterator jend = original_affinity.end();

            for (; i != iend && j != jend; ++i, ++j)
            {
                REQUIRE((*i) == (*j));
            }
        }
    }
}

TEST_CASE("round_robin_allocator", "[round_robin_allocator]")
{
    SECTION("round_robin_allocator member functions")
    {
        cpuaff::affinity_manager manager;

        REQUIRE(manager.has_cpus());

        cpuaff::cpu_set cpus;
        cpuaff::cpu_set allocated_cpus;
        cpuaff::cpu cpu;

        REQUIRE(manager.get_cpus(cpus));

        cpuaff::round_robin_allocator allocator(cpus);

        for (std::size_t i = 0; i < cpus.size() * 2; ++i)
        {
            auto result = allocator.try_allocate();
            REQUIRE(result.has_value());
            cpu = *std::move(result);
            bool test = cpu.socket() >= 0 && cpu.core() >= 0 &&
                        cpu.processing_unit() >= 0;
            REQUIRE(test);
        }

        auto batch = allocator.try_allocate(4u);
        REQUIRE(batch.has_value());
        allocated_cpus = *std::move(batch);

        bool test =
            allocated_cpus.size() == 4 ||
            (allocated_cpus.size() < 4 && allocated_cpus.size() == cpus.size());
        REQUIRE(test);
    }
}

TEST_CASE("native_cpu_mapper", "[native_cpu_mapper]")
{
    SECTION("native_cpu_mapper member functions")
    {
        cpuaff::affinity_manager manager;

        REQUIRE(manager.has_cpus());

        cpuaff::native_cpu_mapper mapper;

        REQUIRE(mapper.initialize(manager));
        cpuaff::cpu_set cpus;
        manager.get_cpus(cpus);

        cpuaff::cpu_set::iterator i = cpus.begin();
        cpuaff::cpu_set::iterator iend = cpus.end();

        for (; i != iend; ++i)
        {
            cpuaff::native_cpu_mapper::native_cpu_wrapper_type native;
            cpuaff::cpu cpu;

            REQUIRE(mapper.get_native_from_cpu(native, *i));
            REQUIRE(mapper.get_cpu_from_native(cpu, native));
            REQUIRE(mapper.get_cpu_from_native(cpu, native.get()));
        }
    }
}

// ---------------------------------------------------------------------
// cpu_set_compat: protects the v1 source-compat surface that beta.2
// restored after beta.1's `private std::set` refactor accidentally
// dropped it. Exercises the comparison operators (==, <=>), member
// and ADL swap, merge, and the various using-declared type aliases /
// observers. If any of these regress in a future refactor, this test
// fails to compile (the static_asserts) or fails at runtime.
// ---------------------------------------------------------------------

namespace
{
// Compile-time tripwires for the source-compat surface.
static_assert(
    std::is_same_v< decltype(std::declval< cpuaff::cpu_set & >()
                             == std::declval< cpuaff::cpu_set & >()),
                    bool >,
    "cpuaff::cpu_set must support operator== returning bool");

static_assert(
    std::is_same_v< decltype(std::declval< cpuaff::cpu_set & >()
                             <=> std::declval< cpuaff::cpu_set & >()),
                    std::weak_ordering >,
    "cpuaff::cpu_set must support operator<=> returning weak_ordering");

static_assert(std::is_default_constructible_v< cpuaff::cpu_set >);
static_assert(std::is_copy_constructible_v< cpuaff::cpu_set >);
static_assert(std::is_move_constructible_v< cpuaff::cpu_set >);
static_assert(std::is_copy_assignable_v< cpuaff::cpu_set >);
static_assert(std::is_move_assignable_v< cpuaff::cpu_set >);
static_assert(std::is_swappable_v< cpuaff::cpu_set >);

// The using-declarations on basic_cpu_set must expose these std::set
// type aliases. Failure here means a v1 consumer that named one of
// these types directly stopped compiling.
using cs_iterator = cpuaff::cpu_set::iterator;
using cs_const_iterator = cpuaff::cpu_set::const_iterator;
using cs_value_type = cpuaff::cpu_set::value_type;
using cs_size_type = cpuaff::cpu_set::size_type;
using cs_node_type = cpuaff::cpu_set::node_type;
using cs_key_compare = cpuaff::cpu_set::key_compare;
using cs_value_compare = cpuaff::cpu_set::value_compare;
using cs_allocator_type = cpuaff::cpu_set::allocator_type;
}  // namespace

TEST_CASE("cpu_set_compat", "[cpu_set][source-compat]")
{
    cpuaff::affinity_manager manager;
    REQUIRE(manager.has_cpus());

    cpuaff::cpu_set all_cpus;
    REQUIRE(manager.get_cpus(all_cpus));
    REQUIRE(!all_cpus.empty());

    SECTION("operator== / operator!=")
    {
        cpuaff::cpu_set a = all_cpus;
        cpuaff::cpu_set b = all_cpus;
        REQUIRE(a == b);
        REQUIRE_FALSE(a != b);

        b.erase(b.begin());
        REQUIRE_FALSE(a == b);
        REQUIRE(a != b);
    }

    SECTION("operator<=> yields weak_ordering")
    {
        cpuaff::cpu_set a = all_cpus;
        cpuaff::cpu_set b = all_cpus;
        REQUIRE((a <=> b) == std::weak_ordering::equivalent);

        cpuaff::cpu_set c;  // empty
        REQUIRE((a <=> c) == std::weak_ordering::greater);
        REQUIRE((c <=> a) == std::weak_ordering::less);
    }

    SECTION("member swap and ADL swap")
    {
        cpuaff::cpu_set a = all_cpus;
        cpuaff::cpu_set b;  // empty
        const auto a_size = a.size();

        a.swap(b);
        REQUIRE(a.empty());
        REQUIRE(b.size() == a_size);

        using std::swap;  // ADL
        swap(a, b);
        REQUIRE(a.size() == a_size);
        REQUIRE(b.empty());
    }

    SECTION("merge from another cpu_set")
    {
        cpuaff::cpu_set src = all_cpus;
        cpuaff::cpu_set dst;
        const auto src_size = src.size();

        dst.merge(src);
        REQUIRE(dst.size() == src_size);
        REQUIRE(src.empty());  // merged-from elements are extracted
    }

    SECTION("contains / find / count (C++20 + legacy)")
    {
        const cpuaff::cpu first = *all_cpus.begin();
        REQUIRE(all_cpus.contains(first));
        REQUIRE(all_cpus.find(first) == all_cpus.begin());
        REQUIRE(all_cpus.count(first) == 1);
    }

    SECTION("get_allocator / key_comp / value_comp accessible")
    {
        cpuaff::cpu_set a = all_cpus;
        // Just check that these compile and return reasonable shapes —
        // their identity is implementation-defined.
        auto alloc = a.get_allocator();
        auto kcomp = a.key_comp();
        auto vcomp = a.value_comp();
        (void)alloc;
        (void)kcomp;
        (void)vcomp;
    }

    SECTION("iteration via using-declared iterators")
    {
        cpuaff::cpu_set::iterator i = all_cpus.begin();
        cpuaff::cpu_set::iterator iend = all_cpus.end();
        std::size_t counted = 0;
        for (; i != iend; ++i) ++counted;
        REQUIRE(counted == all_cpus.size());
    }
}

// ---------------------------------------------------------------------
// round_robin_invariant: the v1 round_robin_allocator test only
// asserted non-negative-id; the *actual* invariant (group by
// processing_unit, hand out all PU=0 cpus before any PU=1 cpu) was
// previously untested. Phase 5 fix.
// ---------------------------------------------------------------------

TEST_CASE("round_robin_invariant", "[round_robin_allocator]")
{
    cpuaff::affinity_manager manager;
    REQUIRE(manager.has_cpus());

    cpuaff::cpu_set all_cpus;
    REQUIRE(manager.get_cpus(all_cpus));

    // Bucket by processing_unit.
    std::map< cpuaff::processing_unit_type, std::size_t > pu_count;
    for (const auto &cpu : all_cpus) ++pu_count[cpu.processing_unit()];

    SECTION("first PU's cpus are handed out before the next PU's cpus")
    {
        cpuaff::round_robin_allocator allocator(all_cpus);

        // The expected order: lowest processing_unit_type first
        // (std::map orders by key), all of those cpus, then next pu.
        auto pu_iter = pu_count.begin();
        auto pu_end = pu_count.end();

        for (; pu_iter != pu_end; ++pu_iter)
        {
            const cpuaff::processing_unit_type expected_pu = pu_iter->first;
            const std::size_t bucket_size = pu_iter->second;

            for (std::size_t i = 0; i < bucket_size; ++i)
            {
                auto result = allocator.try_allocate();
                REQUIRE(result.has_value());
                REQUIRE(result->processing_unit() == expected_pu);
            }
        }

        // After one full cycle the next allocation should return us
        // to the first PU bucket — i.e. the round-robin wraps.
        auto wrap = allocator.try_allocate();
        REQUIRE(wrap.has_value());
        REQUIRE(wrap->processing_unit() == pu_count.begin()->first);
    }

    SECTION("try_allocate(count) returns at most cpus.size() distinct cpus")
    {
        cpuaff::round_robin_allocator allocator(all_cpus);
        const auto requested = static_cast< uint32_t >(all_cpus.size() * 3);
        auto batch = allocator.try_allocate(requested);
        REQUIRE(batch.has_value());
        // The result is a cpu_set, which deduplicates — so the size
        // caps at the distinct-cpus count.
        REQUIRE(batch->size() == all_cpus.size());
    }

    SECTION("try_allocate on an empty allocator returns allocator_empty")
    {
        cpuaff::cpu_set empty;
        cpuaff::round_robin_allocator allocator(empty);
        REQUIRE(allocator.empty());
        REQUIRE(allocator.size() == 0);

        auto result = allocator.try_allocate();
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == cpuaff::affinity_errc::allocator_empty);
        REQUIRE(result.error() == std::error_code(
                                      cpuaff::affinity_errc::allocator_empty));
    }

    SECTION("try_allocate(count) on empty allocator returns empty set")
    {
        cpuaff::cpu_set empty;
        cpuaff::round_robin_allocator allocator(empty);
        auto result = allocator.try_allocate(4u);
        REQUIRE(result.has_value());
        REQUIRE(result->empty());
    }
}

// ---------------------------------------------------------------------
// pthread_t overloads: exercise the Phase 4 try_*_affinity / try_pin
// overloads that target an arbitrary pthread_t (rather than the
// calling thread). The pattern: spawn a worker that blocks on a
// condition variable, drive its affinity from the main thread via
// its native_handle(), then signal it to exit.
// ---------------------------------------------------------------------

namespace
{
struct worker_block
{
    std::mutex m;
    std::condition_variable cv;
    bool released = false;
    std::atomic< bool > started{ false };
};

inline void worker_main(worker_block &block)
{
    block.started.store(true, std::memory_order_release);
    std::unique_lock< std::mutex > lock(block.m);
    block.cv.wait(lock, [&block] { return block.released; });
}
}  // namespace

TEST_CASE("pthread_t_overloads", "[affinity_manager][pthread]")
{
    cpuaff::affinity_manager manager;
    REQUIRE(manager.has_cpus());

    cpuaff::cpu first_cpu;
    REQUIRE(manager.get_cpu_from_index(first_cpu, 0));

    worker_block block;
    std::thread worker(worker_main, std::ref(block));

    // Wait for the worker to actually be running before we start
    // poking its affinity. Otherwise pthread_*affinity_np could
    // race with thread startup.
    while (!block.started.load(std::memory_order_acquire))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    pthread_t worker_handle = worker.native_handle();

    SECTION("try_get_affinity(pthread_t) returns the worker's mask")
    {
        auto cpus = manager.try_get_affinity(worker_handle);
        REQUIRE(cpus.has_value());
        REQUIRE(!cpus->empty());
    }

    SECTION("try_set_affinity(pthread_t, cpu_set) pins the worker")
    {
        cpuaff::cpu_set target;
        target.insert(first_cpu);
        REQUIRE(manager.try_set_affinity(worker_handle, target).has_value());

        auto observed = manager.try_get_affinity(worker_handle);
        REQUIRE(observed.has_value());
        REQUIRE(observed->size() == 1);
        REQUIRE(*observed->begin() == first_cpu);
    }

    SECTION("try_pin(pthread_t, cpu) pins the worker")
    {
        REQUIRE(manager.try_pin(worker_handle, first_cpu).has_value());

        auto observed = manager.try_get_affinity(worker_handle);
        REQUIRE(observed.has_value());
        REQUIRE(observed->size() == 1);
        REQUIRE(*observed->begin() == first_cpu);
    }

    SECTION("try_get_available_cpus(pthread_t) returns the worker's mask "
            "intersected with topology")
    {
        // Restore worker to all cpus so the intersection isn't a
        // single-cpu artifact.
        cpuaff::cpu_set all_cpus;
        REQUIRE(manager.get_cpus(all_cpus));
        REQUIRE(manager.try_set_affinity(worker_handle, all_cpus)
                    .has_value());

        auto avail = manager.try_get_available_cpus(worker_handle);
        REQUIRE(avail.has_value());
        REQUIRE(!avail->empty());
        REQUIRE(avail->size() <= all_cpus.size());
    }

    // Release the worker and join.
    {
        std::lock_guard< std::mutex > lock(block.m);
        block.released = true;
    }
    block.cv.notify_all();
    worker.join();
}

// ---------------------------------------------------------------------
// error_category: exercise cpuaff::affinity_category() / affinity_errc
// equality semantics. The intent is that callers can write either
//
//   if (ec == cpuaff::affinity_errc::stack_empty) { ... }
//
// (matches via direct enum comparison through std::error_code) or
//
//   if (ec == std::errc::invalid_argument) { ... }
//
// (matches via default_error_condition mapping cpuaff errno-shaped
// values onto std::generic_category). Cpuaff-internal codes (≥ 10000)
// stay in cpuaff::affinity_category and don't masquerade as std::errc.
// ---------------------------------------------------------------------

TEST_CASE("error_category", "[error]")
{
    SECTION("cpuaff-internal codes match by direct enum comparison")
    {
        std::error_code ec(cpuaff::affinity_errc::stack_empty);
        REQUIRE(ec == cpuaff::affinity_errc::stack_empty);
        REQUIRE(ec.category() == cpuaff::affinity_category());
        REQUIRE(ec.value() == 10001);
        REQUIRE(!ec.message().empty());
        // Should NOT match any std::errc since it's a cpuaff-internal
        // code with no errno equivalent.
        REQUIRE_FALSE(ec == std::errc::invalid_argument);
        REQUIRE_FALSE(ec == std::errc::no_message_available);
    }

    SECTION("allocator_empty has its own value and message")
    {
        std::error_code ec(cpuaff::affinity_errc::allocator_empty);
        REQUIRE(ec == cpuaff::affinity_errc::allocator_empty);
        REQUIRE_FALSE(ec == cpuaff::affinity_errc::stack_empty);
        REQUIRE(ec.value() == 10002);
    }

    SECTION("errno-shaped codes round-trip through std::generic_category")
    {
        // EINVAL → cpuaff::affinity_category, but
        // default_error_condition maps it onto generic_category so
        // ec == std::errc::invalid_argument matches.
        std::error_code ec(cpuaff::affinity_errc::invalid_argument);
        REQUIRE(ec == cpuaff::affinity_errc::invalid_argument);
        REQUIRE(ec == std::errc::invalid_argument);
        REQUIRE(ec.value() == EINVAL);
    }

    SECTION("error_from_errno produces matching codes")
    {
        std::error_code ec = cpuaff::error_from_errno(EPERM);
        REQUIRE(ec == cpuaff::affinity_errc::permission_denied);
        REQUIRE(ec == std::errc::operation_not_permitted);
    }

    SECTION("affinity_category().message() falls through to "
            "std::generic_category for unrecognised values")
    {
        // EFAULT (14) is a real errno but not in cpuaff::affinity_errc.
        // The message() should still produce a real string via the
        // generic_category fallback.
        const auto msg = cpuaff::affinity_category().message(EFAULT);
        REQUIRE(!msg.empty());
        REQUIRE(msg != "unknown cpuaff::affinity error");
    }

    SECTION("default_error_condition splits at 10000")
    {
        // Below 10000: maps to generic_category.
        REQUIRE(cpuaff::affinity_category()
                    .default_error_condition(EINVAL)
                    .category()
                == std::generic_category());

        // At/above 10000: stays in cpuaff::affinity_category.
        REQUIRE(cpuaff::affinity_category()
                    .default_error_condition(10001)
                    .category()
                == cpuaff::affinity_category());
    }
}

// ---------------------------------------------------------------------
// expected_polyfill: exercise the std::expected-parity contract. When
// the toolchain ships <expected> + __cpp_lib_expected, cpuaff::expected
// aliases std::expected and these tests verify the alias works as
// advertised. When the polyfill is in use, they verify the polyfill
// matches the std::expected contract on the misuse paths that
// diverged in beta.1 (and were realigned in beta.2).
// ---------------------------------------------------------------------

TEST_CASE("expected_polyfill", "[error][expected]")
{
    using error_t = std::error_code;
    using value_expected = cpuaff::expected< int, error_t >;
    using void_expected = cpuaff::expected< void, error_t >;

    SECTION("expected<int, error_code> happy path")
    {
        value_expected v(42);
        REQUIRE(v.has_value());
        REQUIRE(static_cast< bool >(v));
        REQUIRE(*v == 42);
        REQUIRE(v.value() == 42);
    }

    SECTION("expected<int, error_code> error path")
    {
        value_expected v{ cpuaff::unexpected< error_t >(
            cpuaff::make_error_code(cpuaff::affinity_errc::stack_empty)) };
        REQUIRE_FALSE(v.has_value());
        REQUIRE_FALSE(static_cast< bool >(v));
        REQUIRE(v.error() == cpuaff::affinity_errc::stack_empty);
        REQUIRE_THROWS_AS(v.value(), cpuaff::bad_expected_access< error_t >);
    }

    SECTION("expected<void, error_code> happy path")
    {
        void_expected v;
        REQUIRE(v.has_value());
        REQUIRE(static_cast< bool >(v));
        REQUIRE_NOTHROW(v.value());
    }

    SECTION("expected<void, error_code> error path throws on value()")
    {
        void_expected v{ cpuaff::unexpected< error_t >(
            cpuaff::make_error_code(
                cpuaff::affinity_errc::allocator_empty)) };
        REQUIRE_FALSE(v.has_value());
        REQUIRE(v.error() == cpuaff::affinity_errc::allocator_empty);
        REQUIRE_THROWS_AS(v.value(), cpuaff::bad_expected_access< error_t >);
    }
}

// ---------------------------------------------------------------------
// try_get_available_cpus: exercise the cgroup/cpuset-aware accessor.
// In a unit-test environment without explicit cgroup setup, the
// available cpus equal the topology — this test verifies the happy
// path and the intersection semantics by manually narrowing the
// affinity to a single cpu.
//
// A truly cgroup-restricted scenario (where topology > available)
// requires running inside a cgroup or under systemd CPUAffinity= /
// Docker --cpuset-cpus and isn't reproducible from a plain test
// binary. That's tracked under the v3 test surface.
// ---------------------------------------------------------------------

TEST_CASE("try_get_available_cpus", "[affinity_manager][cgroup]")
{
    cpuaff::affinity_manager manager;
    REQUIRE(manager.has_cpus());

    cpuaff::cpu_set all_cpus;
    REQUIRE(manager.get_cpus(all_cpus));

    cpuaff::cpu first_cpu;
    REQUIRE(manager.get_cpu_from_index(first_cpu, 0));

    SECTION("default available == topology when unrestricted")
    {
        // Restore affinity to all cpus so we have an unconstrained
        // baseline; some CI environments may have set a starting
        // mask we don't want to inherit here.
        REQUIRE(manager.try_set_affinity(all_cpus).has_value());

        auto avail = manager.try_get_available_cpus();
        REQUIRE(avail.has_value());
        REQUIRE(avail->size() == all_cpus.size());
    }

    SECTION("narrowing the affinity narrows try_get_available_cpus()")
    {
        // Simulate a restricted environment by pinning the calling
        // thread to a single cpu. try_get_available_cpus() should
        // intersect the topology with that mask and return a
        // single-cpu set.
        cpuaff::cpu_set restricted;
        restricted.insert(first_cpu);
        REQUIRE(manager.try_set_affinity(restricted).has_value());

        auto avail = manager.try_get_available_cpus();
        REQUIRE(avail.has_value());
        REQUIRE(avail->size() == 1);
        REQUIRE(*avail->begin() == first_cpu);

        // Restore for subsequent tests.
        REQUIRE(manager.try_set_affinity(all_cpus).has_value());
    }

    SECTION("pthread_t overload reports the worker's available cpus")
    {
        worker_block block;
        std::thread worker(worker_main, std::ref(block));

        while (!block.started.load(std::memory_order_acquire))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        auto avail = manager.try_get_available_cpus(worker.native_handle());
        REQUIRE(avail.has_value());
        REQUIRE(!avail->empty());
        REQUIRE(avail->size() <= all_cpus.size());

        {
            std::lock_guard< std::mutex > lock(block.m);
            block.released = true;
        }
        block.cv.notify_all();
        worker.join();
    }
}
