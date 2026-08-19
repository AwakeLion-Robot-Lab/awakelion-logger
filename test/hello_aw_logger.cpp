// Copyright 2026 siyiovo
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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// aw_logger library
#include "aw_logger/aw_logger.hpp"
#include "utils.hpp"

template<typename T>
concept HasStats = requires(const T& logger)
{
    logger.getStats();
};

static_assert(!HasStats<aw_logger::Logger>);

/***
 * @brief appender that pauses the first append
 */
class BlockingAppender final: public aw_logger::BaseAppender {
public:
    /***
     * @brief constructor
     * @param flush_logger optional logger to flush from append
     */
    explicit BlockingAppender(std::weak_ptr<aw_logger::Logger> flush_logger = {}):
        flush_logger_(std::move(flush_logger))
    {}

    /***
     * @brief append event and pause the first call
     * @param event log event
     */
    void append(const aw_logger::LogEvent::Ptr& event) override
    {
        std::unique_lock<std::mutex> lk(mtx_);
        messages_.push_back(event->getMsg());
        if (auto logger = flush_logger_.lock())
            logger->flush();
        if (messages_.size() == 1)
        {
            entered_ = true;
            entered_cv_.notify_all();
            release_cv_.wait(lk, [this] { return released_; });
        }
    }

    /***
     * @brief flush appender output
     */
    void flush() override {}

    /***
     * @brief wait until the first append is paused
     * @return whether the first append was entered
     */
    bool waitUntilEntered()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        return entered_cv_.wait_for(lk, std::chrono::seconds(2), [this] { return entered_; });
    }

    /***
     * @brief release the paused append
     */
    void release()
    {
        std::lock_guard<std::mutex> lk(mtx_);
        released_ = true;
        release_cv_.notify_all();
    }

    /***
     * @brief copy delivered messages
     * @return delivered messages
     */
    std::vector<std::string> messages()
    {
        std::lock_guard<std::mutex> lk(mtx_);
        return messages_;
    }

private:
    /***
     * @brief optional logger flushed by append
     */
    std::weak_ptr<aw_logger::Logger> flush_logger_;

    /***
     * @brief protects appender state
     */
    std::mutex mtx_;

    /***
     * @brief wakes waiters after the first append starts
     */
    std::condition_variable entered_cv_;

    /***
     * @brief wakes the paused append
     */
    std::condition_variable release_cv_;

    /***
     * @brief delivered event messages
     */
    std::vector<std::string> messages_;

    /***
     * @brief whether the first append started
     */
    bool entered_ = false;

    /***
     * @brief whether the first append may return
     */
    bool released_ = false;
};

std::vector<std::string> runOverflowPolicy(aw_logger::Logger::Policy policy)
{
    aw_logger::Logger::Options options;
    options.normal_capacity = 2;
    options.critical_capacity = 2;
    options.normal_policy = policy;
    options.critical_policy = policy;
    auto appender = std::make_shared<BlockingAppender>();
    auto logger =
        std::make_shared<aw_logger::Logger>("overflow", aw_logger::LogLevel::level::DEBUG, options);
    logger->setAppender(appender);

    AW_LOG_INFO(logger, "first");
    EXPECT_TRUE(appender->waitUntilEntered());
    AW_LOG_INFO(logger, "second");
    AW_LOG_INFO(logger, "third");
    AW_LOG_INFO(logger, "fourth");
    appender->release();
    EXPECT_NO_THROW(logger->flush());
    return appender->messages();
}

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
 * @brief verify a logger without an appender does not escape submit failures
 */
