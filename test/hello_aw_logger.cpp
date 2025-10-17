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

#ifndef TEST__HELLO_AW_LOGGER_CPP
#define TEST__HELLO_AW_LOGGER_CPP

// GoogleTest library
#include <gtest/gtest.h>

// C++ standard library
#include <thread>
#include <vector>

// aw_logger library
#include "aw_logger/aw_logger.hpp"
#include "utils.hpp"

/***
 * @brief Test logger instance
 */
TEST(HelloAWLogger, LoggerInstance)
{
    auto logger1 = aw_logger::getLogger();
    auto logger2 = aw_logger::getLogger();

    ASSERT_NE(logger1, nullptr);
    ASSERT_NE(logger2, nullptr);
    EXPECT_EQ(logger1, logger2);
}

/***
 * @brief Test basic macro
 */
TEST(HelloAWLogger, BasicMacro)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    AW_LOG_DEBUG(logger, "Hello DEBUG");
    AW_LOG_INFO(logger, "Hello INFO");
    AW_LOG_NOTICE(logger, "Hello NOTICE");
    AW_LOG_WARN(logger, "Hello WARN");
    AW_LOG_ERROR(logger, "Hello ERROR");
    AW_LOG_FATAL(logger, "Hello FATAL");

    SUCCEED();
}

/***
 * @brief Test macro with fmt
 */
TEST(HelloAWLogger, FMTMacro)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    AW_LOG_FMT_DEBUG(logger, "Debug: value = {}", 42);
    AW_LOG_FMT_INFO(logger, "Info: {} + {} = {}", 1, 2, 3);
    AW_LOG_FMT_NOTICE(logger, "Notice: pi = {:.2f}", 3.14159);
    AW_LOG_FMT_WARN(logger, "Warn: name = {}", "test");
    AW_LOG_FMT_ERROR(logger, "Error: bool = {}", true);
    AW_LOG_FMT_FATAL(logger, "Fatal: hex = {:#x}", 255);

    SUCCEED();
}

/***
 * @brief Test 100 macro calls
 */
TEST(HelloAWLogger, HCall)
{
    auto logger = aw_logger::getLogger();
    ASSERT_NE(logger, nullptr);

    for (int i = 0; i < 100; i++)
    {
        AW_LOG_INFO(logger, "Hello aw_logger!");
        AW_LOG_FMT_INFO(logger, "Counter: {}", i);
    }

    SUCCEED();
}

#endif //! TEST__HELLO_AW_LOGGER_CPP
