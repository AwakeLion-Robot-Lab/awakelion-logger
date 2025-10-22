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
#include <fstream>

// aw_logger library
#include "aw_logger/config_path.h"
#include "aw_logger/exception.hpp"
#include "aw_logger/formatter.hpp"

namespace aw_logger {

ComponentFactory::ComponentFactory()
{
    const std::string setting_path = CONFIG_FILE_PATH;
    loadSettingComponents(setting_path);
    registerComponents(setting_json_);
}

void ComponentFactory::loadSettingComponents(std::string_view file_name)
{
    const auto file_name_s = std::string(file_name);
    std::ifstream setting_file(file_name_s);

    if (!setting_file.is_open())
        throw aw_logger::invalid_parameter(
            std::string("can not open setting file: ") + file_name_s
        );

    setting_json_ = nlohmann::json::parse(setting_file);
    setting_file.close();

    if (!setting_json_.contains("components"))
        setting_json_ = default_json_;
}

inline void ComponentFactory::registerComponents(const nlohmann::json& json)
{
    for (const auto& component: json["components"])
    {
        if (component["enabled"].get<bool>())
        {
            const auto& type = component["type"];
            /* color */
            if (type == "color")
                registered_components_.push_back({ "color", component["level_colors"].dump(4) });

            /* timestamp */
            if (type == "timestamp")
                registered_components_.push_back({ "timestamp", "" });

            /* level */
            if (type == "level")
                registered_components_.push_back({ "level", "" });

            /* thread id */
            if (type == "tid")
                registered_components_.push_back({ "tid", "" });

            /* source location */
            if (type == "loc")
                registered_components_.push_back({ "loc", component.value("format", "") });

            /* message */
            if (type == "msg")
                registered_components_.push_back({ "msg", "" });
        }
    }
}

inline Formatter::Formatter(const ComponentFactory::Ptr& factory)
{
    setFactory(factory);
}

std::string Formatter::formatComponents(
    const LogEvent::Ptr& event,
    const std::vector<std::pair<std::string, std::string>>& components
)
{
    /* validate log event pointer */
    if (event == nullptr)
        throw aw_logger::invalid_parameter("log event pointer is nullptr!");

    std::string result;
    result.reserve(event->getMsg().size() + 256);

    /* pre-scan to find color settings */
    std::string color_code;
    for (const auto& [type, format]: components)
    {
        if (type == "color")
        {
            const auto level_colors = nlohmann::json::parse(format);
            std::string level_str = event->getLogLevelString();

            /* convert to lowercase for JSON key-value pair matching */
            std::transform(level_str.begin(), level_str.end(), level_str.begin(), ::tolower);

            if (level_colors.contains(level_str))
            {
                color_code = formatColor(level_colors[level_str].get<std::string>());
            }
            break;
        }
    }

    try
    {
        /* if has color code, just format level and log message */
        const bool is_has_color_code = !color_code.empty();

        for (const auto& [type, format]: components)
        {
            if (type == "timestamp")
            {
                result += formatTimestamp(event);
            }
            else if (type == "level")
            {
                if (is_has_color_code)
                {
                    result += color_code;
                }
                result += formatLevel(event);
                if (is_has_color_code)
                {
                    result += aw_logger::Color::endColor;
                }
            }
            else if (type == "tid")
            {
                result += formatThreadId(event);
            }
            else if (type == "loc")
            {
                result += formatSourceLocation(event, format);
            }
            else if (type == "color")
            {
                /* skip color component, already handled in pre-scan */
                continue;
            }
            else if (type == "msg")
            {
                if (is_has_color_code)
                {
                    result += color_code;
                }
                result += formatMsg(event);
                if (is_has_color_code)
                {
                    result += aw_logger::Color::endColor;
                }
            }
        }
    } catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n' << std::endl;
    }
    return result;
}

inline std::string Formatter::formatColor(std::string_view format)
{
    const auto& color_map = Color::getColorMap();
    /* default color is white */
    int r = 255, g = 255, b = 255;

    auto it = color_map.find(format);
    try
    {
        /* if color is found, convert hex to rgb */
        if (it != color_map.end())
            /* std::tie allow to tie multiple variables as std::tuple */
            std::tie(r, g, b) = Color::convertHexToRGB(it->second);
        else
        {
            throw aw_logger::invalid_parameter(
                std::string("Color ") + std::string(format)
                + " not found, use default color 'white' instead."
            );
        }
    } catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n' << std::endl;
    }

    return Formatter::vformat("\033[38;2;{};{};{}m", r, g, b);
}

inline std::string
Formatter::formatSourceLocation(const LogEvent::Ptr& event, std::string_view format)
{
    auto const& loc = event->getSourceLocation();
    std::string result;
    result.reserve(format.size() + 100);
    size_t prev_pos = 0, pos = 0;

    while ((pos = format.find('{', prev_pos)) != std::string_view::npos)
    {
        result.append(format.data() + prev_pos, pos - prev_pos);

        /* match placeholders */
        if (format.compare(pos, 11, "{file_name}") == 0)
        {
            result += loc.file_name();
            prev_pos = pos + 11;
        }
        else if (format.compare(pos, 15, "{function_name}") == 0)
        {
            result += loc.function_name();
            prev_pos = pos + 15;
        }
        else if (format.compare(pos, 6, "{line}") == 0)
        {
            result += std::to_string(loc.line());
            prev_pos = pos + 6;
        }
        else
        {
            result += format[pos];
            prev_pos = pos + 1;
        }
    }

    result.append(format.data() + prev_pos, format.size() - prev_pos);
    return Formatter::vformat("{}", result);
}

} // namespace aw_logger

#endif //! IMPL__FORMATTER_IMPL_HPP
