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

#ifndef IMPL__LOGGER_IMPL_HPP
#define IMPL__LOGGER_IMPL_HPP

// C++ standard library
#include <algorithm>
#include <typeinfo>
#include <vector>

// aw_logger library
#include "aw_logger/exception.hpp"
#include "aw_logger/logger.hpp"

namespace aw_logger {
inline Logger::Logger(const std::string& name, const LogLevel::level lvl):
    Logger(name, lvl, Options {})
{}

inline Logger::Logger(const std::string& name, const LogLevel::level lvl, const Options& options):
    options_(options),
    threshold_level_(lvl),
    running_(false),
    name_(name)
{}

inline Logger::~Logger()
{
    /* reject destruction inside worker */
    if (active_worker_logger_ == this)
        std::terminate();

    /* flush queues and stop */
    flush();
    stop();
}

inline void Logger::submit(const std::shared_ptr<LogEvent>& event)
{
    /* check status of log level */
    const auto curr_level = getThresholdLevel();
    if (event == nullptr || event->getLogLevel() < curr_level)
        return;

    /* route to the local queue or the bound root logger */
    Backend* state = nullptr;
    Logger::Ptr root;
    std::shared_lock<std::shared_mutex> route_lock(rw_mtx_);
    if (!appenders_.empty())
    {
        state = backend_.get();
    }
    else
    {
        root = root_logger_;
    }
    route_lock.unlock();

    if (state == nullptr)
    {
        if (root != nullptr)
            root->submit(event);
        else
            throw aw_logger::invalid_parameter("root logger is nullptr!");
        return;
    }

    /* choose queue by log level */
    const bool critical = event->getLogLevel() >= LogLevel::level::ERROR;
    const auto policy = critical ? options_.critical_policy : options_.normal_policy;
    if (policy == Logger::Policy::block && active_worker_logger_ == this)
        throw aw_logger::invalid_parameter("blocking submit from appender callback is invalid!");

    /* start worker before publishing the event */
    try
    {
        startWorker(state, false);
    } catch (...)
    {
        return;
    }

    if (!beginSubmission())
        return;

    bool accepted = false;
    LogEvent::Ptr overwritten_event;
    auto& queue = critical ? state->critical : state->normal;
    /* apply selected overflow policy */
    try
    {
        switch (policy)
        {
            case Logger::Policy::drop_new:
                accepted = queue.push(event);
                break;
            case Logger::Policy::block:
                accepted = pushBlocking(state, queue, event);
                break;
            case Logger::Policy::overrun_oldest:
                accepted = queue.pushOverwriteOldest(event, overwritten_event);
                break;
        }
    } catch (...)
    {
        finishSubmission();
        throw;
    }

    if (accepted && overwritten_event != nullptr)
    {
        /* complete the event removed by overwrite */
        auto& completed = critical ? state->critical_completed : state->normal_completed;
        completed.fetch_add(1, std::memory_order_release);
        state->flush_cv.notify_all();
    }

    if (accepted && policy != Logger::Policy::block)
    {
        /* wake worker after enqueue */
        state->work_seq.fetch_add(1, std::memory_order_seq_cst);
        state->work_seq.notify_one();
    }
    finishSubmission();
}

inline bool Logger::pushBlocking(
    Backend* state,
    RingBuffer<std::shared_ptr<LogEvent>>& queue,
    const std::shared_ptr<LogEvent>& event
)
{
    std::unique_lock<std::mutex> wait_lk(state->space_mtx);
    while (isAccepting())
    {
        if (queue.push(event))
        {
            /* publish work before releasing space mutex */
            state->work_seq.fetch_add(1, std::memory_order_seq_cst);
            state->work_seq.notify_one();
            return true;
        }

        /* wait for consumer progress or shutdown */
        state->space_cv.wait(wait_lk, [&] {
            return !isAccepting() || queue.getSize() < queue.getCapacity();
        });
    }
    return false;
}

inline void Logger::setRootLogger(const Logger::Ptr& root_logger)
{
    if (root_logger == nullptr)
        throw aw_logger::invalid_parameter("input root logger is nullptr!");

    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    /* check existing and set root logger under write lock for thread-safe */
    if (root_logger_ != nullptr)
        throw aw_logger::invalid_parameter("root logger has been already set!");

    root_logger_ = root_logger;
}

inline void Logger::setAppender(const std::shared_ptr<BaseAppender>& appender)
{
    if (appender == nullptr)
        throw aw_logger::invalid_parameter("input appender is nullptr!");

    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    /* check existing and set appender under write lock for thread-safe */
    bool ok = std::any_of(
        appenders_.begin(),
        appenders_.end(),
        [&appender](const std::shared_ptr<BaseAppender>& ex_app) { return (ex_app == appender); }
    );
    if (ok)
        throw aw_logger::invalid_parameter(
            std::string("an existing-type appender like: ") + typeid(*appender).name()
            + "has already setup!"
        );

    /* allocate queues lazily with the first appender */
    std::unique_ptr<Backend> new_backend;
    if (backend_ == nullptr)
        new_backend = std::make_unique<Backend>(options_);

    appenders_.emplace_back(appender);
    if (new_backend != nullptr)
        backend_ = std::move(new_backend);
}

// clang-format off
template<typename... UArgs>
    requires(std::convertible_to<UArgs, std::shared_ptr<BaseAppender>> && ...)
void aw_logger::Logger::setAppenders(UArgs&&... appenders)
// clang-format on
{
    /* check duplicate first */
    std::vector<std::shared_ptr<BaseAppender>> temp_appenders { std::forward<UArgs>(appenders)... };

    /* `std::sort` for faster search */
    std::sort(temp_appenders.begin(), temp_appenders.end());
    auto duplicate_it = std::adjacent_find(temp_appenders.begin(), temp_appenders.end());
    if (duplicate_it != temp_appenders.end())
        throw aw_logger::invalid_parameter("input appenders exist duplicate(s)!");

    for (const auto& app: temp_appenders)
    {
        setAppender(app);
    }
}

inline void Logger::removeAppender(const std::shared_ptr<BaseAppender>& appender)
{
    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    for (auto it = appenders_.begin(); it != appenders_.end(); it++)
    {
        if (*it == appender)
        {
            appenders_.erase(it);
            return;
        }
    }

    /* if could not find, throw exception */
    throw aw_logger::invalid_parameter(
        std::string("appenders list did not set appender like: ") + typeid(*appender).name()
        + " before!"
    );
}

inline void Logger::clearAppenders()
{
    std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
    appenders_.clear();
}

inline void Logger::flush()
{
    /* avoid waiting on the worker's own appender call */
    if (active_worker_logger_ == this)
        return;

    Backend* state = nullptr;
    Logger::Ptr root;
    {
        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        state = backend_.get();
        if (appenders_.empty())
            root = root_logger_;
    }

    if (state != nullptr)
    {
        /* capture submission watermarks for this flush */
        const auto normal_target = state->normal.getWritePosition();
        const auto critical_target = state->critical.getWritePosition();
        std::unique_lock<std::mutex> wait_lk(state->flush_mtx);
        /* wait until both queues reach the captured watermarks */
        state->flush_cv.wait(wait_lk, [&] {
            return state->normal_completed.load(std::memory_order_acquire) >= normal_target
                && state->critical_completed.load(std::memory_order_acquire) >= critical_target;
        });
        wait_lk.unlock();

        /* serialize flush with worker appender calls */
        std::unique_lock<std::mutex> appender_lk(state->appender_call_mtx);
        std::vector<BaseAppender::Ptr> snapshot;
        {
            /* snapshot appenders once per flush */
            std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
            snapshot.assign(appenders_.begin(), appenders_.end());
        }
        /* flush each appender independently */
        for (const auto& app: snapshot)
        {
            try
            {
                app->flush();
            } catch (...)
            {
                /* keep other appenders available */
            }
        }
    }

    if (root != nullptr && root.get() != this)
        root->flush();
}

void Logger::init()
{
    start();
}

void Logger::start()
{
    Backend* state = nullptr;
    {
        /* keep queue state stable while finding the backend */
        std::shared_lock<std::shared_mutex> route_lock(rw_mtx_);
        state = backend_.get();
    }
    startWorker(state, true);
}

inline void Logger::startWorker(Backend* state, bool reopen)
{
    if (state == nullptr)
    {
        if (reopen)
            submission_state_.fetch_and(~stopped_mask_, std::memory_order_release);
        return;
    }

    /* serialize worker lifecycle transitions */
    std::lock_guard<std::mutex> worker_lk(state->worker_lifecycle_mtx);
    if (running_.load(std::memory_order_acquire))
        return;

    if (state->worker.joinable())
    {
        if (active_worker_logger_ == this)
            return;
        state->worker.join();
    }

    if (reopen)
        submission_state_.fetch_and(~stopped_mask_, std::memory_order_release);
    if (!isAccepting())
        return;

    /* publish running state before thread creation */
    running_.store(true, std::memory_order_release);
    try
    {
        state->worker = std::thread([this, state]() { workerLoop(state); });
    } catch (...)
    {
        running_.store(false, std::memory_order_release);
        throw;
    }
}

inline void Logger::workerLoop(Backend* state)
{
    active_worker_logger_ = this;

    const auto pop_event = [state](
                               RingBuffer<std::shared_ptr<LogEvent>>& queue,
                               const Logger::Policy policy,
                               LogEvent::Ptr& event
                           ) {
        /* serialize overwrite and blocking policies with producers */
        if (policy == Logger::Policy::overrun_oldest)
            return queue.popOverwriteOldest(event);

        if (policy == Logger::Policy::block)
        {
            bool popped = false;
            {
                std::lock_guard<std::mutex> space_lk(state->space_mtx);
                popped = queue.pop(event);
            }
            if (popped)
                state->space_cv.notify_all();
            return popped;
        }

        return queue.pop(event);
    };

    uint64_t observed_work_seq = state->work_seq.load(std::memory_order_seq_cst);
    while (true)
    {
        /* wait for new work or shutdown */
        while (state->normal.getSize() == 0 && state->critical.getSize() == 0)
        {
            if (!running_.load(std::memory_order_acquire))
                return;

            const auto current_work_seq = state->work_seq.load(std::memory_order_seq_cst);
            if (current_work_seq != observed_work_seq)
            {
                observed_work_seq = current_work_seq;
                continue;
            }
            state->work_seq.wait(observed_work_seq, std::memory_order_seq_cst);
        }

        std::vector<BaseAppender::Ptr> snapshot;
        {
            /* snapshot appenders once per batch */
            std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
            snapshot.assign(appenders_.begin(), appenders_.end());
        }

        std::unique_lock<std::mutex> appender_lk(state->appender_call_mtx);
        /* prefer critical events but reserve normal progress */
        size_t critical_budget = 8;
        size_t normal_budget = 1;
        /* 64 is the maximum number of events to process in a single batch */
        for (size_t n = 0; n < 64; ++n)
        {
            LogEvent::Ptr event;
            bool critical = false;
            if (critical_budget != 0 && pop_event(state->critical, options_.critical_policy, event))
            {
                --critical_budget;
                critical = true;
            }
            else if (normal_budget != 0 && pop_event(state->normal, options_.normal_policy, event))
            {
                critical_budget = 8;
                normal_budget = 1;
            }
            else if (pop_event(state->critical, options_.critical_policy, event))
            {
                critical_budget = 7;
                critical = true;
            }
            else if (pop_event(state->normal, options_.normal_policy, event))
            {
                critical_budget = 8;
                normal_budget = 0;
            }
            else
            {
                break;
            }

            /* append event to the stable batch snapshot */
            for (const auto& app: snapshot)
            {
                try
                {
                    app->append(event);
                } catch (...)
                {
                    /* keep other appenders available */
                }
            }

            /* complete event for flush */
            auto& completed = critical ? state->critical_completed : state->normal_completed;
            completed.fetch_add(1, std::memory_order_release);
            state->flush_cv.notify_all();
        }
        appender_lk.unlock();

        observed_work_seq = state->work_seq.load(std::memory_order_seq_cst);
        if (!running_.load(std::memory_order_acquire) && state->normal.getSize() == 0
            && state->critical.getSize() == 0)
        {
            /* stop after draining queued events */
            return;
        }
    }
}

inline void Logger::stop()
{
    if (active_worker_logger_ == this)
        throw aw_logger::invalid_parameter("stop from appender callback is invalid!");

    Backend* state = nullptr;
    {
        std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
        /* close producer admission before waking waiters */
        submission_state_.fetch_or(stopped_mask_, std::memory_order_acq_rel);
        state = backend_.get();
        if (state != nullptr)
        {
            /* wake blocked producers */
            std::lock_guard<std::mutex> space_lk(state->space_mtx);
            state->space_cv.notify_all();
        }
    }

    if (state == nullptr)
        return;

    /* wait for submissions that entered before admission closed */
    auto submission_state = submission_state_.load(std::memory_order_acquire);
    while ((submission_state & ~stopped_mask_) != 0)
    {
        submission_state_.wait(submission_state, std::memory_order_acquire);
        submission_state = submission_state_.load(std::memory_order_acquire);
    }

    /* serialize shutdown with a possible restart */
    std::lock_guard<std::mutex> worker_lk(state->worker_lifecycle_mtx);
    running_.store(false, std::memory_order_release);
    /* wake worker for shutdown */
    state->work_seq.fetch_add(1, std::memory_order_seq_cst);
    state->work_seq.notify_all();
    if (state->worker.joinable())
    {
        state->worker.join();
    }
}

inline bool Logger::beginSubmission() noexcept
{
    auto state = submission_state_.load(std::memory_order_acquire);
    while ((state & stopped_mask_) == 0)
    {
        if (submission_state_.compare_exchange_weak(
                state,
                state + 1,
                std::memory_order_acq_rel,
                std::memory_order_acquire
            ))
        {
            return true;
        }
    }
    return false;
}

inline void Logger::finishSubmission() noexcept
{
    submission_state_.fetch_sub(1, std::memory_order_release);
    submission_state_.notify_all();
}

inline bool Logger::isAccepting() const noexcept
{
    return (submission_state_.load(std::memory_order_acquire) & stopped_mask_) == 0;
}

inline LoggerManager::~LoggerManager()
{
    destroy();
}

inline Logger::Ptr LoggerManager::getLogger(const std::string& name)
{
    /* if just want to get root logger, return directly */
    if (name == "root")
    {
        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        return root_logger_;
    }

    /* find in loggers map */
    {
        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        auto it = loggers_map_.find(name);
        if (it != loggers_map_.end())
            return it->second;
    }

    /* if can't find, create new logger */
    /* copy root logger for thread-safe */
    Logger::Ptr copy_root_logger;
    {
        std::shared_lock<std::shared_mutex> read_lk(rw_mtx_);
        copy_root_logger = root_logger_;
    }

    Logger::Ptr logger = std::make_shared<Logger>(name);
    /* pass copy root logger instead of `this->root_logger_`, here we can use lock-free operation */
    logger->setRootLogger(copy_root_logger);

    /* lock again and check again, avoid another thread create it before */
    {
        std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
        auto [it, inserted] = loggers_map_.try_emplace(name, logger);
        /* if this logger has create, `inserted` == false and it won't be construct */
        if (!inserted)
            return it->second;
        else
            return logger;
    }
}

inline void LoggerManager::init()
{
    std::call_once(start_flag_, [this]() {
        std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
        root_logger_ = std::make_shared<Logger>("root");
        root_logger_->setAppender(std::make_shared<ConsoleAppender>());
        loggers_map_.emplace("root", root_logger_);
        root_logger_->init();
    });
}

inline void LoggerManager::destroy()
{
    /* swap out state under lock, then shut down loggers without holding the mutex */
    decltype(loggers_map_) local_map;
    Logger::Ptr local_root;

    {
        std::unique_lock<std::shared_mutex> write_lk(rw_mtx_);
        local_map.swap(loggers_map_);
        local_root.swap(root_logger_);
    }

    for (auto& [name, logger]: local_map)
    {
        if (logger)
        {
            logger->flush();
            logger->stop();
        }
    }
}

} // namespace aw_logger

#endif //! IMPL__LOGGER_IMPL_HPP
