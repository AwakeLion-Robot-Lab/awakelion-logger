
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

#ifndef FORMATTER_HPP
#define FORMATTER_HPP

// C++ standard library
#include <map>

// nlohmann JSON library
#include <nlohmann/json.hpp>

// aw_logger library
#include "aw_logger/log_event.hpp"

/***
 * @brief a low-latency, high-throughput and few-dependency logger for `AwakeLion Robot Lab` project
 * @note fundamental structure is inspired by [sylar logger](https://github.com/sylar-yin/sylar) and implement is
 * inspired by [log4j2](https://logging.apache.org/log4j/2.12.x/) and [minilog](https://github.com/archibate/minilog)
 * @author jinhua "siyiovo" deng
 */
namespace aw_logger {

/***
 * @brief component factory to manager component registration via json file
 */
class ComponentFactory {
public:
    using Ptr = std::shared_ptr<ComponentFactory>;
    using ConstPtr = const std::shared_ptr<ComponentFactory>;

    /***
     * @brief constructor
     */
    explicit ComponentFactory();

    /***
     * @brief get registered components
     * @return registered components
     */
    auto getRegisteredComponents() -> std::map<std::string, std::string>
    {
        return registered_components_;
    }

private:
    /***
     * @brief log event format json from `logger_settings.json`
     */
    nlohmann::json setting_json_;

    /***
     * @brief default log event format
     */
    nlohmann::json default_json_ = { "log_event",
                                     { { { "type", "timestamp" }, { "enabled", true } },
                                       { { "type", "level" }, { "enabled", true } },
                                       { { "type", "tid" }, { "enabled", true } },
                                       { { "type", "loc" },
                                         { "format", "[{file_name}:{function_name}:{line}]" },
                                         { "enabled", true } },
                                       { { "type", "msg" }, { "enabled", true } },
                                       { { "type", "color" },
                                         { "level_colors",
                                           { { "debug", "white" },
                                             { "info", "cyan" },
                                             { "notice", "blue" },
                                             { "warn", "yellow" },
                                             { "error", "red" },
                                             { "fatal", "magenta" } } },
                                         { "enabled", true } } } };

    /***
     * @brief map of registered components
     * @details {type: format string}
     */
    std::map<std::string, std::string> registered_components_;

    /***
     * @brief load components from setting
     * @param setting_file setting file name
     */
    void loadSettingComponents(std::string_view setting_file);

    /***
     * @brief register components from json
     * @param json input json
     * @note each component must have `type` and `enabled` fields, `type` is the type of component, `enabled` is whether
     * the component is enabled, if `enabled` is false, the component will not be formatted
     * @note the format of component is defined in `json/log_event_format.json`
     */
    void registerComponents(const nlohmann::json& json);
};

/***
 * @brief formatter class to format log message
 * @note inspired by [sylar logger](https://github.com/sylar-yin/sylar)
 */
class Formatter {
public:
    using Ptr = std::shared_ptr<Formatter>;
    using ConstPtr = std::shared_ptr<const Formatter>;

    /***
     * @brief Formatter constructor
     * @param factory component factory
     */
    explicit Formatter(const ComponentFactory& factory);

    /***
     * @brief format message
     * @tparam Args type of universal parameter pack
     * @param fmt formatted message
     * @param args universal parameter pack
     * @note inspired by [fmtlib](https://github.com/fmtlib)
     */
    template<typename... Args>
    inline std::string format(const std::format_string<Args...>&& fmt, Args&&... args)
    {
        return std::format(
            std::forward<std::format_string<Args...>>(fmt),
            std::forward<Args>(args)...
        );
    }

    /***
     * @brief format log message into `std::string` within registered components
     * @param components registered components map
     * @return formatted log message
     * @details the format is able to be customized in `logger_settings.json`
     */
    std::string formatComponents(const std::map<std::string, std::string>& components);

private:
    /***
     * @brief log event
     */
    LogEvent::Ptr event_;

    /***
     * @brief component factory provides registered components
     */
    ComponentFactory::Ptr factory_;

    /***
     * @brief format color
     * @param format `aw_logger::Color` format
     * @return formatted color from color map
     * @note if color is not found, return `white` format
     */
    std::string formatColor(std::string_view format);

    /***
     * @brief format log message
     * @return formatted log message
     */
    std::string formatMsg()
    {
        return Formatter::format(event_->getMsg());
    }

    /***
     * @brief format log level
     * @return formatted log level
     */
    std::string formatLevel()
    {
        return Formatter::format("[" + event_->getLogLevelString() + "]");
    }

    /***
     * @brief format log timestamp
     * @param format timestamp format
     * @return formatted log timestamp
     */
    std::string formatTimestamp()
    {
        return "[" + Formatter::format("{}", event_->getTimestamp()) + "]";
    }

    /***
     * @brief format log source location
     * @param format source location format
     * @return formatted log source location
     */
    std::string formatSourceLocation(std::string_view format);

    /***
     * @brief format log thread id
     * @return formatted log thread id
     */
    std::string formatThreadId()
    {
        return "[tid: " + Formatter::format("{}", event_->getThreadId()) + "]";
    }
};

} // namespace aw_logger

#endif //! FORMATTER_HPP
