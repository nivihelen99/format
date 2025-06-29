#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <tuple>
#include <stdexcept>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <utility>
#include <iomanip>
#include <sstream>
#include <limits>
#include <type_traits>
#include <charconv>

// C++23 standard library detection
#if __has_include(<format>) && __has_include(<print>) && defined(__cpp_lib_format) && __cpp_lib_format >= 202106L && defined(__cpp_lib_print) && __cpp_lib_print >= 202207L
#define COMPAT_USE_STD_FORMAT 1
#include <format>
#include <print>
#endif

namespace compat {

#ifdef COMPAT_USE_STD_FORMAT
    // If C++23 <format> and <print> are available, use them directly.
    using std::format;
    
    template <typename OutputIt, typename... Args>
    OutputIt format_to(OutputIt out, std::string_view fmt, Args&&... args) {
        std::string formatted_str = std::format(fmt, std::forward<Args>(args)...);
        return std::copy(formatted_str.begin(), formatted_str.end(), out);
    }

    using std::print;
    using std::println;

#else
    // C++17 optimized implementation

    namespace internal {
        // Structure to hold parsed format specifier details
        struct ParsedFormatSpec {
            std::string_view arg_id_str;
            char fill = ' ';
            char align = '\0';
            int width = -1;
            int precision = -1;
            char type = '\0';

            constexpr bool has_fill_align() const noexcept { return align != '\0'; }
            constexpr bool has_width() const noexcept { return width != -1; }
            constexpr bool has_precision() const noexcept { return precision != -1; }
        };

        // Fast integer parsing using string_view
        inline std::pair<int, std::size_t> parse_int_fast(std::string_view str, std::size_t start = 0) {
            if (start >= str.size() || !std::isdigit(str[start])) {
                return {-1, start};
            }
            
            int result = 0;
            std::size_t pos = start;
            
            while (pos < str.size() && std::isdigit(str[pos])) {
                int digit = str[pos] - '0';
                if (result > (std::numeric_limits<int>::max() - digit) / 10) {
                    throw std::runtime_error("Format specifier width/precision out of range");
                }
                result = result * 10 + digit;
                ++pos;
            }
            
            return {result, pos};
        }

        // Optimized placeholder content parser using string_view
        ParsedFormatSpec parse_placeholder_content(std::string_view content) {
            ParsedFormatSpec spec;
            std::size_t pos = 0;
            const std::size_t len = content.size();

            // 1. Parse argument ID (before ':')
            std::size_t colon_pos = content.find(':');
            if (colon_pos == 0) {
                pos = 1;
            } else if (colon_pos != std::string_view::npos) {
                spec.arg_id_str = content.substr(0, colon_pos);
                pos = colon_pos + 1;
            } else {
                spec.arg_id_str = content;
                return spec;
            }

            if (pos >= len) {
                return spec;
            }

            // 2. Parse fill and align
            if (pos + 1 < len) {
                char next_char = content[pos + 1];
                if (next_char == '<' || next_char == '^' || next_char == '>') {
                    spec.fill = content[pos];
                    spec.align = next_char;
                    pos += 2;
                } else if (content[pos] == '<' || content[pos] == '^' || content[pos] == '>') {
                    spec.align = content[pos];
                    pos += 1;
                }
            } else if (pos < len && (content[pos] == '<' || content[pos] == '^' || content[pos] == '>')) {
                spec.align = content[pos];
                pos += 1;
            }

            // Handle '0' prefix for zero-padding
            if (spec.fill == ' ' && spec.align == '\0' && pos < len && content[pos] == '0' &&
                pos + 1 < len && std::isdigit(content[pos + 1])) {
                spec.fill = '0';
            }

            // 3. Parse width
            auto [width, new_pos] = parse_int_fast(content, pos);
            if (width != -1) {
                spec.width = width;
                pos = new_pos;
            }

            // 4. Parse precision
            if (pos < len && content[pos] == '.') {
                ++pos;
                auto [precision, precision_end] = parse_int_fast(content, pos);
                if (precision == -1) {
                    throw std::runtime_error("Format specifier missing precision digits after '.'");
                }
                spec.precision = precision;
                pos = precision_end;
            }

            // 5. Parse type
            if (pos < len) {
                spec.type = content[pos];
                ++pos;
            }

            if (pos < len) {
                throw std::runtime_error("Invalid characters at end of format specifier");
            }
            
            return spec;
        }

