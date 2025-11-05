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
#include <filesystem>
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

/***
 * @brief Test multiple named loggers (non-root) with different outputs
 */
TEST(HelloAWLogger, MultiLoggerCall)
{
    // Create multiple loggers with different names
    auto logger_network = aw_logger::getLogger("network");
    auto logger_database = aw_logger::getLogger("database");
    auto logger_business = aw_logger::getLogger("business");
    auto logger_auth = aw_logger::getLogger("auth");

    // verify they are created successfully
    ASSERT_NE(logger_network, nullptr);
    ASSERT_NE(logger_database, nullptr);
    ASSERT_NE(logger_business, nullptr);
    ASSERT_NE(logger_auth, nullptr);

    /* TODO(siyiya): fix log path */
    const auto log_path = std::filesystem::current_path().parent_path() / "log" / "test.log";

    // configure file appender
    auto database_appender = std::make_shared<aw_logger::FileAppender>(log_path.string());
    database_appender->setMaxFileSize(2 * 1024 * 1024); // 2 MB
    database_appender->setMaxBackupNum(5);

    // attribute logger to their own appender
    logger_network->setAppender(std::make_shared<aw_logger::ConsoleAppender>());
    logger_database->setAppender(database_appender);
    logger_business->setAppender(std::make_shared<aw_logger::FileAppender>(log_path.string()));
    logger_auth->setAppender(std::make_shared<aw_logger::ConsoleAppender>("stderr"));

    // verify they are different instances
    EXPECT_NE(logger_network, logger_database);
    EXPECT_NE(logger_network, logger_business);
    EXPECT_NE(logger_network, logger_auth);
    EXPECT_NE(logger_database, logger_business);
    EXPECT_NE(logger_database, logger_auth);
    EXPECT_NE(logger_auth, logger_business);

    // verify they are different from root logger
    auto root_logger = aw_logger::getLogger();
    EXPECT_NE(logger_network, root_logger);
    EXPECT_NE(logger_database, root_logger);
    EXPECT_NE(logger_business, root_logger);
    EXPECT_NE(logger_auth, root_logger);

    // test that getting the same name returns the same instance
    auto logger_network_2 = aw_logger::getLogger("network");
    EXPECT_EQ(logger_network, logger_network_2);

    // each logger outputs different messages
    EXPECT_NO_THROW(AW_LOG_INFO(logger_network, "[NETWORK] Server started on port 8080"));
    EXPECT_NO_THROW(AW_LOG_WARN(logger_network, "[NETWORK] Connection timeout detected"));
    EXPECT_NO_THROW(AW_LOG_FMT_ERROR(logger_network, "[NETWORK] Failed to bind to port {}", 8080));

    EXPECT_NO_THROW(AW_LOG_INFO(logger_database, "[DATABASE] Connected to PostgreSQL"));
    EXPECT_NO_THROW(AW_LOG_ERROR(logger_database, "[DATABASE] Query execution failed"));
    EXPECT_NO_THROW(AW_LOG_FMT_WARN(logger_database, "[DATABASE] Slow query detected: {}ms", 1500));

    EXPECT_NO_THROW(AW_LOG_DEBUG(logger_business, "[BUSINESS] Processing order #12345"));
    EXPECT_NO_THROW(AW_LOG_INFO(logger_business, "[BUSINESS] Order completed successfully"));
    EXPECT_NO_THROW(
        AW_LOG_FMT_NOTICE(logger_business, "[BUSINESS] Revenue today: ${:.2f}", 45678.90)
    );

    EXPECT_NO_THROW(AW_LOG_NOTICE(logger_auth, "[AUTH] User login attempt"));
    EXPECT_NO_THROW(AW_LOG_FATAL(logger_auth, "[AUTH] Authentication service down"));
    EXPECT_NO_THROW(AW_LOG_FMT_INFO(logger_auth, "[AUTH] Active sessions: {}", 42));

    // test concurrent logging from multiple named loggers
    std::vector<std::thread> threads;

    threads.emplace_back([logger_network]() {
        for (int i = 0; i < 300; i++)
        {
            AW_LOG_FMT_DEBUG(logger_network, "[NETWORK-THREAD] Packet {} received", i);
        }
    });

    threads.emplace_back([logger_database]() {
        for (int i = 0; i < 300; i++)
        {
            AW_LOG_FMT_INFO(logger_database, "[DATABASE-THREAD] Transaction {} committed", i);
        }
    });

    threads.emplace_back([logger_business]() {
        for (int i = 0; i < 300; i++)
        {
            AW_LOG_FMT_WARN(logger_business, "[BUSINESS-THREAD] Invoice {} generated", i);
        }
    });

    // wait for all threads to complete
    for (auto& t: threads)
    {
        EXPECT_TRUE(t.joinable());
        if (t.joinable())
            t.join();
    }

    // verify flush operations work correctly
    EXPECT_NO_THROW(logger_network->flush());
    EXPECT_NO_THROW(logger_database->flush());
    EXPECT_NO_THROW(logger_business->flush());
    EXPECT_NO_THROW(logger_auth->flush());

    SUCCEED();
}

