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
#include "aw_logger/formatter.hpp"

namespace aw_logger {

explicit ComponentFactory::ComponentFactory()
{
    /* TODO (siyiya): load setting json first, and register component into `std::list` via setting json or default json */
}

inline void ComponentFactory::registerComponents(const nlohmann::json& json)
{
    /* TODO (siyiya): register component */
    for (const auto& component: json["log_event"])
    {
        if (component["enabled"].get<bool>())
        {
            const auto& type = component["type"];
            /* color */
            if (type == "color")
            {}
            /* source location */
            if (type == "loc")
            {}
            /* others */
        }
    }
}

inline Formatter::Formatter(const ComponentFactory& factory)
{
    /* TODO (siyiya): add formatting functions */
    factory_ = std::make_shared<ComponentFactory>(factory);
}

std::string Formatter::formatComponents(const std::map<std::string, std::string>& components)
{
    std::string result;

    /* TODO: format registered component */
    for (const auto& [type, format]: components)
    {
        if (type == "timestamp")
        {
            result += formatTimestamp();
        }
        else if (type == "level")
        {
            result += formatLevel();
        }
        else if (type == "tid")
        {
            result += formatThreadId();
        }
        else if (type == "loc")
        {
            result += formatSourceLocation(format);
        }
        else if (type == "msg")
        {
            result += formatMsg();
        }
    }
    return result;
}

void ComponentFactory::loadSettingComponents(std::string_view setting_file)
{
    std::ifstream ifs(setting_file.data());
    setting_json_ = nlohmann::json::parse(ifs);
    if (!setting_json_.contains("log_event"))
    {}

    for (const auto& component: setting_json_["log_event"])
    {
        if (!component.contains("type") || !component.contains("enabled"))
        {}
    }
    /* TODO：add exception handling, what about std::optional for format functions */

    ifs.close();
}

inline std::string Formatter::formatColor(std::string_view format)
{
    const auto& color_map = Color::getColorMap();
    /* default color is white */
    int r = 255, g = 255, b = 255;
    auto it = color_map.find(format);
    /* if color is found, convert hex to rgb */
    if (it != color_map.end())
    {
        /* std::tie allow to tie multiple variables as std::tuple */
        std::tie(r, g, b) = Color::convertHexToRGB(it->second);
    }
    else
    {
        fprintf(
            stderr,
            "[aw_logger]:unknown color: %s, use default color 'white' instead.",
            format.data()
        );
    }

    return Formatter::format("\033[38;2;{};{};{}m", r, g, b);
}

inline std::string Formatter::formatSourceLocation(std::string_view format)
{
    const auto& loc = event_->getSourceLocation();
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