        // Optimized padding with pre-allocated string
        std::string apply_padding(std::string_view value, const ParsedFormatSpec& spec) {
            const int value_len = static_cast<int>(value.size());
            
            if (!spec.has_width() || value_len >= spec.width) {
                return std::string(value);
            }

            const int padding_needed = spec.width - value_len;
            std::string result;
            result.reserve(spec.width);

            // Determine alignment
            char align_char = spec.align;
            if (align_char == '\0') {
                // Check if it's a numeric value for default alignment
                bool is_numeric = !value.empty() && 
                    (std::isdigit(value[0]) || 
                     ((value[0] == '-' || value[0] == '+') && value.size() > 1 && std::isdigit(value[1])));
                
                if (spec.fill == '0' && is_numeric) {
                    align_char = '>';
                } else {
                    align_char = is_numeric ? '>' : '<';
                }
            }

            // Apply alignment efficiently
            switch (align_char) {
                case '<': // Left align
                    result.append(value);
                    result.append(padding_needed, spec.fill);
                    break;
                case '>': // Right align
                    result.append(padding_needed, spec.fill);
                    result.append(value);
                    break;
                case '^': { // Center align
                    const int pad_left = padding_needed / 2;
                    const int pad_right = padding_needed - pad_left;
                    result.append(pad_left, spec.fill);
                    result.append(value);
                    result.append(pad_right, spec.fill);
                    break;
                }
                default:
                    result.append(value);
                    break;
            }
            
            return result;
        }

        // Base formatter template with SFINAE-friendly design
        template <typename T, typename = void>
        struct formatter {
            static_assert(sizeof(T) == 0, "No formatter found for this type");
        };

