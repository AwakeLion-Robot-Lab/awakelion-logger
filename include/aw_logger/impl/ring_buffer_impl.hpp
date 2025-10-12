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

#ifndef IMPL__RING_BUFFER_IMPL_HPP
#define IMPL__RING_BUFFER_IMPL_HPP

// aw_logger library
#include "aw_logger/ring_buffer.hpp"

namespace aw_logger {
template<typename DataT, typename Allocator>
inline RingBuffer<DataT, Allocator>::RingBuffer(size_t capacity):
    buffer_(nullptr),
    alloc_(),
    wIdx_(0),
    rIdx_(0),
    capacity_(0),
    mirror_capacity_(0)
{
    /* judge size */
    if (capacity == 0)
        throw aw_logger::invalid_parameter("capacity must be greater than 0!");

    const auto e_size = sizeof(DataT);
    if (capacity > (std::numeric_limits<size_t>::max() / e_size))
        throw aw_logger::invalid_parameter("requested capacity too large!");

    /* align storage to 4 bytes */
    const auto aligned_storage = ALIGN4(capacity * e_size);
    capacity_ = aligned_storage / e_size;

    if ((capacity_ << 1) > std::numeric_limits<size_t>::max())
        throw aw_logger::invalid_parameter("requested mirror capacity too large!");
    mirror_capacity_ = capacity_ << 1;

    /* allocate memory */
    buffer_ = allocator_trait::allocate(alloc_, capacity_);
}

template<typename DataT, typename Allocator>
RingBuffer<DataT, Allocator>::~RingBuffer()
{
    const size_t curr_wIdx = wIdx_.load(std::memory_order_acquire);
    const size_t curr_rIdx = rIdx_.load(std::memory_order_acquire);
    const size_t used = (curr_wIdx >= curr_rIdx) ? (curr_wIdx - curr_rIdx)
                                                 : (curr_wIdx + mirror_capacity_ - curr_rIdx);

    // if ringbuffer is not empty
    if (used > 0)
    {
        auto head = toPtr(curr_rIdx);
        const size_t no_wrap_mem = std::min(used, capacity_ - head);
        const size_t wrap_mem = used - no_wrap_mem;

        /* destroy no-wrap memory */
        for (size_t i = 0; i < no_wrap_mem; i++)
        {
            allocator_trait::destroy(alloc_, buffer_ + head + i);
        }

        /* destroy wrap memory if indices did */
        if (wrap_mem > 0)
        {
            for (size_t i = 0; i < wrap_mem; i++)
            {
                allocator_trait::destroy(alloc_, buffer_ + i);
            }
        }
    }

    /* free ringbuffer */
    if (buffer_ != nullptr)
    {
        allocator_trait::deallocate(alloc_, buffer_, capacity_);
        capacity_ = 0;
        mirror_capacity_ = 0;
    }
}

template<typename DataT, typename Allocator>
template<typename U>
bool RingBuffer<DataT, Allocator>::push(U&& data)
{
    try
    {
        /* check if ring buffer is valid */
        if (buffer_ == nullptr || capacity_ == 0)
            return false;

        /* check if the buffer is full, NOTE here input data will be discarded instead of force-covering */

        // here use `std::memory_order_relaxed` 'cause ONLY producer can update write index
        const size_t curr_wIdx = wIdx_.load(std::memory_order_relaxed);
        // here use `std::memory_order_acquire` 'cause ONLY consumer can update read index
        const size_t curr_rIdx = rIdx_.load(std::memory_order_acquire);
        const size_t used = (curr_wIdx >= curr_rIdx) ? (curr_wIdx - curr_rIdx)
                                                     : (curr_wIdx + mirror_capacity_ - curr_rIdx);

        if (used >= capacity_)
            throw aw_logger::ringbuffer_exception("ring buffer is full!");
    } catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n' << std::endl;
        return false;
    }

    /**
     *  below is the normal way for placement new
     *  new& buffer_[wPtr] DataT(std::forward<U>(data));
     *  we use std::allocator::construct() instead
     * NOTE that the second parameter is same to &buffer_[wPtr], but prior is faster 'cause operator[] also take some time
    */
    auto wPtr = toPtr(curr_wIdx);
    allocator_trait::construct(alloc_, buffer_ + wPtr, std::forward<U>(data));

    /* update atomic write index */
    size_t next_wIdx = curr_wIdx + 1;
    if (next_wIdx >= mirror_capacity_) // here use `capacity_ << 1` instead of `2 * capacity_`
        next_wIdx -= mirror_capacity_; // wrap-around
    wIdx_.store(next_wIdx, std::memory_order_release);

    return true;
}

template<typename DataT, typename Allocator>
bool RingBuffer<DataT, Allocator>::pop(value_type& data)
{
    try
    {
        /* check if ring buffer is valid */
        if (buffer_ == nullptr || capacity_ == 0)
            throw aw_logger::ringbuffer_exception("ring buffer is not initialized!");

        /* check if ring buffer is empty */
        const size_t curr_rIdx = rIdx_.load(std::memory_order_relaxed);
        const size_t curr_wIdx = wIdx_.load(std::memory_order_acquire);
        const size_t used = (curr_wIdx >= curr_rIdx) ? (curr_wIdx - curr_rIdx)
                                                     : (curr_wIdx + mirror_capacity_ - curr_rIdx);

        if (used == 0)
            throw aw_logger::ringbuffer_exception("ring buffer is empty!");

    } catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n' << std::endl;
        return false;
    }

    /* move and destroy */
    auto rPtr = toPtr(curr_rIdx);
    data = std::move(buffer_[rPtr]);
    allocator_trait::destroy(alloc_, buffer_ + rPtr);

    size_t next_rIdx = curr_rIdx + 1;
    if (next_rIdx >= mirror_capacity_)
        next_rIdx -= mirror_capacity_;
    rIdx_.store(next_rIdx, std::memory_order_release);

    return true;
}

template<typename DataT, typename Allocator>
inline const size_t RingBuffer<DataT, Allocator>::getSize() const noexcept
{
    const size_t curr_wIdx = wIdx_.load(std::memory_order_acquire);
    const size_t curr_rIdx = rIdx_.load(std::memory_order_acquire);
    return (curr_wIdx >= curr_rIdx) ? (curr_wIdx - curr_rIdx)
                                    : (curr_wIdx + mirror_capacity_ - curr_rIdx);
}

} // namespace aw_logger

#endif //! IMPL__RING_BUFFER_IMPL_HPP