/***
 * @brief Test custom pattern parsing with text components
 * @details
 * pattern type
 * %t timestamp
 * %p log level
 * %i thread id
 * %f source location - file name
 * %n source location - function name
 * %l source location - line
 * %m log message
 */
TEST(HelloAWLogger, CustomPatternParsing)
{
    // Test pattern with text (normal characters)
    auto factory1 = std::make_shared<aw_logger::ComponentFactory>("[%t] <%p> %m");
    auto formatter1 = std::make_shared<aw_logger::Formatter>(factory1);
    auto appender1 = std::make_shared<aw_logger::ConsoleAppender>(formatter1);
    auto logger1 = aw_logger::getLogger("pattern_test_1");
    logger1->setAppender(appender1);

    EXPECT_NO_THROW(AW_LOG_INFO(logger1, "Testing pattern with brackets and angle brackets"));

    // Test pattern with various separators
    auto factory2 = std::make_shared<aw_logger::ComponentFactory>("%t | %p | %i | %f:%l | %m");
    auto formatter2 = std::make_shared<aw_logger::Formatter>(factory2);
    auto appender2 = std::make_shared<aw_logger::ConsoleAppender>(formatter2);
    auto logger2 = aw_logger::getLogger("pattern_test_2");
    logger2->setAppender(appender2);

    EXPECT_NO_THROW(AW_LOG_WARN(logger2, "Testing pattern with pipe separators"));

    // Test pattern with prefix text
    auto factory3 =
        std::make_shared<aw_logger::ComponentFactory>("LOG: %t [Level=%p] [TID=%i] Message: %m");
    auto formatter3 = std::make_shared<aw_logger::Formatter>(factory3);
    auto appender3 = std::make_shared<aw_logger::ConsoleAppender>(formatter3);
    auto logger3 = aw_logger::getLogger("pattern_test_3");
    logger3->setAppender(appender3);

    EXPECT_NO_THROW(AW_LOG_ERROR(logger3, "Testing pattern with descriptive text"));

    // Test pattern with source location
    auto factory4 = std::make_shared<aw_logger::ComponentFactory>("%t [%p] (%f:%n:%l) -> %m");
    auto formatter4 = std::make_shared<aw_logger::Formatter>(factory4);
    auto appender4 = std::make_shared<aw_logger::ConsoleAppender>(formatter4);
    auto logger4 = aw_logger::getLogger("pattern_test_4");
    logger4->setAppender(appender4);

    EXPECT_NO_THROW(AW_LOG_FMT_FATAL(logger4, "Testing with source location: value={}", 123));

    // Test simple pattern without text
    auto factory5 = std::make_shared<aw_logger::ComponentFactory>("%t%p%i%m");
    auto formatter5 = std::make_shared<aw_logger::Formatter>(factory5);
    auto appender5 = std::make_shared<aw_logger::ConsoleAppender>(formatter5);
    auto logger5 = aw_logger::getLogger("pattern_test_5");
    logger5->setAppender(appender5);

    EXPECT_NO_THROW(AW_LOG_DEBUG(logger5, "Testing compact pattern"));

    // Test pattern with complex text
    auto factory6 = std::make_shared<aw_logger::ComponentFactory>(
        "=== Time: %t === Level: %p === Thread: %i === Location: %f at line %l === Message: %m ==="
    );
    auto formatter6 = std::make_shared<aw_logger::Formatter>(factory6);
    auto appender6 = std::make_shared<aw_logger::ConsoleAppender>(formatter6);
    auto logger6 = aw_logger::getLogger("pattern_test_6");
    logger6->setAppender(appender6);

    EXPECT_NO_THROW(AW_LOG_NOTICE(logger6, "Testing verbose pattern with multiple text segments"));

    SUCCEED();
}

#endif //! TEST__HELLO_AW_LOGGER_CPP
