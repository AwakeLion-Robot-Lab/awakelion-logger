
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
#include <format>
#include <map>
#include <memory>
#include <string>
#include <string_view>

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
    using ConstPtr = std::shared_ptr<const ComponentFactory>;

    /***
     * @brief default constructor
     * @details the component is loaded via `aw_logger_settings.json`
     */
    explicit ComponentFactory();

    /***
     * @brief runtime pattern parsing constructor
     * @param pattern pattern divided by `%`
     */
    explicit ComponentFactory(std::string_view pattern);

    /***
     * @brief map of registered components
     * @details {type: unformatted data}
     */
    std::map<std::string, std::string> registered_components_;

private:
    /***
     * @brief log event format json from `aw_logger_settings.json`
     */
    nlohmann::json setting_json_;

    /***
     * @brief default log event format
     */
    const nlohmann::json default_json_ = { "log_event",
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
     * @brief parsed pattern
     */
    std::string parsed_pattern_;

    /***
     * @brief default pattern
     * @details
     * %T timestamp
     * %L log level
     * %t thread id
     * %l source location
     * %a tab
     * %s log message
     */
    const std::string default_pattern_ = "%T%L%t%l%a%s";

    /***
     * @brief load components from setting
     * @param file_name file name
     */
    void loadSettingComponents(std::string_view file_name);

    /***
     * @brief register components from json
     * @param json input json
     * @note each component must have `type` and `enabled` fields, `type` is the type of component, `enabled` is whether
     * the component is enabled, if `enabled` is false, the component will not be formatted
     * @note the format of component is defined in `config/aw_logger_settings.json`
     */
    void registerComponents(const nlohmann::json& json);

    /***
     * @brief parse patterns
     * @param pattern pattern
     */
    void parsePatterns(std::string_view pattern);
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
    explicit Formatter(const ComponentFactory::Ptr& factory);

    /***
     * @brief format message while compiling
     * @tparam Args type of universal parameter pack
     * @param fmt unformatted message
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
     * @brief format message while runtime
     * @tparam Args type of universal parameter pack
     * @param fmt unformatted message
     * @param args universal parameter pack
     */
    template<typename... Args>
    inline std::string vformat(std::string_view fmt, Args&&... args)
    {
        return std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
    }

    /***
     * @brief set component factory
     * @param factory component factory
     */
    void setFactory(const ComponentFactory::Ptr& factory)
    {
        factory_ = factory;
    }

    /***
     * @brief format log message into `std::string` within registered components
     * @param event log event
     * @param components registered components map
     * @return formatted log message
     * @details the format is able to be customized in `logger_settings.json`
     */
    std::string formatComponents(
        const LogEvent::Ptr& event,
        const std::map<std::string, std::string>& components
    );

    /***
     * @brief get registered components map
     * @return registered components map
     */
    auto getRegisteredComponents() -> const std::map<std::string, std::string>&
    {
        return factory_->registered_components_;
    }

private:
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
     * @param event log event
     * @return formatted log message
     */
    std::string formatMsg(const LogEvent::Ptr& event)
    {
        return event->getMsg();
    }

    /***
     * @brief format log level
     * @param event log event
     * @return formatted log level
     */
    std::string formatLevel(const LogEvent::Ptr& event)
    {
        return Formatter::vformat("[{}]", event->getLogLevelString());
    }

    /***
     * @brief format log timestamp
     * @param event log event
     * @return formatted log timestamp
     */
    std::string formatTimestamp(const LogEvent::Ptr& event)
    {
        return Formatter::vformat("[{}]", event->getTimestamp());
    }

    /***
     * @brief format log source location
     * @param format source location format
     * @return formatted log source location
     */
    std::string formatSourceLocation(const LogEvent::Ptr& event, std::string_view format);

    /***
     * @brief format log thread id
     * @return formatted log thread id
     */
    std::string formatThreadId(const LogEvent::Ptr& event)
    {
        return Formatter::vformat("[tid: {}]", event->getThreadId());
    }
};

} // namespace aw_logger

#endif //! FORMATTER_HPP
