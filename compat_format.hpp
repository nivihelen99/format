#pragma once

#include <string>
#include <array>
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
#include <cstdint>   // For std::uint8_t
#include <cmath>     // For std::isinf, std::isnan

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
            bool hash_flag = false; // For alternate form (e.g. 0x, 0b, 0)
            int width = -1;
            int precision = -1;
            char type = '\0';  // f, d, s, b, o, x, X etc.

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
            // This check is moved after hash_flag parsing.
            // if (spec.fill == ' ' && spec.align == '\0' &&      // No explicit fill or alignment yet
            //     current_pos < content_len && content[current_pos] == '0' && // Current character is '0'
            //     (current_pos + 1 < content_len && std::isdigit(content[current_pos + 1]))) { // And it's followed by another digit (part of width)
            //     spec.fill = '0';
            //     // The '0' is not consumed here; it will be part of the width parsed next.
            // }

            // 3. Hash flag '#' (optional, for alternate form like 0x, 0b)
            if (current_pos < content_len && content[current_pos] == '#') {
                spec.hash_flag = true;
                current_pos++;
            }

            // 3.5. Zero padding option '0' (distinct from fill character '0')
            // This '0' acts as a flag if it appears before width digits and after any fill/align/hash.
            // It sets the fill character to '0' if no other fill character has been specified.
            if (current_pos < content_len && content[current_pos] == '0') {
                if (spec.fill == ' ') { // No fill character specified yet by fill/align
                    // Check if this '0' is followed by a digit (part of width) or is standalone before type.
                    // If it's like {:0f} or {:010f}, it implies zero padding.
                    // If it was like {:0<10f}, spec.fill would already be '0' from fill/align.
                    spec.fill = '0';
                }
                // This '0' will be consumed by width parsing if it's part of the width.
                // If format is just "{:0}", it's not width, but this logic is for specifiers after ':'.
                // If format is "{:0f}", width is not parsed, but fill is '0'. This is fine.
            }

            // 4. Width (optional)
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

            // 5. Precision (optional, starts with '.')
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

            // 6. Type (optional, e.g., 'f' for float, 'd' for int, 's' for string)
            if (current_pos < content_len) {
                spec.type = content[current_pos];
                current_pos++;
            }

            if (current_pos < content_len) {
                throw std::runtime_error("Invalid characters at end of format specifier: " + content.substr(current_pos));
            }
            return spec;
        }

        // Special function to handle padding with prefix for zero-padding cases
        // and general padding.
        std::string apply_padding_internal(std::string value_str, std::string prefix_str, const ParsedFormatSpec& spec) {
            // Handle sign for numeric types if present in value_str
            std::string sign_str;
            // Only extract sign if it's a numeric type specifier or default for number-like things
            bool could_be_numeric = (spec.type == 'd' ||
                                     spec.type == 'b' || spec.type == 'B' || // Added 'B'
                                     spec.type == 'o' ||
                                     spec.type == 'x' || spec.type == 'X' ||
                                     spec.type == 'f' || spec.type == 'F' ||
                                     spec.type == '\0'); // \0 could be int/float

            if (could_be_numeric && !value_str.empty() && (value_str[0] == '-' || value_str[0] == '+')) {
                 // Further check: is the rest of it a number? Or is it something like "-text"?
                 // This is tricky. For now, assume if type is numeric, sign is numeric.
                 // A more robust check might involve trying to parse value_str.substr(1) as a number.
                 // However, for formatting, this heuristic is often sufficient.
                bool actually_numeric_payload = true;
                if (value_str.length() > 1) {
                    char first_char_after_sign = value_str[1];
                    if (!std::isdigit(first_char_after_sign) && !( (spec.type == 'x' || spec.type == 'X') && std::isxdigit(first_char_after_sign))) {
                         // If not a digit (or hex char for hex types), it's likely not a simple number like "-123" or "-0xAF"
                         // This check is imperfect for binary/octal if they are not digits.
                         // For now, we keep it simple: if type is numeric, assume sign is part of the number.
                    }
                } else { // Just "+" or "-"
                    actually_numeric_payload = false;
                }

                if(actually_numeric_payload){
                    sign_str = value_str.substr(0, 1);
                    value_str = value_str.substr(1);
                }
            }

            std::string core_content_no_sign = prefix_str + value_str;
            int content_len_with_sign = static_cast<int>(sign_str.length() + core_content_no_sign.length());

            if (spec.has_width() && spec.width > content_len_with_sign) {
                int padding_needed = spec.width - content_len_with_sign;
                std::string padding_chars(padding_needed, spec.fill);

                char effective_align = spec.align;
                bool is_numeric_for_align_default = could_be_numeric;


                if (effective_align == '\0') { // Default alignment
                    effective_align = (is_numeric_for_align_default) ? '>' : '<';
                }

                // If zero ('0') fill is specified with a numeric type,
                // the sign and prefix (if any) appear before the fill characters.
                if (spec.fill == '0' && is_numeric_for_align_default) {
                    if (effective_align == '>') { // Right align (or default for numbers)
                        return sign_str + prefix_str + padding_chars + value_str;
                    } else if (effective_align == '<') { // Left align with zero fill
                        // std::format pads with spaces for left align, even if fill is '0', unless it's after sign/prefix.
                        // This is a bit ambiguous. For simplicity, if fill is '0', we use it.
                        return sign_str + prefix_str + value_str + padding_chars;
                    } else if (effective_align == '^') { // Center align with zero fill
                        // This is also tricky. std::format might use spaces.
                        // Let's do sign + prefix + left_zeros + value + right_zeros. This is not standard.
                        // A simpler interpretation for center with '0' fill for numbers:
                        // sign + prefix + (centered (value with '0' padding on both sides))
                        // For now, let's treat '0' fill for center like other fills for center.
                        int pad_left = padding_needed / 2;
                        int pad_right = padding_needed - pad_left;
                        return std::string(pad_left, spec.fill) + sign_str + prefix_str + value_str + std::string(pad_right, spec.fill);
                    }
                }

                // Standard alignment for non-zero fill, or for zero-fill with non-numeric types
                switch (effective_align) {
                    case '<': // Left align
                        return sign_str + core_content_no_sign + padding_chars;
                    case '>': // Right align
                        return padding_chars + sign_str + core_content_no_sign;
                    case '^': // Center align
                        {
                            int pad_left = padding_needed / 2;
                            int pad_right = padding_needed - pad_left;
                            std::string left_padding(pad_left, spec.fill);
                            std::string right_padding(pad_right, spec.fill);
                            return left_padding + sign_str + core_content_no_sign + right_padding;
                        }
                    default:
                        // Should not happen if effective_align is always set from spec.align or default
                        return sign_str + core_content_no_sign;
                }
            }
            return sign_str + core_content_no_sign; // No padding needed or width too small
        }

        // Base formatter template
        template <typename T, typename CharT = char>
        struct formatter {
            static_assert(sizeof(T) == 0, "No formatter found for this type. Please specialize compat::internal::formatter.");
        };

        // Generic formatter for basic types
        template <typename T>
        std::string format_basic_type(const T& value, ParsedFormatSpec spec) {
            std::stringstream ss_val; // For the main value part
            std::string prefix_str;
            std::string main_formatted_value;
            std::string sign_str_explicit; // To handle sign explicitly, esp for custom binary or when stringstream might not do it as expected

            bool is_integral_type = std::is_integral<T>::value && !std::is_same<T, bool>::value;
            bool is_float_type = std::is_floating_point<T>::value;

            if (is_integral_type) {
                long long ll_value = 0; // Used for formatting signed values or getting absolute
                unsigned long long ull_value = 0; // Used for formatting unsigned or base conversions

                if (std::is_signed<T>::value) {
                    ll_value = static_cast<long long>(value);
                    if (ll_value < 0) {
                        sign_str_explicit = "-"; // Capture sign
                        ull_value = static_cast<unsigned long long>(-ll_value); // Format absolute value
                    } else {
                        ull_value = static_cast<unsigned long long>(ll_value);
                    }
                } else {
                    ull_value = static_cast<unsigned long long>(value);
                    // ll_value = static_cast<long long>(ull_value); // Potentially for ss if it prefers signed
                }

                // Handle value 0 explicitly for correct prefixing and to avoid empty binary string.
                if (value == 0) {
                     main_formatted_value = "0";
                     if (spec.hash_flag) { // C++ std::format behavior for # with zero:
                        if (spec.type == 'x') prefix_str = "0x"; // {:#x} -> 0x0
                        else if (spec.type == 'X') prefix_str = "0X"; // {:#X} -> 0X0
                        else if (spec.type == 'b' || spec.type == 'B') prefix_str = "0b"; // {:#b} -> 0b0
                        // For octal {:#o} of 0 is just "0", no extra "0" prefix.
                     }
                } else { // Non-zero value
                    switch (spec.type) {
                        case 'b': case 'B': {
                            std::string temp_bin_str;
                            unsigned long long num_for_bin = ull_value; // Already absolute
                            if (num_for_bin == 0) temp_bin_str = "0"; // Should be caught by value == 0, but defensive
                            else {
                                while (num_for_bin > 0) {
                                    temp_bin_str += ((num_for_bin % 2) == 0 ? '0' : '1');
                                    num_for_bin /= 2;
                                }
                            }
                            main_formatted_value = std::string(temp_bin_str.rbegin(), temp_bin_str.rend());
                            if (spec.hash_flag) prefix_str = (spec.type == 'b' ? "0b" : "0B");
                            break;
                        }
                        case 'o':
                            ss_val << std::oct << ull_value; // Format absolute value
                            main_formatted_value = ss_val.str();
                            // Add '0' prefix for octal if # specified, value non-zero, and not already prefixed.
                            if (spec.hash_flag && (main_formatted_value.empty() || main_formatted_value[0] != '0')) {
                                prefix_str = "0";
                            }
                            break;
                        case 'x':
                            ss_val << std::hex << ull_value; // Format absolute value
                            main_formatted_value = ss_val.str();
                            if (spec.hash_flag) prefix_str = "0x";
                            break;
                        case 'X':
                            ss_val << std::hex << std::uppercase << ull_value; // Format absolute value
                            main_formatted_value = ss_val.str();
                            if (spec.hash_flag) prefix_str = "0X";
                            break;
                        case 'd': // Decimal
                        case '\0': // Default for integers is decimal
                            if (std::is_signed<T>::value) { // Use original signed value for decimal
                                ss_val << std::dec << ll_value;
                                main_formatted_value = ss_val.str();
                                sign_str_explicit.clear(); // stringstream handles sign for decimal
                            } else {
                                ss_val << std::dec << ull_value;
                                main_formatted_value = ss_val.str();
                            }
                            break;
                        default:
                            throw std::runtime_error("Invalid type specifier '" + std::string(1, spec.type) + "' for integral argument");
                    }
                }
            } else if (is_float_type) {
                // Standard float formatting
                if (spec.type == 'f' || spec.type == 'F' || spec.type == '\0') {
                    if (spec.type == 'f' || spec.type == 'F') ss_val << std::fixed;

                    int prec = -1;
                    if (spec.has_precision()) prec = spec.precision;
                    else if (spec.type == 'f' || spec.type == 'F') prec = 6; // Default for 'f'

                    if (prec != -1) ss_val << std::setprecision(prec);
                } else {
                    // Other float types e, E, a, A, g, G could be added here
                    throw std::runtime_error("Invalid type specifier '" + std::string(1, spec.type) + "' for floating-point argument");
                }
                ss_val << value; // Let stringstream handle float value directly
                main_formatted_value = ss_val.str();

                bool is_inf_nan_val = std::isinf(static_cast<double>(value)) || std::isnan(static_cast<double>(value));

                // Post-processing for 'f'/'F' default precision
                if (!is_inf_nan_val && (spec.type == 'f' || spec.type == 'F') && !spec.has_precision()) {
                    if (main_formatted_value.find('.') == std::string::npos &&
                        main_formatted_value.find('e') == std::string::npos &&
                        main_formatted_value.find('E') == std::string::npos) {
                        main_formatted_value += ".000000";
                    }
                }
                // Hash flag for floats: always include decimal point if not present and not scientific/infinity/nan
                if (!is_inf_nan_val && spec.hash_flag) {
                    if (main_formatted_value.find('.') == std::string::npos &&
                        main_formatted_value.find('e') == std::string::npos &&
                        main_formatted_value.find('E') == std::string::npos) {
                         main_formatted_value += ".";
                    }
                }
            } else {
                 // Bool is handled by its own formatter. This is for other streamable types without type specifiers.
                 if (spec.type != '\0') {
                     throw std::runtime_error("Type specifier '" + std::string(1, spec.type) + "' not allowed for this generic argument type");
                }
                ss_val << value;
                main_formatted_value = ss_val.str();
            }

            // If sign was explicitly captured (e.g. for non-decimal integers), prepend it to the value part that apply_padding_internal will use.
            // apply_padding_internal will then correctly place this sign relative to padding and prefix.
            if (!sign_str_explicit.empty()) {
                 main_formatted_value = sign_str_explicit + main_formatted_value;
            }

            return apply_padding_internal(main_formatted_value, prefix_str, spec);
        }

        template<> struct formatter<const char*> {
            static std::string format(const char* value, const ParsedFormatSpec& spec) {
                // Strings generally don't have a "prefix" in the same way numbers do with base indicators.
                // Pass an empty string for prefix_str.
                return apply_padding_internal(std::string(value), "", spec);
            }
        };
        template<> struct formatter<char*> {
            static std::string format(char* value, const ParsedFormatSpec& spec) {
                return apply_padding_internal(std::string(value), "", spec);
            }
        };
        template<> struct formatter<std::string> {
            static std::string format(const std::string& value, const ParsedFormatSpec& spec) {
                return apply_padding_internal(value, "", spec);
            }
        };
        template<std::size_t N> struct formatter<char[N]> {
            static std::string format(const char(&value)[N], const ParsedFormatSpec& spec) {
                return apply_padding_internal(std::string(value), "", spec);
            }
        };
        template<std::size_t N> struct formatter<const char[N]> {
            static std::string format(const char(&value)[N], const ParsedFormatSpec& spec) {
                return apply_padding_internal(std::string(value), "", spec);
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

        // Example User-defined type
        struct Point {
            int x, y;
        };

        template<> struct formatter<Point> {
            static std::string format(const Point& p, const ParsedFormatSpec& spec) {
                // User can choose how to format. For simplicity, ignore spec for Point members for now.
                // To use spec for members, they could call compat::format internally.
                // E.g., std::string sx = compat::format("{}", p.x);
                //       std::string sy = compat::format("{}", p.y);
                //       std::string point_str = "(" + sx + ", " + sy + ")";
                // For this example, just convert to string simply.
                std::string point_str = "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";

                // The user-defined formatter is responsible for applying padding if desired,
                // or it can return the unpadded string and let a higher level (if any) handle it.
                // Here, we'll use the internal padding function.
                // Note: prefix_str is empty for this Point example unless spec has meaning for it.
                return apply_padding_internal(point_str, "", spec);
            }
        };

        template<> struct formatter<bool> {
            static std::string format(bool value, const ParsedFormatSpec& spec) {
                 if (spec.type != '\0' && spec.type != 'b' && spec.type != 's'){ // s for string-like, b for bool. 'b' is non-standard here, but for consistency.
                    throw std::runtime_error("Invalid type specifier for bool argument");
                }
                // Bool doesn't have a prefix.
                return apply_padding_internal(value ? "true" : "false", "", spec);
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

    enum class IndexingMode : ::std::uint8_t {
        Unknown,
        Automatic,
        Manual
    };

    template <typename OutputIt, typename... Args>
    OutputIt format_to(OutputIt out, const std::string& fmt, Args&&... args) {
        auto arg_tuple = std::make_tuple(std::forward<Args>(args)...);
        std::size_t current_auto_arg_index = 0;
        const std::size_t num_args = sizeof...(Args);
        compat::IndexingMode indexing_mode = compat::IndexingMode::Unknown;

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
                        if (indexing_mode == compat::IndexingMode::Manual) {
                            throw std::runtime_error("Cannot switch from manual (e.g. {0}) to automatic (e.g. {}) argument indexing");
                        }
                        indexing_mode = compat::IndexingMode::Automatic;
                        arg_to_format_idx = current_auto_arg_index;
                        current_auto_arg_index++;
                    } else {
                        if (indexing_mode == compat::IndexingMode::Automatic) {
                            throw std::runtime_error("Cannot switch from automatic (e.g. {}) to manual (e.g. {0}) argument indexing");
                        }
                        indexing_mode = compat::IndexingMode::Manual;
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