TEST(HelloAWLogger, LoggerWithoutAppender)
{
    auto logger = std::make_shared<aw_logger::Logger>("without_appender");
    EXPECT_NO_THROW(AW_LOG_INFO(logger, "discarded"));
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
 * @brief verify drop_new delivery and flush behavior
 */
TEST(HelloAWLogger, DropNewOverflow)
{
    const auto messages = runOverflowPolicy(aw_logger::Logger::Policy::drop_new);
    EXPECT_EQ(messages, (std::vector<std::string> { "first", "second", "third" }));
}

/***
 * @brief verify overrun_oldest delivery and flush behavior
 */
TEST(HelloAWLogger, OverrunOldestOverflow)
{
    const auto messages = runOverflowPolicy(aw_logger::Logger::Policy::overrun_oldest);
    EXPECT_EQ(messages, (std::vector<std::string> { "first", "third", "fourth" }));
}

/***
 * @brief verify block policy delivers every event
 */
TEST(HelloAWLogger, BlockOverflow)
{
    aw_logger::Logger::Options options;
    options.normal_capacity = 2;
    options.critical_capacity = 2;
    options.normal_policy = aw_logger::Logger::Policy::block;
    options.critical_policy = aw_logger::Logger::Policy::block;
    auto appender = std::make_shared<BlockingAppender>();
    auto logger =
        std::make_shared<aw_logger::Logger>("block", aw_logger::LogLevel::level::DEBUG, options);
    logger->setAppender(appender);

    AW_LOG_INFO(logger, "first");
    ASSERT_TRUE(appender->waitUntilEntered());

    std::atomic<int> submitted { 0 };
    std::thread producer([&] {
        AW_LOG_INFO(logger, "second");
        submitted.fetch_add(1, std::memory_order_release);
        AW_LOG_INFO(logger, "third");
        submitted.fetch_add(1, std::memory_order_release);
        AW_LOG_INFO(logger, "fourth");
        submitted.fetch_add(1, std::memory_order_release);
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (submitted.load(std::memory_order_acquire) < 2
           && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::yield();
    }
    const bool producer_blocked = submitted.load(std::memory_order_acquire) == 2;
    appender->release();
    producer.join();

    EXPECT_TRUE(producer_blocked);
    EXPECT_EQ(submitted.load(std::memory_order_acquire), 3);
    EXPECT_NO_THROW(logger->flush());
    EXPECT_EQ(
        appender->messages(),
        (std::vector<std::string> { "first", "second", "third", "fourth" })
    );
}

/***
 * @brief verify stop rejects and wakes a blocked submission
 */
TEST(HelloAWLogger, StopWithBlockedSubmission)
{
    aw_logger::Logger::Options options;
    options.normal_capacity = 2;
    options.critical_capacity = 2;
    options.normal_policy = aw_logger::Logger::Policy::block;
    options.critical_policy = aw_logger::Logger::Policy::block;
    auto appender = std::make_shared<BlockingAppender>();
    auto logger = std::make_shared<aw_logger::Logger>(
        "stop_block",
        aw_logger::LogLevel::level::DEBUG,
        options
    );
    logger->setAppender(appender);

    AW_LOG_INFO(logger, "first");
    ASSERT_TRUE(appender->waitUntilEntered());

    std::atomic<int> submitted { 0 };
    std::thread producer([&] {
        AW_LOG_INFO(logger, "second");
        submitted.fetch_add(1, std::memory_order_release);
        AW_LOG_INFO(logger, "third");
        submitted.fetch_add(1, std::memory_order_release);
        AW_LOG_INFO(logger, "fourth");
        submitted.fetch_add(1, std::memory_order_release);
    });

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (submitted.load(std::memory_order_acquire) < 2
           && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::yield();
    }
    const bool producer_blocked = submitted.load(std::memory_order_acquire) == 2;

    std::atomic<bool> stopped { false };
    std::thread stopper([&] {
        logger->stop();
        stopped.store(true, std::memory_order_release);
    });

    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (submitted.load(std::memory_order_acquire) < 3
           && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::yield();
    }
    const bool producer_released = submitted.load(std::memory_order_acquire) == 3;
    appender->release();
    producer.join();
    stopper.join();

    EXPECT_TRUE(producer_blocked);
    EXPECT_TRUE(producer_released);
    EXPECT_TRUE(stopped.load(std::memory_order_acquire));
    EXPECT_EQ(appender->messages(), (std::vector<std::string> { "first", "second", "third" }));
}

/***
 * @brief verify a stopped logger can be started again
 */
TEST(HelloAWLogger, RestartAfterStop)
{
    auto appender = std::make_shared<BlockingAppender>();
    auto logger = std::make_shared<aw_logger::Logger>("restart");
    logger->setAppender(appender);

    AW_LOG_INFO(logger, "before stop");
    ASSERT_TRUE(appender->waitUntilEntered());
    appender->release();
    ASSERT_NO_THROW(logger->flush());

    logger->stop();
    logger->start();
    AW_LOG_INFO(logger, "after start");
    ASSERT_NO_THROW(logger->flush());

    EXPECT_EQ(appender->messages(), (std::vector<std::string> { "before stop", "after start" }));
}

/***
 * @brief verify flush from an appender callback does not deadlock
 */
TEST(HelloAWLogger, FlushFromAppenderCallback)
{
    auto logger = std::make_shared<aw_logger::Logger>("flush_callback");
    auto appender = std::make_shared<BlockingAppender>(logger);
    appender->release();
    logger->setAppender(appender);

    AW_LOG_INFO(logger, "callback flush");
    ASSERT_NO_THROW(logger->flush());
    EXPECT_EQ(appender->messages(), (std::vector<std::string> { "callback flush" }));
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

    // Create log directory if it doesn't exist and set log file path
    const auto log_dir = std::filesystem::current_path() / "test";
    std::filesystem::create_directories(log_dir);
    const auto log_path = log_dir / "test.log";

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
    auto factory1 = std::make_unique<aw_logger::ComponentFactory>("[%t] <%p> %m");
    auto formatter1 = std::make_unique<aw_logger::Formatter>(std::move(factory1));
    auto appender1 = std::make_shared<aw_logger::ConsoleAppender>(std::move(formatter1));
    auto logger1 = aw_logger::getLogger("pattern_test_1");
    logger1->setAppender(appender1);

    EXPECT_NO_THROW(AW_LOG_INFO(logger1, "Testing pattern with brackets and angle brackets"));

    // Test pattern with various separators
    auto factory2 = std::make_unique<aw_logger::ComponentFactory>("%t | %p | %i | %f:%l | %m");
    auto formatter2 = std::make_unique<aw_logger::Formatter>(std::move(factory2));
    auto appender2 = std::make_shared<aw_logger::ConsoleAppender>(std::move(formatter2));
    auto logger2 = aw_logger::getLogger("pattern_test_2");
    logger2->setAppender(appender2);

    EXPECT_NO_THROW(AW_LOG_WARN(logger2, "Testing pattern with pipe separators"));

    // Test pattern with prefix text
    auto factory3 =
        std::make_unique<aw_logger::ComponentFactory>("LOG: %t [Level=%p] [TID=%i] Message: %m");
    auto formatter3 = std::make_unique<aw_logger::Formatter>(std::move(factory3));
    auto appender3 = std::make_shared<aw_logger::ConsoleAppender>(std::move(formatter3));
    auto logger3 = aw_logger::getLogger("pattern_test_3");
    logger3->setAppender(appender3);

    EXPECT_NO_THROW(AW_LOG_ERROR(logger3, "Testing pattern with descriptive text"));

    // Test pattern with source location
    auto factory4 = std::make_unique<aw_logger::ComponentFactory>("%t [%p] (%f:%n:%l) -> %m");
    auto formatter4 = std::make_unique<aw_logger::Formatter>(std::move(factory4));
    auto appender4 = std::make_shared<aw_logger::ConsoleAppender>(std::move(formatter4));
    auto logger4 = aw_logger::getLogger("pattern_test_4");
    logger4->setAppender(appender4);

    EXPECT_NO_THROW(AW_LOG_FMT_FATAL(logger4, "Testing with source location: value={}", 123));

    // Test simple pattern without text
    auto factory5 = std::make_unique<aw_logger::ComponentFactory>("%t%p%i%m");
    auto formatter5 = std::make_unique<aw_logger::Formatter>(std::move(factory5));
    auto appender5 = std::make_shared<aw_logger::ConsoleAppender>(std::move(formatter5));
    auto logger5 = aw_logger::getLogger("pattern_test_5");
    logger5->setAppender(appender5);

    EXPECT_NO_THROW(AW_LOG_DEBUG(logger5, "Testing compact pattern"));

    // Test pattern with complex text
    auto factory6 = std::make_unique<aw_logger::ComponentFactory>(
        "=== Time: %t === Level: %p === Thread: %i === Location: %f at line %l === Message: %m ==="
    );
    auto formatter6 = std::make_unique<aw_logger::Formatter>(std::move(factory6));
    auto appender6 = std::make_shared<aw_logger::ConsoleAppender>(std::move(formatter6));
    auto logger6 = aw_logger::getLogger("pattern_test_6");
    logger6->setAppender(appender6);

    EXPECT_NO_THROW(AW_LOG_NOTICE(logger6, "Testing verbose pattern with multiple text segments"));

    SUCCEED();
}

TEST(HelloAWLogger, ColorControl)
{
    auto factory = std::make_unique<aw_logger::ComponentFactory>();
    auto formatter = std::make_unique<aw_logger::Formatter>(std::move(factory));
    formatter->setLevelColor(aw_logger::LogLevel::level::INFO, "cyan");
    formatter->setLevelColor(aw_logger::LogLevel::level::WARN, "orange");
    /* you can set specific level like below */
    formatter->setDebugColor("violet");

    auto console_appender = std::make_shared<aw_logger::ConsoleAppender>(std::move(formatter));
    console_appender->enableColor(true);

    auto logger = aw_logger::getLogger("colorful_test");
    logger->setAppender(console_appender);

    EXPECT_NO_THROW(AW_LOG_INFO(logger, "INFO should be cyan"));
    EXPECT_NO_THROW(AW_LOG_WARN(logger, "WARN should be orange"));
    EXPECT_NO_THROW(AW_LOG_DEBUG(logger, "DEBUG should be violet"));

    SUCCEED();
}

TEST(HelloAWLogger, WebsocketLogging)
{
    auto websocket_appender = std::make_shared<aw_logger::WebsocketAppender>("ws://127.0.0.1:1234");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!websocket_appender->isConnected())
        std::this_thread::sleep_for(std::chrono::seconds(1));

    auto websocket_logger = aw_logger::getLogger("websocket");
    websocket_logger->setAppender(websocket_appender);
    ASSERT_NE(websocket_logger, nullptr);

    const int ITERATIONS = 100;
    AW_LOG_NOTICE(
        websocket_logger,
        "Hello Awakelion Logger! Starting websocket logging performance test..."
    );
    for (int i = 1; i <= ITERATIONS; i++)
    {
        AW_LOG_FMT_INFO(websocket_logger, "Awakelion Logger websocket uploading count: {}.", i);
        /* do something like switch threshold level */
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        AW_LOG_FATAL(websocket_logger, "threshold level switch testing");
    }

    SUCCEED();
}

TEST(HelloAWLogger, MultiAppendersLogging)
{
    const auto log_dir = std::filesystem::current_path() / "test";
    std::filesystem::create_directories(log_dir);
    const auto log_path = log_dir / "test.log";

    auto console_appender = std::make_shared<aw_logger::ConsoleAppender>();
    auto file_appender = std::make_shared<aw_logger::FileAppender>(log_path.string());
    auto websocket_appender = std::make_shared<aw_logger::WebsocketAppender>("ws://127.0.0.1:1234");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!websocket_appender->isConnected())
        std::this_thread::sleep_for(std::chrono::seconds(1));

    ASSERT_NE(console_appender, nullptr);
    ASSERT_NE(file_appender, nullptr);
    ASSERT_NE(websocket_appender, nullptr);

    auto logger = aw_logger::getLogger("multi_appender");
    logger->setAppenders(console_appender, file_appender, websocket_appender);
    ASSERT_NE(logger, nullptr);

    const int ITERATIONS = 100;
    AW_LOG_NOTICE(logger, "Starting concurrent multi-appenders logging performance test...");
    for (int i = 1; i <= ITERATIONS; i++)
    {
        AW_LOG_FMT_INFO(logger, "Multi-appenders logging count: {}.", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    SUCCEED();
}

#endif //! TEST__HELLO_AW_LOGGER_CPP
