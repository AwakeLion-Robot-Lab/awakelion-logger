// Copyright 2025 siyiovo
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TEST__BANCHMARK_CPP
#define TEST__BANCHMARK_CPP

// GoogleTest library
#include <gtest/gtest.h>

// C++ standard library
#include <fcntl.h>
#include <unistd.h>
#include <iomanip>
#include <thread>
#include <vector>

// aw_logger library
#include "aw_logger/aw_logger.hpp"
#include "utils.hpp"

/***
 * @brief Helper class to redirect stdout
 */
class StdoutRedirector {
public:
    StdoutRedirector(const char* path)
    {
        fflush(stdout);
        old_stdout_ = dup(STDOUT_FILENO);
        new_stdout_ = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        dup2(new_stdout_, STDOUT_FILENO);
    }

    ~StdoutRedirector()
    {
        fflush(stdout);
        dup2(old_stdout_, STDOUT_FILENO);
        close(old_stdout_);
        close(new_stdout_);
    }

private:
    int old_stdout_;
    int new_stdout_;
};

/***
 * @brief Benchmark: Basic macro -> Console
 */
TEST(BenchmarkLogger, BasicMacro_Console)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    const int ITERATIONS = 10000;
    aw_test::Latency stats;

    std::cerr << "\n[Test 1] Basic Macro -> Console (" << ITERATIONS << " calls)\n";

    aw_test::TicToc total_timer;
    total_timer.tic();

    for (int i = 0; i < ITERATIONS; i++)
    {
        aw_test::TicToc timer;
        timer.tic();
        AW_LOG_INFO(logger, "Benchmark test message");
        stats.add(timer.toc());
    }

    total_timer.toc();

    stats.print("Basic Macro (Console)");
    SUCCEED();
}

/***
 * @brief Benchmark: Basic macro -> /dev/null
 */
TEST(BenchmarkLogger, BasicMacro_DevNull)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    const int ITERATIONS = 10000;
    aw_test::Latency stats;

    std::cerr << "\n[Test 2] Basic Macro -> /dev/null (" << ITERATIONS << " calls)\n";
    std::cerr << "NOTE: Output redirected to /dev/null to measure pure logging performance\n";

    {
        StdoutRedirector redirector("/dev/null");

        aw_test::TicToc total_timer;
        total_timer.tic();

        for (int i = 0; i < ITERATIONS; i++)
        {
            aw_test::TicToc timer;
            timer.tic();
            AW_LOG_INFO(logger, "Benchmark test message");
            stats.add(timer.toc());
        }

        total_timer.toc();
    }

    stats.print("Basic Macro (/dev/null)");
    SUCCEED();
}

/***
 * @brief Benchmark: Formatted macro -> Console
 */
TEST(BenchmarkLogger, FmtMacro_Console)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    const int ITERATIONS = 10000;
    aw_test::Latency stats;

    std::cerr << "\n[Test 3] Formatted Macro -> Console (" << ITERATIONS << " calls)\n";

    aw_test::TicToc total_timer;
    total_timer.tic();

    for (int i = 0; i < ITERATIONS; i++)
    {
        aw_test::TicToc timer;
        timer.tic();
        AW_LOG_FMT_INFO(logger, "Benchmark test: iteration {}", i);
        stats.add(timer.toc());
    }

    total_timer.toc();

    stats.print("Formatted Macro (Console)");
    SUCCEED();
}

/***
 * @brief Benchmark: Formatted macro -> /dev/null
 */
TEST(BenchmarkLogger, FmtMacro_DevNull)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    const int ITERATIONS = 10000;
    aw_test::Latency stats;

    std::cerr << "\n[Test 4] Formatted Macro -> /dev/null (" << ITERATIONS << " calls)\n";

    {
        StdoutRedirector redirector("/dev/null");

        aw_test::TicToc total_timer;
        total_timer.tic();

        for (int i = 0; i < ITERATIONS; i++)
        {
            aw_test::TicToc timer;
            timer.tic();
            AW_LOG_FMT_INFO(logger, "Benchmark test: iteration {}", i);
            stats.add(timer.toc());
        }

        total_timer.toc();
    }

    stats.print("Formatted Macro (/dev/null)");
    SUCCEED();
}

/***
 * @brief Benchmark: Different log levels -> Console
 */
TEST(BenchmarkLogger, DifferentLevels_Console)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    const int ITERATIONS = 5000;

    std::cerr << "\n[Test 5] Different Log Levels -> Console (" << ITERATIONS << " calls each)\n";

    /* DEBUG */
    {
        aw_test::Latency stats;
        for (int i = 0; i < ITERATIONS; i++)
        {
            aw_test::TicToc timer;
            timer.tic();
            AW_LOG_DEBUG(logger, "DEBUG level benchmark");
            stats.add(timer.toc());
        }
        stats.print("DEBUG Level (Console)");
    }

    /* INFO */
    {
        aw_test::Latency stats;
        for (int i = 0; i < ITERATIONS; i++)
        {
            aw_test::TicToc timer;
            timer.tic();
            AW_LOG_INFO(logger, "INFO level benchmark");
            stats.add(timer.toc());
        }
        stats.print("INFO Level (Console)");
    }

    /* ERROR */
    {
        aw_test::Latency stats;
        for (int i = 0; i < ITERATIONS; i++)
        {
            aw_test::TicToc timer;
            timer.tic();
            AW_LOG_ERROR(logger, "ERROR level benchmark");
            stats.add(timer.toc());
        }
        stats.print("ERROR Level (Console)");
    }

    SUCCEED();
}

/***
 * @brief Benchmark: Different log levels -> /dev/null
 */
