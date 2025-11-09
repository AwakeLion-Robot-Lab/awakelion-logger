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

#ifndef IMPL__FILE_APPENDER_IMPL_HPP
#define IMPL__FILE_APPENDER_IMPL_HPP

// aw_logger library
#include "aw_logger/appender.hpp"

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {
inline FileAppender::FileAppender(
    std::string_view file_path,
    bool is_trunc,
    size_t buffer_capacity
):
    file_path_(file_path),
    buffer_(),
    file_size_(0),
    max_file_size_(0),
    max_backup_num_(5),
    is_trunc_(is_trunc)
{
    /* reserve buffer capacity without initializing */
    buffer_.reserve(buffer_capacity);

    /* get current file size if exists the file and is not truncated */
    if (std::filesystem::exists(file_path_) && !is_trunc_)
        file_size_ = std::filesystem::file_size(file_path_);

    open(is_trunc_);
}

inline FileAppender::FileAppender(
    Formatter::Ptr formatter,
    std::string_view file_path,
    bool is_trunc,
    size_t buffer_capacity
):
    BaseAppender(std::move(formatter)),
    file_path_(file_path),
    buffer_(),
    file_size_(0),
    max_file_size_(0),
    max_backup_num_(5),
    is_trunc_(is_trunc)
{
    buffer_.reserve(buffer_capacity);

    if (std::filesystem::exists(file_path_) && !is_trunc_)
        file_size_ = std::filesystem::file_size(file_path_);

    open(is_trunc_);
}

inline FileAppender::~FileAppender()
{
    flush();

    if (file_stream_.is_open())
        file_stream_.close();
}

inline void FileAppender::open(bool is_trunc)
{
    /* check file stream */
    if (file_stream_.is_open())
    {
        file_stream_.flush();
        file_stream_.close();
    }

    /* if directory did not exist, create one */
    if (!file_path_.parent_path().empty())
        std::filesystem::create_directories(file_path_.parent_path());

    /**
     * select open mode
     * NOTE that if did not exist file, ofstream will create automatically
     */
    auto open_mode =
        (std::ios::out | std::ios::binary) | (is_trunc ? std::ios::trunc : std::ios::app);
    file_stream_.open(file_path_, open_mode);

    if (!file_stream_.is_open())
        throw aw_logger::aw_logger_exception("can not open file: " + file_path_.string());

    if (is_trunc)
        file_size_ = 0;
}

void FileAppender::append(const LogEvent::Ptr& event)
{
    auto log_msg = formatMsg(event);
    /* make sure that it has EOF */
    if (log_msg.empty() || log_msg.back() != '\n')
        log_msg.push_back('\n');
    const auto log_msg_size = log_msg.size();

    std::lock_guard<std::mutex> app_lk(app_mtx_);
    /* check if buffer needs flush before append */
    if (buffer_.capacity() == 0)
    {
        /* if file stream is close, reopen it in append mode */
        if (!file_stream_.is_open())
            open(false);

        /* write into file and record file size */
        file_stream_.write(log_msg.data(), static_cast<std::streamsize>(log_msg_size));
        if (!file_stream_.good())
            throw aw_logger::aw_logger_exception("failed to write to file: " + file_path_.string());
        file_size_ += log_msg_size;

        /* if file size is greater than max file size, rotate */
        if (max_file_size_ > 0 && file_size_ >= max_file_size_)
            rotateFile();
        return;
    }

    /* if buffer is gonna full, flush it */
    if (buffer_.size() + log_msg_size > buffer_.capacity())
        flushToBuffer();

    /* if not, just append to buffer */
    buffer_.append(log_msg);
}

inline void FileAppender::flush()
{
    std::lock_guard<std::mutex> app_lk(app_mtx_);
    flushToBuffer();

    if (file_stream_.is_open())
        file_stream_.flush();
}

inline void FileAppender::reopen(bool is_trunc)
{
    std::lock_guard<std::mutex> app_lk(app_mtx_);
    flushToBuffer();
    open(is_trunc);

    if (!is_trunc && std::filesystem::exists(file_path_))
        file_size_ = std::filesystem::file_size(file_path_);
}

inline void FileAppender::flushToBuffer()
{
    /* check buffer size */
    if (buffer_.empty())
        return;

    if (!file_stream_.is_open())
        open(false);

    /* write into file and record file size */
    file_stream_.write(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
    if (!file_stream_.good())
        throw aw_logger::aw_logger_exception("failed to write to file: " + file_path_.string());
    file_size_ += buffer_.size();

    /* clear buffer */
    buffer_.clear();

    /* check if file needs rotated to new log file */
    if (max_file_size_ > 0 && file_size_ >= max_file_size_)
        rotateFile();
}

inline void FileAppender::rotateFile()
{
    /* flush and close current file stream */
    file_stream_.flush();
    file_stream_.close();

    /* rotate backup files: filename_backupN.ext -> filename_backup(N+1).ext */
    if (max_backup_num_ > 0)
    {
        /* delete oldest backup if exists */
        std::filesystem::path oldest_backup = createBackupPath(max_backup_num_);
        if (std::filesystem::exists(oldest_backup))
            std::filesystem::remove(oldest_backup);

        /* rename existing backups: backup(N-1) -> backupN in the loop */
        for (size_t i = max_backup_num_; i > 1; i--)
        {
            const auto src = createBackupPath(i - 1);
            if (std::filesystem::exists(src))
                std::filesystem::rename(src, createBackupPath(i));
        }

        /* rename current file to the first backup */
        if (std::filesystem::exists(file_path_))
            std::filesystem::rename(file_path_, createBackupPath(1));
    }
    else if (std::filesystem::exists(file_path_))
    {
        /* if no backup limit, just remove current file */
        std::filesystem::remove(file_path_);
    }

    /* reset file size and open new file */
    file_size_ = 0;
    /* open in truncate mode for clear new file first */
    open(true);
}

inline std::filesystem::path FileAppender::createBackupPath(size_t index) const noexcept
{
    return file_path_.parent_path()
        / (file_path_.stem().string() + "_backup" + std::to_string(index)
           + file_path_.extension().string());
}

} // namespace aw_logger

#endif //! IMPL__FILE_APPENDER_IMPL_HPP
