#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <stdexcept> // For std::runtime_error
#include <iostream>  // For std::cout, std::ostream_iterator
#include <iterator>  // For std::back_inserter, std::ostream_iterator
#include <algorithm> // For std::copy, std::fill_n
#include <utility>   // For std::forward, std::move
#include <iomanip>   // For std::setprecision, std::fixed
#include <sstream>   // For std::stringstream in formatters
#include <limits>    // For std::numeric_limits

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
    // We need to define our own format_to that matches the signature,
    // as std::format_to has a slightly different template signature (charT vs const std::string&)
    // and also to ensure it's available if only <format> is present but not <print> fully.
    template <typename OutputIt, typename... Args>
    OutputIt format_to(OutputIt out, const std::string& fmt, Args&&... args) {
        std::string formatted_str = std::format(fmt, std::forward<Args>(args)...);
        return std::copy(formatted_str.begin(), formatted_str.end(), out);
    }

    using std::print;
    using std::println;

#else
    // C++17 custom implementation will go here

    namespace internal {

        // Structure to hold parsed format specifier details
        struct ParsedFormatSpec {
            std::string arg_id_str;    // The part before ':', can be empty or a number
            char fill = ' ';
            char align = '\0'; // '\0' (none), '<' (left), '>' (right), '^' (center)
            int width = -1;
            int precision = -1;
            char type = '\0';  // f, d, s, etc. (currently used for 'f')

            bool has_fill_align() const { return align != '\0'; }
            bool has_width() const { return width != -1; }
            bool has_precision() const { return precision != -1; }
        };

        // Parses the content of {} like "0:*>10.2f"
        // Returns ParsedFormatSpec. Throws on error.
        ParsedFormatSpec parse_placeholder_content(const std::string& content) {
            ParsedFormatSpec spec;
            std::size_t current_pos = 0;
            std::size_t content_len = content.length();

            // 1. Argument ID (before ':')
            std::size_t colon_pos = content.find(':');
            if (colon_pos == 0) { // Starts with ':', so no arg id, implicit indexing
                current_pos = 1; // Start parsing spec after ':'
            } else if (colon_pos != std::string::npos) { // Has ':' and something before it
                spec.arg_id_str = content.substr(0, colon_pos);
                current_pos = colon_pos + 1;
            } else { // No ':', so the whole content is arg_id or empty
                spec.arg_id_str = content;
                return spec; // No further specifiers
            }

            // If there's nothing after ':', it's an error or just an empty spec
            if (current_pos >= content_len) {
                if (colon_pos != std::string::npos) {
                    // Allow empty spec after colon, e.g. "{:}"
                     return spec;
                }
                 // This case should ideally be caught by previous logic returning early if no colon
                throw std::runtime_error("Invalid format specifier: empty after colon");
            }

            // 2. Fill and Align (optional)
            // Check if the character *after* current_pos is one of <, ^, >
            // If so, the character at current_pos is the fill char.
            if (current_pos + 1 < content_len) {
                char next_char = content[current_pos + 1];
                if (next_char == '<' || next_char == '^' || next_char == '>') {
                    spec.fill = content[current_pos];
                    spec.align = next_char;
                    current_pos += 2;
                } else if (content[current_pos] == '<' || content[current_pos] == '^' || content[current_pos] == '>') {
                    spec.align = content[current_pos]; // Default fill is space
                    current_pos += 1;
                }
            } else if (current_pos < content_len) { // Only one char left, could be alignment
                 if (content[current_pos] == '<' || content[current_pos] == '^' || content[current_pos] == '>') {
                    spec.align = content[current_pos]; // Default fill is space
                    current_pos += 1;
                }
            }

            // Check for '0' prefix which implies zero-padding if no explicit fill/align is set.
            if (spec.fill == ' ' && spec.align == '\0' &&      // No explicit fill or alignment yet
                current_pos < content_len && content[current_pos] == '0' && // Current character is '0'
                (current_pos + 1 < content_len && std::isdigit(content[current_pos + 1]))) { // And it's followed by another digit (part of width)
                spec.fill = '0';
                // The '0' is not consumed here; it will be part of the width parsed next.
            }

            // 3. Width (optional)
            if (current_pos < content_len && std::isdigit(content[current_pos])) {
                std::size_t width_end = current_pos;
                while (width_end < content_len && std::isdigit(content[width_end])) {
                    width_end++;
                }
                try {
                    spec.width = std::stoi(content.substr(current_pos, width_end - current_pos));
                } catch (const std::out_of_range&) {
                    throw std::runtime_error("Format specifier width out of range");
                }
                current_pos = width_end;
            }

            // 4. Precision (optional, starts with '.')
            if (current_pos < content_len && content[current_pos] == '.') {
                current_pos++; // Skip '.'
                if (current_pos < content_len && std::isdigit(content[current_pos])) {
                    std::size_t precision_end = current_pos;
                    while (precision_end < content_len && std::isdigit(content[precision_end])) {
                        precision_end++;
                    }
                    try {
                        spec.precision = std::stoi(content.substr(current_pos, precision_end - current_pos));
                    } catch (const std::out_of_range&) {
                        throw std::runtime_error("Format specifier precision out of range");
                    }
                    current_pos = precision_end;
                } else {
                    throw std::runtime_error("Format specifier missing precision digits after '.'");
                }
            }

            // 5. Type (optional, e.g., 'f' for float, 'd' for int, 's' for string)
            if (current_pos < content_len) {
                spec.type = content[current_pos];
                current_pos++;
            }

            if (current_pos < content_len) {
                throw std::runtime_error("Invalid characters at end of format specifier: " + content.substr(current_pos));
            }
            return spec;
        }

        // Apply padding based on ParsedFormatSpec
        std::string apply_padding(const std::string& value, const ParsedFormatSpec& spec) {
            if (!spec.has_width() || static_cast<int>(value.length()) >= spec.width) {
                return value;
            }

            int padding_needed = spec.width - static_cast<int>(value.length());
            if (padding_needed <= 0) return value;

            std::string result;
            result.reserve(spec.width);

            char align_char = spec.align;
            bool is_numeric_type_for_default_align = false;
            if (!value.empty() && (std::isdigit(value[0]) || ((value[0] == '-' || value[0] == '+') && value.length() > 1 && std::isdigit(value[1])))) {
                is_numeric_type_for_default_align = true;
            }

            if (align_char == '\0') { // No alignment specified
                // If fill is '0' (zero-padding flag was set), default to right align for numbers.
                // Otherwise, left for strings, right for numbers.
                if (spec.fill == '0' && is_numeric_type_for_default_align) {
                    align_char = '>';
                } else {
                    align_char = is_numeric_type_for_default_align ? '>' : '<';
                }
            }

            if (align_char == '<') { // Left align
                result += value;
                for(int i=0; i<padding_needed; ++i) result += spec.fill;
            } else if (align_char == '>') { // Right align
                for(int i=0; i<padding_needed; ++i) result += spec.fill;
                result += value;
            } else if (align_char == '^') { // Center align
                int pad_left = padding_needed / 2;
                int pad_right = padding_needed - pad_left;
                for(int i=0; i<pad_left; ++i) result += spec.fill;
                result += value;
                for(int i=0; i<pad_right; ++i) result += spec.fill;
            } else {
                result += value;
            }
            return result;
        }

        // Base formatter template
        template <typename T, typename CharT = char>
        struct formatter {
            static_assert(sizeof(T) == 0, "No formatter found for this type. Please specialize compat::internal::formatter.");
        };

        // Generic formatter for types convertible by std::stringstream
        template <typename T>
        std::string format_basic_type(const T& value, ParsedFormatSpec spec) {
            std::stringstream ss;
            bool is_float = std::is_floating_point<T>::value;

            if (is_float) {
                if (spec.type == 'f' || spec.type == 'F') {
                    ss << std::fixed;
                    if (spec.has_precision()) {
                        ss << std::setprecision(spec.precision);
                    } else {
                        ss << std::setprecision(6); // Default precision for 'f'
                    }
                } else if (spec.has_precision()) {
                    ss << std::setprecision(spec.precision);
                } else if (spec.type == '\0' && !spec.has_fill_align() && !spec.has_width()) {
                     if (value == static_cast<long long>(value)) {
                         ss << std::fixed << std::setprecision(6);
                     } else {
                        // Default std::stringstream precision is usually 6.
                     }
                }
            }

            if (spec.type != '\0') {
                if (is_float) {
                    if (spec.type != 'f' && spec.type != 'F') {
                        throw std::runtime_error("Invalid type specifier '" + std::string(1, spec.type) + "' for floating-point argument");
                    }
                } else if (std::is_integral<T>::value) {
                    if (spec.type != 'd') {
                         throw std::runtime_error("Invalid type specifier '" + std::string(1, spec.type) + "' for integral argument");
                    }
                } else if (!std::is_same<T,bool>::value) { // Bool formatter handles its own 'b' or default
                     throw std::runtime_error("Type specifier '" + std::string(1, spec.type) + "' not allowed for this argument type");
                }
            }

            ss << value;
            std::string str_val = ss.str();

            if (is_float && (spec.type == 'f' || spec.type == 'F') && !spec.has_precision()) {
                 if (str_val.find('.') == std::string::npos) {
                     str_val += ".000000";
                 }
            }
            return apply_padding(str_val, spec);
        }

        template<> struct formatter<const char*> {
            static std::string format(const char* value, const ParsedFormatSpec& spec) {
                return apply_padding(std::string(value), spec);
            }
        };
        template<> struct formatter<char*> {
            static std::string format(char* value, const ParsedFormatSpec& spec) {
                return apply_padding(std::string(value), spec);
            }
        };
        template<> struct formatter<std::string> {
            static std::string format(const std::string& value, const ParsedFormatSpec& spec) {
                return apply_padding(value, spec);
            }
        };
        template<std::size_t N> struct formatter<char[N]> {
            static std::string format(const char(&value)[N], const ParsedFormatSpec& spec) {
                return apply_padding(std::string(value), spec);
            }
        };
        template<std::size_t N> struct formatter<const char[N]> {
            static std::string format(const char(&value)[N], const ParsedFormatSpec& spec) {
                return apply_padding(std::string(value), spec);
            }
        };

        #define DEFINE_FORMATTER_STREAMABLE(TYPE) \
            template<> struct formatter<TYPE> { \
                static std::string format(TYPE value, const ParsedFormatSpec& spec) { \
                    return format_basic_type(value, spec); \
                } \
            }

        DEFINE_FORMATTER_STREAMABLE(int);
        DEFINE_FORMATTER_STREAMABLE(unsigned int);
        DEFINE_FORMATTER_STREAMABLE(long);
        DEFINE_FORMATTER_STREAMABLE(unsigned long);
        DEFINE_FORMATTER_STREAMABLE(long long);
        DEFINE_FORMATTER_STREAMABLE(unsigned long long);
        DEFINE_FORMATTER_STREAMABLE(float);
        DEFINE_FORMATTER_STREAMABLE(double);
        DEFINE_FORMATTER_STREAMABLE(long double);

        template<> struct formatter<bool> {
            static std::string format(bool value, const ParsedFormatSpec& spec) {
                 if (spec.type != '\0' && spec.type != 'b' && spec.type != 's'){ // s for string-like, b for bool
                    throw std::runtime_error("Invalid type specifier for bool argument");
                }
                return apply_padding(value ? "true" : "false", spec);
            }
        };

        template <std::size_t I, typename Tuple, typename OutputIt>
        void format_nth_arg_to(OutputIt& out, const Tuple& tpl, const ParsedFormatSpec& spec) {
            using OriginalType = typename std::tuple_element<I, Tuple>::type;
            using DecayedType = typename std::decay<OriginalType>::type;
            
            std::string formatted_val = formatter<DecayedType>::format(std::get<I>(tpl), spec);
            for (char ch : formatted_val) {
                *out++ = ch;
            }
        }

        template <typename OutputIt, typename Tuple>
        struct DispatchHelper {
            OutputIt& out_ref;
            const Tuple& tuple_ref;
            const ParsedFormatSpec& spec_ref;
            std::size_t target_idx_val;
            bool& processed_flag_ref;

            DispatchHelper(OutputIt& o, const Tuple& t, const ParsedFormatSpec& s, std::size_t tidx, bool& pf)
                : out_ref(o), tuple_ref(t), spec_ref(s), target_idx_val(tidx), processed_flag_ref(pf) {}

            template <std::size_t... Is>
            void do_dispatch(std::index_sequence<Is...>) {
                (void)std::initializer_list<int>{
                    ((Is == target_idx_val ? (internal::format_nth_arg_to<Is>(out_ref, tuple_ref, spec_ref), processed_flag_ref = true) : false), 0)...
                };
            }
        };

    } // namespace internal

    enum class IndexingMode {
        Unknown,
        Automatic,
        Manual
    };

    template <typename OutputIt, typename... Args>
    OutputIt format_to(OutputIt out, const std::string& fmt, Args&&... args) {
        auto arg_tuple = std::make_tuple(std::forward<Args>(args)...);
        std::size_t current_auto_arg_index = 0;
        const std::size_t num_args = sizeof...(Args);
        IndexingMode indexing_mode = IndexingMode::Unknown;

        for (std::size_t i = 0; i < fmt.length(); ++i) {
            if (fmt[i] == '{') {
                if (i + 1 < fmt.length() && fmt[i+1] == '{') {
                    *out++ = '{';
                    i++;
                } else {
                    std::size_t placeholder_end = fmt.find('}', i + 1);
                    if (placeholder_end == std::string::npos) {
                        throw std::runtime_error("Unmatched '{' in format string");
                    }
                    
                    std::string placeholder_full_content = fmt.substr(i + 1, placeholder_end - (i + 1));
                    internal::ParsedFormatSpec parsed_spec;
                    try {
                        parsed_spec = internal::parse_placeholder_content(placeholder_full_content);
                    } catch (const std::runtime_error& e) {
                        throw std::runtime_error(std::string("Error parsing placeholder content '") + placeholder_full_content + "': " + e.what());
                    }
                    
                    std::size_t arg_to_format_idx;

                    if (parsed_spec.arg_id_str.empty()) {
                        if (indexing_mode == IndexingMode::Manual) {
                            throw std::runtime_error("Cannot switch from manual (e.g. {0}) to automatic (e.g. {}) argument indexing");
                        }
                        indexing_mode = IndexingMode::Automatic;
                        arg_to_format_idx = current_auto_arg_index;
                        current_auto_arg_index++;
                    } else {
                        if (indexing_mode == IndexingMode::Automatic) {
                            throw std::runtime_error("Cannot switch from automatic (e.g. {}) to manual (e.g. {0}) argument indexing");
                        }
                        indexing_mode = IndexingMode::Manual;
                        try {
                            for(char ch_id : parsed_spec.arg_id_str) {
                                if (!std::isdigit(ch_id)) {
                                    throw std::invalid_argument("Argument ID is not a number");
                                }
                            }
                            arg_to_format_idx = std::stoul(parsed_spec.arg_id_str);
                        } catch (const std::invalid_argument& ) {
                            throw std::runtime_error("Invalid format placeholder: non-numeric argument index '" + parsed_spec.arg_id_str + "'");
                        } catch (const std::out_of_range& ) {
                            throw std::runtime_error("Invalid format placeholder: argument index '" + parsed_spec.arg_id_str + "' out of range for stoul");
                        }
                    }

                    if (arg_to_format_idx >= num_args) {
                         if (num_args == 0) {
                            throw std::runtime_error("Argument index " + std::to_string(arg_to_format_idx) + " out of bounds (no arguments provided).");
                        }
                        throw std::runtime_error("Argument index " + std::to_string(arg_to_format_idx) + " out of bounds for " + std::to_string(num_args) + " arguments.");
                    }
                    
                    bool processed_dispatch = false;
                    internal::DispatchHelper<OutputIt, decltype(arg_tuple)> dispatcher(out, arg_tuple, parsed_spec, arg_to_format_idx, processed_dispatch);
                    dispatcher.do_dispatch(std::make_index_sequence<sizeof...(Args)>{});

                    if (!processed_dispatch && num_args > 0 && arg_to_format_idx < num_args) {
                        throw std::runtime_error("Internal error: argument dispatch failed for index " + std::to_string(arg_to_format_idx));
                    }
                    i = placeholder_end; 
                }
            } else if (fmt[i] == '}') {
                if (i + 1 < fmt.length() && fmt[i+1] == '}') {
                    *out++ = '}';
                    i++; 
                } else {
                    throw std::runtime_error("Unmatched '}' in format string");
                }
            } else {
                *out++ = fmt[i];
            }
        }
        return out;
    }

    template <typename... Args>
    std::string format(const std::string& fmt, Args&&... args) {
        std::string s;
        format_to(std::back_inserter(s), fmt, std::forward<Args>(args)...);
        return s;
    }

    template <typename... Args>
    void print(const std::string& fmt, Args&&... args) {
        format_to(std::ostream_iterator<char>(std::cout), fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void println(const std::string& fmt, Args&&... args) {
        format_to(std::ostream_iterator<char>(std::cout), fmt, std::forward<Args>(args)...);
        std::cout << '\n';
    }

#endif // COMPAT_USE_STD_FORMAT

} // namespace compat