        // Optimized numeric formatting using std::to_chars when available
        template <typename T>
        std::string format_integer(T value, const ParsedFormatSpec& spec) {
            if (spec.type != '\0' && spec.type != 'd') {
                throw std::runtime_error("Invalid type specifier for integral argument");
            }

            std::string str_val;
            
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
            // Use std::to_chars for better performance
            std::array<char, 32> buffer{};
            auto [ptr, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            if (ec == std::errc{}) {
                str_val.assign(buffer.data(), ptr);
            } else {
                // Fallback to stringstream
                std::ostringstream oss;
                oss << value;
                str_val = oss.str();
            }
#else
            std::ostringstream oss;
            oss << value;
            str_val = oss.str();
#endif
            
            return apply_padding(str_val, spec);
        }

        template <typename T>
        std::string format_floating_point(T value, const ParsedFormatSpec& spec) {
            if (spec.type != '\0' && spec.type != 'f' && spec.type != 'F') {
                throw std::runtime_error("Invalid type specifier for floating-point argument");
            }

            std::ostringstream oss;
            
            if (spec.type == 'f' || spec.type == 'F') {
                oss << std::fixed;
                oss << std::setprecision(spec.has_precision() ? spec.precision : 6);
            } else if (spec.has_precision()) {
                oss << std::setprecision(spec.precision);
            }
            
            oss << value;
            return apply_padding(oss.str(), spec);
        }

        // Specialized formatters using SFINAE
        template <typename T>
        struct formatter<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>>> {
            static std::string format(T value, const ParsedFormatSpec& spec) {
                return format_integer(value, spec);
            }
        };

        template <typename T>
        struct formatter<T, std::enable_if_t<std::is_floating_point_v<T>>> {
            static std::string format(T value, const ParsedFormatSpec& spec) {
                return format_floating_point(value, spec);
            }
        };

        template <>
        struct formatter<bool> {
            static std::string format(bool value, const ParsedFormatSpec& spec) {
                if (spec.type != '\0' && spec.type != 'b' && spec.type != 's') {
                    throw std::runtime_error("Invalid type specifier for bool argument");
                }
                return apply_padding(value ? "true" : "false", spec);
            }
        };

        template <>
        struct formatter<char> {
            static std::string format(char value, const ParsedFormatSpec& spec) {
                return apply_padding(std::string(1, value), spec);
            }
        };

        // String formatters
        template <>
        struct formatter<std::string> {
            static std::string format(const std::string& value, const ParsedFormatSpec& spec) {
                return apply_padding(value, spec);
            }
        };

        template <>
        struct formatter<std::string_view> {
            static std::string format(std::string_view value, const ParsedFormatSpec& spec) {
                return apply_padding(value, spec);
            }
        };

        template <>
        struct formatter<const char*> {
            static std::string format(const char* value, const ParsedFormatSpec& spec) {
                return apply_padding(value ? std::string_view(value) : std::string_view("(null)"), spec);
            }
        };

        template <>
        struct formatter<char*> {
            static std::string format(char* value, const ParsedFormatSpec& spec) {
                return apply_padding(value ? std::string_view(value) : std::string_view("(null)"), spec);
            }
        };

        // Array formatters
        template <std::size_t N>
        struct formatter<char[N]> {
            static std::string format(const char(&value)[N], const ParsedFormatSpec& spec) {
                return apply_padding(std::string_view(value, strnlen(value, N)), spec);
            }
        };

        template <std::size_t N>
        struct formatter<const char[N]> {
            static std::string format(const char(&value)[N], const ParsedFormatSpec& spec) {
                return apply_padding(std::string_view(value, strnlen(value, N)), spec);
            }
        };

        // Optimized argument formatting dispatch
        template <std::size_t I, typename Tuple, typename OutputIt>
        void format_nth_arg_to(OutputIt& out, const Tuple& tpl, const ParsedFormatSpec& spec) {
            using OriginalType = std::tuple_element_t<I, Tuple>;
            using DecayedType = std::decay_t<OriginalType>;
            
            std::string formatted_val = formatter<DecayedType>::format(std::get<I>(tpl), spec);
            out = std::copy(formatted_val.begin(), formatted_val.end(), out);
        }

        template <typename OutputIt, typename Tuple, std::size_t... Is>
        bool dispatch_format_arg(OutputIt& out, const Tuple& tuple, const ParsedFormatSpec& spec, 
                                std::size_t target_idx, std::index_sequence<Is...>) {
            return ((Is == target_idx ? (format_nth_arg_to<Is>(out, tuple, spec), true) : false) || ...);
        }

    } // namespace internal

    enum class IndexingMode : uint8_t {
        Unknown,
        Automatic,
        Manual
    };

