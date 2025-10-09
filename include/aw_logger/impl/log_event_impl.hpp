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

#ifndef IMPL__LOG_EVENT_IMPL_HPP
#define IMPL__LOG_EVENT_IMPL_HPP

// linux library
#include <sys/stat.h>
#include <sys/types.h>

// aw_logger library
#include "aw_logger/log_event.hpp"

namespace aw_logger {
LogEvent::LogEvent(LogLevel::level level, LocalSourceLocation<std::string> wrapped_msg):
    msg_(wrapped_msg.getData()),
    level_(level),
    timestamp_({ std::chrono::current_zone(), std::chrono::system_clock::now() }),
    loc_(wrapped_msg.getLocation()),
    thread_id_(LogEvent::getThreadId())
{}

inline const size_t LogEvent::getThreadId() const noexcept
{
    static thread_local size_t thread_id = _getThreadId();
    return thread_id;
}

inline const size_t LogEvent::_getThreadId() const noexcept
{
#ifdef _WIN32
    return static_cast<size_t>(::GetCurrentThreadId());
#elif defined(__linux__)
    #if defined(__ANDROID__) && defined(__ANDROID_API__) && (__ANDROID_API__ < 21)
        #define SYS_gettid __NR_gettid
    #endif
    return static_cast<size_t>(::syscall(SYS_gettid));
#elif defined(_AIX)
    struct __pthrdsinfo buf;
    int reg_size = 0;
    pthread_t pt = pthread_self();
    int retval = pthread_getthrds_np(&pt, PTHRDSINFO_QUERY_TID, &buf, sizeof(buf), NULL, &reg_size);
    int tid = (!retval) ? buf.__pi_tid : 0;
    return static_cast<size_t>(tid);
#elif defined(__DragonFly__) || defined(__FreeBSD__)
    return static_cast<size_t>(::pthread_getthreadid_np());
#elif defined(__NetBSD__)
    return static_cast<size_t>(::_lwp_self());
#elif defined(__OpenBSD__)
    return static_cast<size_t>(::getthrid());
#elif defined(__sun)
    return static_cast<size_t>(::thr_self());
#elif __APPLE__
    uint64_t tid;
    // There is no pthread_threadid_np prior to Mac OS X 10.6, and it is not supported on any PPC,
    // including 10.6.8 Rosetta. __POWERPC__ is Apple-specific define encompassing ppc and ppc64.
    #ifdef MAC_OS_X_VERSION_MAX_ALLOWED
    {
        #if (MAC_OS_X_VERSION_MAX_ALLOWED < 1060) || defined(__POWERPC__)
        tid = pthread_mach_thread_np(pthread_self());
        #elif MAC_OS_X_VERSION_MIN_REQUIRED < 1060
        if (&pthread_threadid_np)
        {
            pthread_threadid_np(nullptr, &tid);
        }
        else
        {
            tid = pthread_mach_thread_np(pthread_self());
        }
        #else
        pthread_threadid_np(nullptr, &tid);
        #endif
    }
    #else
    pthread_threadid_np(nullptr, &tid);
    #endif
    return static_cast<size_t>(tid);
#else // Default to standard C++11 (other Unix)
    return static_cast<size_t>(std::hash<std::thread::id>()(std::this_thread::get_id()));
#endif
}
} // namespace aw_logger

#endif //! IMPL__LOG_EVENT_IMPL_HPP
