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
#include <filesystem>
#include <fstream>

// aw_logger library
#include "aw_logger/exception.hpp"
#include "aw_logger/formatter.hpp"

namespace aw_logger {

explicit ComponentFactory::ComponentFactory()
{
    /* TODO (siyiya): load setting json first, and register component via setting json or default json */
    const auto current_path = std::filesystem::current_path();
    const auto setting_path = (current_path / "../../../config/aw_logger_settings.json").string();
    loadSettingComponents(setting_path);
    registerComponents(setting_json_);
}

void ComponentFactory::loadSettingComponents(std::string_view file_name)
{
    try
    {
        std::ifstream setting_file(file_name.data());

        if (!setting_file.is_open())
            throw aw_logger::invalid_parameter(
                std::string("can not open setting file: ") + file_name.data()
            );

        setting_json_ = nlohmann::json::parse(setting_file);
        setting_file.close();

        if (!setting_json_.contains("log_event"))
            throw aw_logger::bad_json(
                "can not find 'log_event' field in setting json, use default json instead."
            );

    } catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n' << std::endl;
        setting_json_ = default_json_;
    }
}

inline void ComponentFactory::registerComponents(const nlohmann::json& json)
{
    for (const auto& component: json["log_event"])
    {
        if (component["enabled"].get<bool>())
        {
            const auto& type = component["type"];
            /* color */
            if (type == "color")
                registered_components_.emplace("color", component["level_colors"].dump(4));

            /* timestamp */
            if (type == "timestamp")
                registered_components_.emplace("timestamp", "");

            /* level */
            if (type == "level")
                registered_components_.emplace("level", "");

            /* thread id */
            if (type == "tid")
                registered_components_.emplace("tid", "");

            /* source location */
            if (type == "loc")
                registered_components_.emplace("loc", component.value("format", ""));

            /* message */
            if (type == "msg")
                registered_components_.emplace("msg", "");
        }
    }
}

inline Formatter::Formatter(const ComponentFactory& factory)
{
    factory_ = std::make_shared<ComponentFactory>(factory);
}

std::string Formatter::formatComponents(
    LogEvent::ConstPtr& event,
    const std::map<std::string, std::string>& components
)
{
    /* validate log event pointer */
    if (event == nullptr)
        throw aw_logger::invalid_parameter("log event pointer is nullptr!");

    std::string result;
    result.reserve(512);

    for (const auto& [type, format]: components)
    {
        if (type == "timestamp")
        {
            result += formatTimestamp(event);
        }
        else if (type == "level")
        {
            result += formatLevel(event);
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
            const auto level_colors = nlohmann::json::parse(format);
            const auto level_str = event->getLogLevelString();

            if (level_colors.contains(level_str))
                result += formatColor(level_colors[level_str].get<std::string>());
        }
        else if (type == "msg")
        {
            result += formatMsg(event);
            result += aw_logger::Color::endColor;
        }
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
                std::string("Color ") + format.data()
                + " not found, use default color 'white' instead."
            );
        }
    } catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n' << std::endl;
    }

    return Formatter::format("\033[38;2;{};{};{}m", r, g, b);
}

inline std::string
Formatter::formatSourceLocation(LogEvent::ConstPtr& event, std::string_view format)
{
    const auto& loc = event->getSourceLocation();
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
    return result;
}

} // namespace aw_logger

#endif //! IMPL__FORMATTER_IMPL_HPP