    template <typename OutputIt, typename... Args>
    OutputIt format_to(OutputIt out, std::string_view fmt, Args&&... args) {
        if constexpr (sizeof...(Args) == 0) {
            // No arguments case - just copy format string, checking for invalid placeholders
            for (std::size_t i = 0; i < fmt.size(); ++i) {
                if (fmt[i] == '{') {
                    if (i + 1 < fmt.size() && fmt[i + 1] == '{') {
                        *out++ = '{';
                        ++i;
                    } else {
                        throw std::runtime_error("Format string contains placeholders but no arguments provided");
                    }
                } else if (fmt[i] == '}') {
                    if (i + 1 < fmt.size() && fmt[i + 1] == '}') {
                        *out++ = '}';
                        ++i;
                    } else {
                        throw std::runtime_error("Unmatched '}' in format string");
                    }
                } else {
                    *out++ = fmt[i];
                }
            }
            return out;
        } else {
            auto arg_tuple = std::make_tuple(std::forward<Args>(args)...);
            std::size_t current_auto_arg_index = 0;
            constexpr std::size_t num_args = sizeof...(Args);
            IndexingMode indexing_mode = IndexingMode::Unknown;

            for (std::size_t i = 0; i < fmt.size(); ++i) {
                if (fmt[i] == '{') {
                    if (i + 1 < fmt.size() && fmt[i + 1] == '{') {
                        *out++ = '{';
                        ++i;
                        continue;
                    }
                    
                    const std::size_t placeholder_end = fmt.find('}', i + 1);
                    if (placeholder_end == std::string_view::npos) {
                        throw std::runtime_error("Unmatched '{' in format string");
                    }
                    
                    const std::string_view placeholder_content = fmt.substr(i + 1, placeholder_end - (i + 1));
                    internal::ParsedFormatSpec parsed_spec;
                    
                    try {
                        parsed_spec = internal::parse_placeholder_content(placeholder_content);
                    } catch (const std::runtime_error& e) {
                        throw std::runtime_error("Error parsing placeholder: " + std::string(e.what()));
                    }
                    
                    std::size_t arg_to_format_idx;

                    if (parsed_spec.arg_id_str.empty()) {
                        if (indexing_mode == IndexingMode::Manual) {
                            throw std::runtime_error("Cannot mix automatic and manual argument indexing");
                        }
                        indexing_mode = IndexingMode::Automatic;
                        arg_to_format_idx = current_auto_arg_index++;
                    } else {
                        if (indexing_mode == IndexingMode::Automatic) {
                            throw std::runtime_error("Cannot mix automatic and manual argument indexing");
                        }
                        indexing_mode = IndexingMode::Manual;
                        
                        // Parse argument index
                        int parsed_idx;
                        auto [ptr, ec] = std::from_chars(parsed_spec.arg_id_str.data(), 
                                                       parsed_spec.arg_id_str.data() + parsed_spec.arg_id_str.size(), 
                                                       parsed_idx);
                        if (ec != std::errc{} || ptr != parsed_spec.arg_id_str.data() + parsed_spec.arg_id_str.size() || parsed_idx < 0) {
                            throw std::runtime_error("Invalid argument index: " + std::string(parsed_spec.arg_id_str));
                        }
                        arg_to_format_idx = static_cast<std::size_t>(parsed_idx);
                    }

                    if (arg_to_format_idx >= num_args) {
                        throw std::runtime_error("Argument index " + std::to_string(arg_to_format_idx) + 
                                                " out of bounds for " + std::to_string(num_args) + " arguments");
                    }
                    
                    bool processed = internal::dispatch_format_arg(out, arg_tuple, parsed_spec, arg_to_format_idx,
                                                                 std::make_index_sequence<num_args>{});
                    
                    if (!processed) {
                        throw std::runtime_error("Internal error: argument dispatch failed");
                    }
                    
                    i = placeholder_end;
                    
                } else if (fmt[i] == '}') {
                    if (i + 1 < fmt.size() && fmt[i + 1] == '}') {
                        *out++ = '}';
                        ++i;
                    } else {
                        throw std::runtime_error("Unmatched '}' in format string");
                    }
                } else {
                    *out++ = fmt[i];
                }
            }
            
            return out;
        }
    }

    template <typename... Args>
    std::string format(std::string_view fmt, Args&&... args) {
        std::string result;
        result.reserve(fmt.size() + sizeof...(Args) * 16); // Reasonable initial capacity
        format_to(std::back_inserter(result), fmt, std::forward<Args>(args)...);
        return result;
    }

    template <typename... Args>
    void print(std::string_view fmt, Args&&... args) {
        format_to(std::ostream_iterator<char>(std::cout), fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void println(std::string_view fmt, Args&&... args) {
        format_to(std::ostream_iterator<char>(std::cout), fmt, std::forward<Args>(args)...);
        std::cout << '\n';
    }

#endif // COMPAT_USE_STD_FORMAT

} // namespace compat
