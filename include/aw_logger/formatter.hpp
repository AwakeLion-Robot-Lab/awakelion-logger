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
#include <array>
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
 * @brief component factory to manage components via built-in defaults or runtime pattern
 */
class ComponentFactory {
public:
    /***
     * @brief pattern parsing state enum
     * @details
     * NORMAL_TEXT: normal text state, e.g. text insize `()` `[]` etc.
     * PATTERN_CHAR: pattern character state, e.g. char after `%`, like `%t`, `%p`
     */
    enum class patternState : size_t { NORMAL_TEXT, PATTERN_CHAR };

    using Ptr = std::unique_ptr<ComponentFactory>;
    using ConstPtr = std::unique_ptr<const ComponentFactory>;

    /***
     * @brief default constructor (uses built-in default components)
     */
    explicit ComponentFactory();

    /***
     * @brief runtime pattern parsing constructor
     * @param pattern pattern divided by `%`
     */
    explicit ComponentFactory(std::string_view pattern);

    /***
     * @brief ordered vector of registered components
     * @details preserve the order from JSON config file
     * @note vector of {type: unformatted data} pairs
     */
    std::vector<std::pair<std::string, std::string>> registered_components_;

private:
    /***
     * @brief default log event format
     */
    const nlohmann::json default_json_ = {
        { "components",
          { { { "type", "timestamp" }, { "enabled", true } },
            { { "type", "level" }, { "enabled", true } },
            { { "type", "tid" }, { "enabled", true } },
            { { "type", "loc" },
              { "format", "[{file_name}:{function_name}:{line}]" },
              { "enabled", true } },
            { { "type", "msg" }, { "enabled", true } } } }
    };

    /***
     * @brief register components from json
     * @param json input json
     * @note each component must have `type` and `enabled` fields, `type` is the type of component, `enabled` is whether
     * the component is enabled, if `enabled` is false, the component will not be formatted
     */
    void registerComponents(const nlohmann::json& json);

    /***
     * @brief parse runtime pattern
     * @param pattern pattern string
     */
    void parsePattern(std::string_view pattern);
};

/***
 * @brief formatter class to format log message
 * @note inspired by [sylar logger](https://github.com/sylar-yin/sylar)
 */
class Formatter {
public:
    using Ptr = std::unique_ptr<Formatter>;
    using ConstPtr = std::unique_ptr<const Formatter>;

    /***
     * @brief constructor
     * @param factory component factory
     */
    explicit Formatter(ComponentFactory::Ptr factory);

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
    inline std::string vformat(std::string_view fmt, const Args&... args)
    {
        return std::vformat(fmt, std::make_format_args(args...));
    }

    /***
     * @brief set component factory
     * @param factory component factory
     */
    void setFactory(ComponentFactory::Ptr factory)
    {
        factory_ = std::move(factory);
    }

    /***
     * @brief enable or disable ANSI color output
     * @param enable true to enable color output, false to disable color output
     */
    void enableColor(bool enable = true)
    {
        color_enabled_ = enable;
    }

    /***
     * @brief reset all level colors to the built-in defaults
     */
    void resetLevelColors();

    /***
     * @brief set color for a log level by color name
     * @param level log level
     * @param color_name color name
     */
    void setLevelColor(LogLevel::level level, std::string_view color_name);

    /***
     * @brief set debug color
     * @param color_name color name
     */
    void setDebugColor(std::string_view color_name)
    {
        setLevelColor(LogLevel::level::DEBUG, color_name);
    }

    /***
     * @brief set info color
     * @param color_name color name
     */
    void setInfoColor(std::string_view color_name)
    {
        setLevelColor(LogLevel::level::INFO, color_name);
    }

    /***
     * @brief set notice color
     * @param color_name color name
     */
    void setNoticeColor(std::string_view color_name)
    {
        setLevelColor(LogLevel::level::NOTICE, color_name);
    }

    /***
     * @brief set warn color
     * @param color_name color name
     */
    void setWarnColor(std::string_view color_name)
    {
        setLevelColor(LogLevel::level::WARN, color_name);
    }

    /***
     * @brief set error color
     * @param color_name color name
     */
    void setErrorColor(std::string_view color_name)
    {
        setLevelColor(LogLevel::level::ERROR, color_name);
    }

    /***
     * @brief set fatal color
     * @param color_name color name
     */
    void setFatalColor(std::string_view color_name)
    {
        setLevelColor(LogLevel::level::FATAL, color_name);
    }

    /***
     * @brief format log message within registered components and append into `out`
     * @param out output string
     * @param event log event
     * @param components registered components ordered vector
     * @details avoids per-call heap allocation when callers reuse a `thread_local` buffer.
     */
    void formatComponentsTo(
        std::string& out,
        const LogEvent::Ptr& event,
        const std::vector<std::pair<std::string, std::string>>& components
    );

    /***
     * @brief get registered components ordered vector
     * @return registered components ordered vector
     */
    auto getRegisteredComponents() -> const std::vector<std::pair<std::string, std::string>>&
    {
        return factory_->registered_components_;
    }

private:
    /***
     * @brief component factory provides registered components
     */
    ComponentFactory::Ptr factory_;

    /***
     * @brief flag to control color output
     */
    bool color_enabled_;

    /***
     * @brief pre-built color codes per log level
     */
    std::array<std::string, 7> level_color_codes_ {};

    /***
     * @brief format color
     * @param format `aw_logger::Color` format
     * @return formatted color from color map
     * @note if color is not found, return `white` format
     */
    std::string formatColor(std::string_view format);

    /***
     * @brief format source location into `out`
     * @param out output string
     * @param event log event
     * @param format source location format
     */
    void
    formatSourceLocation(std::string& out, const LogEvent::Ptr& event, std::string_view format);

    /***
     * @brief convert log level to index
     * @param level log level
     * @return index for log level
     * @note retval is constexpr
     */
    static constexpr size_t levelToIndex(LogLevel::level level) noexcept
    {
        return static_cast<size_t>(level);
    }

    /***
     * @brief fetch color code for a given level
     * @param level log level
     * @return color code string for the log level
     */
    const std::string& colorForLevel(LogLevel::level level) const noexcept
    {
        static const std::string empty;
        const auto idx = levelToIndex(level);
        if (!color_enabled_ || idx >= level_color_codes_.size())
            return empty;

        return level_color_codes_[idx];
    }
};

} // namespace aw_logger

#endif //! FORMATTER_HPP
