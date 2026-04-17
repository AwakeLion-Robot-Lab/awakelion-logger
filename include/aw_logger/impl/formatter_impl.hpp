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

#ifndef IMPL__FORMATTER_IMPL_HPP
#define IMPL__FORMATTER_IMPL_HPP

// C++ standard library
#include <cctype>

// aw_logger library
#include "aw_logger/exception.hpp"
#include "aw_logger/formatter.hpp"

namespace aw_logger {

ComponentFactory::ComponentFactory()
{
    registerComponents(default_json_);
}

ComponentFactory::ComponentFactory(std::string_view pattern)
{
    parsePattern(pattern);
}

inline void ComponentFactory::registerComponents(const nlohmann::json& json)
{
    for (const auto& component: json["components"])
    {
        if (component["enabled"].get<bool>())
        {
            const auto& type = component["type"];
            /* timestamp */
            if (type == "timestamp")
                registered_components_.push_back({ "timestamp", "" });

            /* level */
            else if (type == "level")
                registered_components_.push_back({ "level", "" });

            /* thread id */
            else if (type == "tid")
                registered_components_.push_back({ "tid", "" });

            /* source location */
            else if (type == "loc")
                registered_components_.push_back({ "loc", component.value("format", "") });

            /* message */
            else if (type == "msg")
                registered_components_.push_back({ "msg", "" });
        }
    }
}

void ComponentFactory::parsePattern(std::string_view pattern)
{
    /* initialize state, left position and right position */
    /* vector of {type, unformatted data} */
    std::vector<std::pair<std::string, std::string>> pattern_components;
    const size_t size = pattern.size();
    patternState state = patternState::NORMAL_TEXT;
    size_t lpos = 0, rpos = 0;

    /* parse loop until EOL: '\0' */
    while (rpos <= size)
    {
        /* current char to judge whether '%' or EOL */
        auto curr_char = (rpos < size) ? pattern[rpos] : '\0';
        switch (state)
        {
            /* emplace normal text directly */
            case patternState::NORMAL_TEXT:
                /* if current char is '%' or EOL, emplace text to break or ready for parse pattern char */
                if (curr_char == '%' || curr_char == '\0')
                {
                    /**
                     * emplace back text between lpos and rpos which means char before '%'
                     * e.g.:
                     * - in case of '%', "abc%t", it emplace back "abc"
                     * - in case of EOL, "abcefgdasd\0", it emplace back "abcefgdasd"
                     */
                    if (lpos < rpos)
                        pattern_components.emplace_back(
                            "s",
                            std::string(pattern.substr(lpos, rpos - lpos))
                        );

                    if (curr_char == '%')
                    {
                        /**
                         * move `lpos` to next char of '%' for parsing pattern char
                         * e.g. `lpos` move to 't' of '%t'
                         */
                        lpos = rpos + 1;
                        /* switch state to parse pattern char */
                        state = patternState::PATTERN_CHAR;
                    }
                }
                break;

            /* parse pattern char */
            case patternState::PATTERN_CHAR:
                /* if current char is not alphabet or it's an EOL, break and switch to normal text */
                if (!std::isalpha(static_cast<unsigned char>(curr_char)))
                {
                    /* emplace pattern char if exists */
                    if (lpos < rpos)
                        pattern_components.emplace_back(
                            std::string(pattern.substr(lpos, rpos - lpos)),
                            ""
                        );

                    /* switch state to normal text parsing */
                    state = patternState::NORMAL_TEXT;
                    /* move `lpos` to current position for next normal text emplacing */
                    lpos = rpos;
                    continue;
                }
                break;
        }
        /* add `rpos` by 1 for next char in loop */
        rpos++;
    }

    /* ready for components registration */
    registered_components_.clear();
    registered_components_.reserve(pattern_components.size());

    for (const auto& [type, format]: pattern_components)
    {
        /* timestamp */
        if (type == "t")
            registered_components_.push_back({ "timestamp", "" });

        /* level */
        else if (type == "p")
            registered_components_.push_back({ "level", "" });

        /* thread id */
        else if (type == "i")
            registered_components_.push_back({ "tid", "" });

        /* file name */
        else if (type == "f")
            registered_components_.push_back({ "loc", "{file_name}" });

        /* function name */
        else if (type == "n")
            registered_components_.push_back({ "loc", "{function_name}" });

        /* line */
        else if (type == "l")
            registered_components_.push_back({ "loc", "{line}" });

        /* log message */
        else if (type == "m")
            registered_components_.push_back({ "msg", "" });

        /* text in pattern, e.g. timestamp:[%t] => timestamp:[1760000000] */
        else if (type == "s")
            registered_components_.push_back({ "text", format });
    }
}

inline Formatter::Formatter(ComponentFactory::Ptr factory):
    factory_(std::move(factory)),
    color_enabled_(true)
{
    resetLevelColors();
}

inline void Formatter::resetLevelColors()
{
    level_color_codes_.fill(formatColor("white"));

    static constexpr std::array<std::pair<LogLevel::level, std::string_view>, 6> defaults = {
        std::pair { LogLevel::level::DEBUG, "white" },
        std::pair { LogLevel::level::INFO, "cyan" },
        std::pair { LogLevel::level::NOTICE, "blue" },
        std::pair { LogLevel::level::WARN, "yellow" },
        std::pair { LogLevel::level::ERROR, "red" },
        std::pair { LogLevel::level::FATAL, "magenta" },
    };

    for (const auto& [lvl, name]: defaults)
    {
        setLevelColor(lvl, name);
    }
}

inline void Formatter::setLevelColor(LogLevel::level level, std::string_view color_name)
{
    level_color_codes_[levelToIndex(level)] = formatColor(color_name);
}

inline void Formatter::formatComponentsTo(
    std::string& out,
    const LogEvent::Ptr& event,
    const std::vector<std::pair<std::string, std::string>>& components
)
{
    /* validate log event pointer */
    if (event == nullptr)
        throw aw_logger::invalid_parameter("log event pointer is nullptr!");

    try
    {
        const std::string& color_code = colorForLevel(event->getLogLevel());
        const bool is_has_color_code = color_enabled_ && !color_code.empty();
        auto inserter = std::back_inserter(out);

        for (const auto& [type, format]: components)
        {
            if (type == "timestamp")
            {
                std::format_to(inserter, "[{}]", event->getTimestamp());
            }
            else if (type == "level")
            {
                if (is_has_color_code)
                    out.append(color_code);

                std::format_to(inserter, "[{}]", event->getLogLevelString());

                if (is_has_color_code)
                    out.append(aw_logger::Color::endColor);
            }
            else if (type == "tid")
            {
                std::format_to(inserter, "[tid: {}]", event->getThreadId());
            }
            else if (type == "loc")
            {
                formatSourceLocation(out, event, format);
            }
            else if (type == "msg")
            {
                if (is_has_color_code)
                    out.append(color_code);

                out.append(event->getMsg());

                if (is_has_color_code)
                    out.append(aw_logger::Color::endColor);
            }
            else if (type == "text")
            {
                out.append(format);
            }
        }
    } catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n' << std::endl;
    }
}

