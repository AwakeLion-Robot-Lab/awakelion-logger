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
    alloc_(allocator_type()),
    wIdx_(0),
    rIdx_(0),
    mask_(roundUpPow2(capacity) - 1)
{
    /* judge size */
    const size_t r_capacity = mask_ + 1;
    if (r_capacity < 2)
        throw aw_logger::invalid_parameter("capacity must be greater than 1!");

    const size_t e_size = sizeof(DataT);
    if (r_capacity > (std::numeric_limits<size_t>::max() / e_size))
        throw aw_logger::invalid_parameter("requested capacity too large!");

    /* allocate buffer */
    buffer_ = allocator_trait::allocate(alloc_, r_capacity);
    for (size_t i = 0; i < r_capacity; i++)
    {
        /* construct empty cell */
        allocator_trait::construct(alloc_, buffer_ + i);
        /* initialize sequence */
        (buffer_ + i)->sequence_.store(i, std::memory_order_relaxed);
    }
    /* initialize write/read index */
    wIdx_.store(0, std::memory_order_relaxed);
    rIdx_.store(0, std::memory_order_relaxed);
}

template<typename DataT, typename Allocator>
RingBuffer<DataT, Allocator>::~RingBuffer()
{
    if (buffer_ != nullptr)
    {
        const size_t capacity = mask_ + 1;
        /* free cells */
        for (size_t i = 0; i < capacity; ++i)
        {
            allocator_trait::destroy(alloc_, buffer_ + i);
        }
        /* free memory of buffer */
        allocator_trait::deallocate(alloc_, buffer_, capacity);
    }
}

template<typename DataT, typename Allocator>
template<typename U>
bool RingBuffer<DataT, Allocator>::push(U&& data)
{
    /* check if ring buffer is valid */
    if (buffer_ == nullptr)
        return false;

    /* here use `std::memory_order_relaxed` 'cause ONLY producer can update write index */
    size_t curr_wIdx = wIdx_.load(std::memory_order_relaxed);
    cell_t* curr_cell;

    /* loop until ready for write */
    while (true)
    {
        curr_cell = buffer_ + toPtr(curr_wIdx);

        /* we gotta get this sequence number, so this is a read operation, use `std::memory_order_acquire` */
        size_t curr_seq = curr_cell->sequence_.load(std::memory_order_acquire);
        /* `intptr_t` can convert to sign type, in order to detect overflow */
        intptr_t used_size = static_cast<intptr_t>(curr_seq) - static_cast<intptr_t>(curr_wIdx);

        /* this cell is ready for write */
        if (used_size == 0)
        {
            /* wIdx_ update to next index if equal to curr_wIdx */
            if (wIdx_.compare_exchange_weak(curr_wIdx, curr_wIdx + 1, std::memory_order_relaxed))
                break;
        }
        /**
         * this cell has already been written BUT NOT read(read operation + (mask + 1))
         * which means ringbuffer is full
        */
        else if (used_size < 0)
        {
            return false;
        }
        /* another write thread is writing this cell, load again and retry */
        else
        {
            curr_wIdx = wIdx_.load(std::memory_order_relaxed);
        }
    }

    /* update members of current cell */
    curr_cell->data_ = std::forward<U>(data);
    /* write operation；sequence = curr_wIdx + 1 */
    curr_cell->sequence_.store(curr_wIdx + 1, std::memory_order_release);

    return true;
}

template<typename DataT, typename Allocator>
bool RingBuffer<DataT, Allocator>::pop(value_t& data)
{
    /* check if ring buffer is valid */
    if (buffer_ == nullptr)
        return false;

    /* here use `std::memory_order_acquire` 'cause ONLY consumer can update read index */
    size_t curr_rIdx = rIdx_.load(std::memory_order_relaxed);
    cell_t* curr_cell;

    /* loop until ready for read */
    while (true)
    {
        curr_cell = buffer_ + toPtr(curr_rIdx);

        size_t curr_seq = curr_cell->sequence_.load(std::memory_order_acquire);
        /* here curr_rIdx + 1 means EXPECTED after write operation */
        intptr_t used_size = static_cast<intptr_t>(curr_seq) - static_cast<intptr_t>(curr_rIdx + 1);

        if (used_size == 0)
        {
            /* rIdx_ update to next index if equal to curr_rIdx */
            if (rIdx_.compare_exchange_weak(curr_rIdx, curr_rIdx + 1, std::memory_order_relaxed))
                break;
        }
        /* here means all the data has been read */
        else if (used_size < 0)
        {
            return false;
        }
        /* producer is writing */
        else
        {
            curr_rIdx = rIdx_.load(std::memory_order_relaxed);
        }
    }

    data = std::move(curr_cell->data_);
    /* read operation；sequence = curr_wIdx + mask_ + 1 */
    curr_cell->sequence_.store(curr_rIdx + mask_ + 1, std::memory_order_release);

    return true;
}

template<typename DataT, typename Allocator>
inline const size_t RingBuffer<DataT, Allocator>::getSize() const noexcept
{
    const size_t curr_wIdx = wIdx_.load(std::memory_order_acquire);
    const size_t curr_rIdx = rIdx_.load(std::memory_order_acquire);
    return (curr_wIdx >= curr_rIdx) ? (curr_wIdx - curr_rIdx) : (curr_wIdx + mask_ + 1 - curr_rIdx);
}

} // namespace aw_logger

#endif //! IMPL__RING_BUFFER_IMPL_HPP
