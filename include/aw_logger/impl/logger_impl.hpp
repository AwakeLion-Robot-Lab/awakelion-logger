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
#ifndef IMPL__LOGGER_IMPL_HPP
#define IMPL__LOGGER_IMPL_HPP

// aw_logger library
#include "aw_logger/logger.hpp"

namespace aw_logger {
Filter::Filter() {}

template<typename... Args>
inline void aw_logger::Filter::setFilter(std::string_view s, Args&&... args)
{}
} // namespace aw_logger

#endif //! IMPL__LOGGER_IMPL_HPP