inline std::string Formatter::formatColor(std::string_view format)
{
    const auto& color_map = Color::getColorMap();
    /* default color is white */
    int r = 255, g = 255, b = 255;

    auto it = color_map.find(format);
    /* if color is found, convert hex to rgb */
    if (it != color_map.end())
        /* std::tie allow to tie multiple variables as std::tuple */
        std::tie(r, g, b) = Color::convertHexToRGB(it->second);

    return Formatter::vformat("\033[38;2;{};{};{}m", r, g, b);
}

inline void
Formatter::formatSourceLocation(std::string& out, const LogEvent::Ptr& event, std::string_view format)
{
    auto const& loc = event->getSourceLocation();
    size_t prev_pos = 0, pos = 0;

    while ((pos = format.find('{', prev_pos)) != std::string_view::npos)
    {
        out.append(format.data() + prev_pos, pos - prev_pos);

        /* match placeholders */
        if (format.compare(pos, 11, "{file_name}") == 0)
        {
            out.append(loc.file_name());
            prev_pos = pos + 11;
        }
        else if (format.compare(pos, 15, "{function_name}") == 0)
        {
            out.append(loc.function_name());
            prev_pos = pos + 15;
        }
        else if (format.compare(pos, 6, "{line}") == 0)
        {
            std::format_to(std::back_inserter(out), "{}", loc.line());
            prev_pos = pos + 6;
        }
        else
        {
            out.push_back(format[pos]);
            prev_pos = pos + 1;
        }
    }

    out.append(format.data() + prev_pos, format.size() - prev_pos);
}

} // namespace aw_logger

#endif //! IMPL__FORMATTER_IMPL_HPP
