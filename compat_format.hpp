#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <stdexcept> // For std::runtime_error
#include <iostream>  // For std::cout, std::ostream_iterator
#include <iterator>  // For std::back_inserter, std::ostream_iterator
#include <algorithm> // For std::copy (potentially)
#include <utility>   // For std::forward, std::move

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
        // Helper to convert individual arguments to string
        template <typename T, typename CharT = char> // CharT for future wide char support
        struct formatter {
            // Default: try to use format_value if available, otherwise error.
            // This would ideally be a static_assert(false, "No formatter for this type"),
            // but that's tricky to make dependent in C++17.
            // For now, lack of specialization will lead to a compile error.
        };

        // Specializations for format_value (moved into formatter struct for better organization)
        // We could also use standalone format_value functions and SFINAE.

        template<> struct formatter<const char*> {
            static std::string format(const char* value, const std::string& /*fmt_spec*/) {
                return std::string(value);
            }
        };
        template<> struct formatter<std::string> {
            static std::string format(const std::string& value, const std::string& /*fmt_spec*/) {
                return value;
            }
        };
        // For C-style arrays of char (string literals)
        template<std::size_t N> struct formatter<char[N]> {
            static std::string format(const char(&value)[N], const std::string& /*fmt_spec*/) {
                return std::string(value);
            }
        };
        template<std::size_t N> struct formatter<const char[N]> {
            static std::string format(const char(&value)[N], const std::string& /*fmt_spec*/) {
                return std::string(value);
            }
        };


        // Macro to easily define formatters for std::to_string compatible types
        #define DEFINE_FORMATTER_STD_TO_STRING(TYPE) \
            template<> struct formatter<TYPE> { \
                static std::string format(TYPE value, const std::string& /*fmt_spec*/) { \
                    return std::to_string(value); \
                } \
            }

        DEFINE_FORMATTER_STD_TO_STRING(int);
        DEFINE_FORMATTER_STD_TO_STRING(unsigned int);
        DEFINE_FORMATTER_STD_TO_STRING(long);
        DEFINE_FORMATTER_STD_TO_STRING(unsigned long);
        DEFINE_FORMATTER_STD_TO_STRING(long long);
        DEFINE_FORMATTER_STD_TO_STRING(unsigned long long);
        DEFINE_FORMATTER_STD_TO_STRING(float);
        DEFINE_FORMATTER_STD_TO_STRING(double);
        DEFINE_FORMATTER_STD_TO_STRING(long double);
        // Add bool here, as std::to_string doesn't support it directly
        template<> struct formatter<bool> {
            static std::string format(bool value, const std::string& /*fmt_spec*/) {
                return value ? "true" : "false";
            }
        };


        // Argument visitor using std::apply (available in C++17 via <tuple>)
        // This helper will call the appropriate formatter for the Nth argument.
        template <std::size_t I, typename Tuple, typename OutputIt>
        void format_nth_arg_to(OutputIt& out, const Tuple& tpl, const std::string& fmt_spec) {
            // std::get<I>(tpl) gets the argument
            // We then remove const, volatile, and reference qualifiers to get the base type for formatter lookup
            using OriginalType = typename std::tuple_element<I, Tuple>::type;
            using DecayedType = typename std::decay<OriginalType>::type;
            
            // Pass fmt_spec to the formatter.
            std::string formatted_val = formatter<DecayedType>::format(std::get<I>(tpl), fmt_spec);
            for (char ch : formatted_val) {
                *out++ = ch;
            }
        }
        
        // Overload for when index I is out of bounds of the tuple (compile-time check via enable_if)
        // This is more of a conceptual placeholder as direct out-of-bounds access is a compile error with std::get
        // Runtime checks for argument count vs placeholder count are done elsewhere.

    // C++17 compatible dispatch helper struct
    template <typename OutputIt, typename Tuple>
    struct DispatchHelper {
        OutputIt& out_ref;
        const Tuple& tuple_ref;
        const std::string& fmt_spec_ref;
        std::size_t target_idx_val;
        bool& processed_flag_ref;

        DispatchHelper(OutputIt& o, const Tuple& t, const std::string& fs, std::size_t tidx, bool& pf)
            : out_ref(o), tuple_ref(t), fmt_spec_ref(fs), target_idx_val(tidx), processed_flag_ref(pf) {}

        template <std::size_t... Is>
        void do_dispatch(std::index_sequence<Is...>) {
            (void)std::initializer_list<int>{
                ((Is == target_idx_val ? (internal::format_nth_arg_to<Is>(out_ref, tuple_ref, fmt_spec_ref), processed_flag_ref = true) : false), 0)...
            };
        }
    };

    } // namespace internal


    // ##########################################################################
    // ## Public API implementations (C++17 mode)
    // ##########################################################################

    template <typename OutputIt, typename... Args>
    OutputIt format_to(OutputIt out, const std::string& fmt, Args&&... args) {
        auto arg_tuple = std::make_tuple(std::forward<Args>(args)...);
        std::size_t current_arg_index = 0;
        const std::size_t num_args = sizeof...(Args);

        for (std::size_t i = 0; i < fmt.length(); ++i) {
            if (fmt[i] == '{') {
                if (i + 1 < fmt.length() && fmt[i+1] == '{') { // Escaped {{
                    *out++ = '{';
                    i++; // Skip the second '{'
                } else {
                    std::size_t placeholder_end = fmt.find('}', i + 1);
                    if (placeholder_end == std::string::npos) {
                        throw std::runtime_error("Unmatched '{' in format string");
                    }
                    
                    std::string placeholder_content = fmt.substr(i + 1, placeholder_end - (i + 1));
                    std::string fmt_spec; // For future specifiers like :<8
                    
                    std::size_t arg_to_format_idx;
                    // bool explicit_idx_used_this_time = false; // Keep track if this specific placeholder used an explicit index
                    if (placeholder_content.empty()) {
                        arg_to_format_idx = current_arg_index;
                        current_arg_index++; 
                    } else {
                        std::size_t parsed_idx;
                        bool parse_success = false;
                        try {
                            std::string potential_idx_str = placeholder_content;
                            // TODO: Parse for ':' for fmt_spec
                            parsed_idx = std::stoul(potential_idx_str);
                            parse_success = true;
                        } catch (const std::invalid_argument& ) {
                            throw std::runtime_error("Invalid format placeholder content: expected empty or an index");
                        } catch (const std::out_of_range& ) {
                            throw std::runtime_error("Invalid format placeholder index: out of range for stoul");
                        }

                        if(parse_success){
                            arg_to_format_idx = parsed_idx;
                            // explicit_idx_used_this_time = true; 
                            // current_arg_index is NOT incremented when an explicit index is used.
                        }
                    }

                    if (arg_to_format_idx >= num_args) {
                         if (num_args == 0) {
                            throw std::runtime_error("Argument index out of bounds: " + std::to_string(arg_to_format_idx) + " requested but no arguments were provided.");
                        }
                        throw std::runtime_error("Argument index out of bounds: " + std::to_string(arg_to_format_idx) + " requested but only " + std::to_string(num_args) + " arguments were provided.");
                    }
                    
                    bool processed_dispatch = false;
                    
                    // Use the DispatchHelper from compat::internal
                    internal::DispatchHelper<OutputIt, decltype(arg_tuple)> dispatcher(out, arg_tuple, fmt_spec, arg_to_format_idx, processed_dispatch);
                    
                    // Dispatcher call can be unconditional as make_index_sequence<0> is fine (empty sequence).
                    // The logic within do_dispatch and the processed_dispatch flag handles cases correctly.
                    dispatcher.do_dispatch(std::make_index_sequence<sizeof...(Args)>{});

                    if (!processed_dispatch && num_args > 0 && arg_to_format_idx < num_args) { // arg_to_format_idx < num_args is important here
                        throw std::runtime_error("Internal error: argument dispatch failed for index " + std::to_string(arg_to_format_idx));
                    }
                    
                    i = placeholder_end; 
                }
            } else if (fmt[i] == '}') {
                if (i + 1 < fmt.length() && fmt[i+1] == '}') { // Escaped }}
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
        // Ensure this calls compat::format_to, not some internal one.
        // Since format_to is now correctly in compat namespace, this should be fine.
        format_to(std::back_inserter(s), fmt, std::forward<Args>(args)...);
        return s;
    }

    template <typename... Args>
    void print(const std::string& fmt, Args&&... args) {
        // Ensure this calls compat::format_to
        format_to(std::ostream_iterator<char>(std::cout), fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void println(const std::string& fmt, Args&&... args) {
        // Ensure this calls compat::format_to
        format_to(std::ostream_iterator<char>(std::cout), fmt, std::forward<Args>(args)...);
        std::cout << '\n';
    }

#endif // COMPAT_USE_STD_FORMAT

} // namespace compat