TEST(BenchmarkLogger, DifferentLevels_DevNull)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    const int ITERATIONS = 5000;

    std::cerr << "\n[Test 6] Different Log Levels -> /dev/null (" << ITERATIONS << " calls each)\n";

    StdoutRedirector redirector("/dev/null");

    /* DEBUG */
    {
        aw_test::Latency stats;
        for (int i = 0; i < ITERATIONS; i++)
        {
            aw_test::TicToc timer;
            timer.tic();
            AW_LOG_DEBUG(logger, "DEBUG level benchmark");
            stats.add(timer.toc());
        }
        stats.print("DEBUG Level (/dev/null)", std::cerr);
    }

    /* INFO */
    {
        aw_test::Latency stats;
        for (int i = 0; i < ITERATIONS; i++)
        {
            aw_test::TicToc timer;
            timer.tic();
            AW_LOG_INFO(logger, "INFO level benchmark");
            stats.add(timer.toc());
        }
        stats.print("INFO Level (/dev/null)", std::cerr);
    }

    /* ERROR */
    {
        aw_test::Latency stats;
        for (int i = 0; i < ITERATIONS; i++)
        {
            aw_test::TicToc timer;
            timer.tic();
            AW_LOG_ERROR(logger, "ERROR level benchmark");
            stats.add(timer.toc());
        }
        stats.print("ERROR Level (/dev/null)", std::cerr);
    }

    SUCCEED();
}

/***
 * @brief Benchmark: EXTREME LOAD -> Console
 */
TEST(BenchmarkLogger, ExtremeLoad_Console)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    const int ITERATIONS = 100000;
    aw_test::Latency stats;

    std::cerr << "\n[Test 7] EXTREME LOAD -> Console (" << ITERATIONS << " calls)\n";

    aw_test::TicToc total_timer;
    total_timer.tic();

    for (int i = 0; i < ITERATIONS; i++)
    {
        aw_test::TicToc timer;
        timer.tic();
        AW_LOG_FMT_INFO(logger, "Extreme load iteration: {}", i);
        stats.add(timer.toc());
    }

    total_timer.toc();

    stats.print("Extreme Load (Console, 100K calls)");
    SUCCEED();
}

/***
 * @brief Benchmark: EXTREME LOAD -> /dev/null
 */
TEST(BenchmarkLogger, ExtremeLoad_DevNull)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    const int ITERATIONS = 100000;
    aw_test::Latency stats;

    std::cerr << "\n[Test 8] EXTREME LOAD -> /dev/null (" << ITERATIONS << " calls)\n";

    {
        StdoutRedirector redirector("/dev/null");

        aw_test::TicToc total_timer;
        total_timer.tic();

        for (int i = 0; i < ITERATIONS; i++)
        {
            aw_test::TicToc timer;
            timer.tic();
            AW_LOG_FMT_INFO(logger, "Extreme load iteration: {}", i);
            stats.add(timer.toc());
        }

        total_timer.toc();
    }

    stats.print("Extreme Load (/dev/null, 100K calls)");
    SUCCEED();
}

/***
 * @brief Benchmark: Multi-type formatting comparison
 */
TEST(BenchmarkLogger, MultiTypeFormatting_Comparison)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    const int ITERATIONS = 5000;

    std::cerr << "\n[Test 9] Multi-Type Formatting Comparison (" << ITERATIONS << " calls each)\n";

    /* output to console */
    {
        aw_test::Latency stats;
        for (int i = 0; i < ITERATIONS; i++)
        {
            aw_test::TicToc timer;
            timer.tic();
            AW_LOG_FMT_INFO(
                logger,
                "Multi: int={}, double={:.3f}, str={}, bool={}",
                i,
                3.14159 * i,
                "test",
                i % 2 == 0
            );
            stats.add(timer.toc());
        }
        stats.print("Multi-Type (Console)");
    }

    /* output to /dev/null */
    {
        StdoutRedirector redirector("/dev/null");
        aw_test::Latency stats;
        for (int i = 0; i < ITERATIONS; i++)
        {
            aw_test::TicToc timer;
            timer.tic();
            AW_LOG_FMT_INFO(
                logger,
                "Multi: int={}, double={:.3f}, str={}, bool={}",
                i,
                3.14159 * i,
                "test",
                i % 2 == 0
            );
            stats.add(timer.toc());
        }
        stats.print("Multi-Type (/dev/null)", std::cerr);
    }

    SUCCEED();
}

/***
 * @brief Benchmark: Multi-threaded logging
 */
TEST(BenchmarkLogger, MultiThreadedLogging)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    const int NUM_THREADS = 4;
    const int LOGS_PER_THREAD = 50000;
    std::vector<std::thread> threads;

    std::cerr << "\n[Test 10] Multi-Threaded Logging (" << NUM_THREADS << " threads, "
              << LOGS_PER_THREAD << " logs per thread)\n";

    aw_test::TicToc timer;
    timer.tic();

    for (int t = 0; t < NUM_THREADS; t++)
    {
        threads.emplace_back([&logger, t]() {
            for (int i = 0; i < LOGS_PER_THREAD; i++)
            {
                AW_LOG_FMT_INFO(logger, "Thread-{} | Message-{}", t, i);
            }
        });
    }

    for (auto& thread: threads)
    {
        thread.join();
    }

    double elapsed = timer.toc();
    int total_logs = NUM_THREADS * LOGS_PER_THREAD;

    std::cerr << "====================Elapsed time: " << "===================\n"
              << elapsed << " seconds\n";
    SUCCEED();
}

#endif //! TEST__BANCHMARK_CPP
