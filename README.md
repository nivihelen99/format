# compat_format: A C++17 `std::format`-like Library

`compat_format` is a header-only C++17 library that provides functionality similar to the C++20/C++23 `std::format`, `std::print`, and `std::println`. It's designed for projects that need such formatting capabilities but are still using C++17.

The library also includes a mechanism (`COMPAT_USE_STD_FORMAT`) to automatically switch to using the standard library's `<format>` and `<print>` if a C++23-compliant compiler and standard library are detected.

## Features

*   **`compat::format(fmt, args...)`**: Returns a formatted `std::string`.
*   **`compat::print(fmt, args...)`**: Prints formatted output to `std::cout`.
*   **`compat::println(fmt, args...)`**: Prints formatted output to `std::cout`, followed by a newline.
*   **`compat::format_to(out_iterator, fmt, args...)`**: Writes formatted output to the given output iterator.
*   **Placeholder Syntax**:
    *   Automatic indexing: `{}` (arguments are used in order).
    *   Manual indexing: `{N}` (e.g., `{0}`, `{1}`).
    *   **Note**: Mixing automatic and manual indexing in the same format string is disallowed and will result in a `std::runtime_error`.
*   **Format Specifiers**: Placeholders can include specifiers after a colon (`:`), e.g., `{0:spec}` or `{:spec}`.
    *   **Syntax**: `[arg_id_or_empty]:[fill_char][align_char][width][.precision][type_char]`
    *   **Fill and Alignment**:
        *   `fill_char`: The character to use for padding (e.g., `*`, `0`, ` `).
        *   `align_char`:
            *   `<`: Left alignment (default for non-numeric types).
            *   `>`: Right alignment (default for numeric types if only width is given).
            *   `^`: Center alignment.
        *   Example: `{*<10}` (fill with `*`, left-align, width 10). `{:>5}` (fill with space, right-align, width 5).
    *   **Width**: A non-negative integer specifying the minimum field width.
        *   Example: `{:10}`.
    *   **Precision**: A non-negative integer preceded by a dot (`.`).
        *   For floating-point types (when `f` type is used or by default for floats): specifies the number of digits after the decimal point.
        *   Example: `{:.2f}` (fixed-point, 2 decimal places).
    *   **Type**:
        *   `f`: Fixed-point notation for floating-point numbers.
*   **Escaped Braces**: `{{` and `}}` are used to print literal `{` and `}` characters.
*   **Type Support**: Handles standard C++ types like integers, floating-point numbers, `bool`, `std::string`, C-style strings (`const char*`, `char[]`).
    *   **Integer Base Formatting**:
        *   `b`, `B`: Binary format.
        *   `o`: Octal format.
        *   `x`, `X`: Hexadecimal format (lowercase `x`, uppercase `X`).
        *   `#`: Alternate form prefix (e.g., `0b`, `0`, `0x`, `0X`).
    *   **User-Defined Types**: Support for formatting user-defined types by specializing `compat::internal::formatter<YourType>`.
*   **Error Handling**: Throws `std::runtime_error` for invalid format strings, mismatched arguments, or parsing errors.

## Usage

Include the header:
```cpp
#include "compat_format.hpp"
```

### `compat::format`
Returns a `std::string`.
```cpp
#include <string>
#include <iostream>

int main() {
    std::string s1 = compat::format("Hello, {}!", "World");
    std::cout << s1 << std::endl; // Output: Hello, World!

    std::string s2 = compat::format("The number is {0}, and the string is {1}.", 42, "test");
    std::cout << s2 << std::endl; // Output: The number is 42, and the string is test.

    std::string s3 = compat::format("Value: {0:.2f}, Padded: '{1:*>10}'", 3.14159, "val");
    std::cout << s3 << std::endl; // Output: Value: 3.14, Padded: '*******val'
}
```

### `compat::print` and `compat::println`
Prints directly to `std::cout`. `println` adds a newline.
```cpp
int main() {
    compat::print("This is a print. ");
    compat::println("This is a println."); // Adds a newline

    compat::println("User: {}, ID: {:04}", "Alice", 7); // Output: User: Alice, ID: 0007
}
```

### `compat::format_to`
Writes to any output iterator.
```cpp
#include <vector>
#include <string>
#include <iterator> // For std::back_inserter
#include <iostream>

int main() {
    std::string s;
    compat::format_to(std::back_inserter(s), "Formatted to string: {}.", 123);
    std::cout << s << std::endl; // Output: Formatted to string: 123.

    std::vector<char> vec;
    compat::format_to(std::back_inserter(vec), "Vector: {}, {}", "abc", 45.6);
    std::cout << std::string(vec.begin(), vec.end()) << std::endl; // Output: Vector: abc, 45.600000
}
```

### Format Specifier Examples

```cpp
// Alignment and Padding
compat::println("Left align:   '{}'", compat::format("{:<10}", "text"));   // 'text      '
compat::println("Right align:  '{}'", compat::format("{:>10}", "text"));   // '      text'
compat::println("Center align: '{}'", compat::format("{:^10}", "text"));   // '   text   '
compat::println("Fill/align: '{}'", compat::format("{:*^10}", "text")); // '***text***'

// Numbers
compat::println("Integer padding: '{}'", compat::format("{:05}", 123));      // '00123'
compat::println("Float precision: '{}'", compat::format("{:.3f}", 12.34567)); // '12.346' (rounding may occur)
compat::println("Float width & prec: '{}'", compat::format("{:10.2f}", 12.34567)); // '     12.35'
compat::println("Float fill/align/prec: '{}'", compat::format("{:0>10.2f}", 12.34567)); // '0000012.35'

// Positional arguments with specifiers
compat::println("Mixed: {1:.2f} then {0:*<5}", "str", 9.876); // Output: 9.88 then str**

// Integer Base Formatting
compat::println("Binary: {0:b} / {0:#b} / {0:#010b}", 42);      // Binary: 101010 / 0b101010 / 0b00101010
compat::println("Octal: {0:o} / {0:#o} / {0:#08o}", 42);        // Octal: 52 / 052 / 00000052
compat::println("Hex: {0:x} / {0:#X} / {0:#08x}", 42);          // Hex: 2a / 0X2A / 0x00002a
compat::println("Negative Hex: {:#x}", -42);                     // Negative Hex: -0x2a

// User-Defined Type Formatting (example with a Point struct)
// Assuming Point struct and its compat::internal::formatter specialization:
// struct Point { int x, y; };
// template<> struct compat::internal::formatter<Point> {
//     static std::string format(const Point& p, const compat::internal::ParsedFormatSpec& spec) {
//         std::string point_str = "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
//         return compat::internal::apply_padding_internal(point_str, "", spec);
//     }
// };
compat::internal::Point pt{10, 20}; // Needs Point to be accessible or defined
compat::println("Point: {}", pt);                                 // Point: (10, 20)
compat::println("Padded Point: '{:*>15}'", pt);                   // Padded Point: '*****(10, 20)'
```

### Defining Formatters for Custom Types
To enable formatting for your own types, you need to specialize `compat::internal::formatter<YourType>` within the `compat::internal` namespace. The specialization must provide a static method:
`static std::string format(const YourType& value, const ParsedFormatSpec& spec);`

Example for a `Point` struct:
```cpp
// In your code, or a header included after compat_format.hpp if Point is defined there
namespace compat { namespace internal { // Open the namespace

struct UserPoint { // Example, could be defined anywhere accessible
    int x, y;
};

template<>
struct formatter<UserPoint> {
    static std::string format(const UserPoint& p, const ParsedFormatSpec& spec) {
        // Create the string representation of UserPoint
        std::string point_str = "(" + std::to_string(p.x) + ", " +
                                  std::to_string(p.y) + ")";

        // Use library's padding logic if desired (prefix is usually empty for non-numeric custom types)
        return apply_padding_internal(point_str, "", spec);
        // Or, for more complex needs, call compat::format recursively:
        // return compat::format("({0},{1})", p.x, p.y); // this would not use 'spec' for the Point itself
                                                       // but for p.x and p.y if they had specs.
    }
};

}} // Close compat::internal

// Then you can use it:
// UserPoint my_point{1, 2};
// compat::println("My Point: {}", my_point); // Output: My Point: (1, 2)
// compat::println("My Point padded: '{:*>15}'", my_point); // Output: My Point padded: '*****(1, 2)'
```


## Building with CMake

This project uses CMake.

1.  **Prerequisites**:
    *   CMake (version 3.14 or higher)
    *   A C++17 compliant compiler (e.g., GCC, Clang, MSVC)

2.  **Configuration & Build**:
    ```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```

3.  **Targets**:
    *   `compat_format`: (Interface library, no actual build output for this target itself as it's header-only, but used by other targets).
    *   `example`: Builds `main.cpp` into an executable named `example`.
        ```bash
        cmake --build . --target example
        ./example # or .\example.exe on Windows
        ```
    *   `gtest_runner`: Builds the Google Test suite from `tests/gtest_main.cpp`. Google Test will be downloaded via `FetchContent` if not found locally.
        ```bash
        cmake --build . --target gtest_runner # or just `cmake --build .`
        ```

4.  **Running Tests**:
    After building, you can run tests using CTest:
    ```bash
    ctest # From the build directory
    ```
    Or run the `gtest_runner` executable directly:
    ```bash
    ./gtest_runner # or .\gtest_runner.exe on Windows (path might be in a subfolder like Debug)
    ```

## Transitioning to C++23 `std::format` and `std::print`

When your project/compiler fully supports C++23's `<format>` and `<print>`:

1.  The `compat_format.hpp` header automatically detects if standard C++23 `<format>` and `<print>` are available (via `__has_include` and feature test macros like `__cpp_lib_format` and `__cpp_lib_print`).
2.  If detected, the `COMPAT_USE_STD_FORMAT` macro is defined to `1`.
3.  In this mode, `compat::format`, `compat::print`, and `compat::println` become aliases for `std::format`, `std::print`, and `std::println` respectively. `compat::format_to` is also implemented using `std::format`.

This allows for a smoother transition. You can start using `compat::` functions, and they will automatically leverage the standard library versions when available, without requiring code changes in your application.

**Note**: The custom C++17 implementation in this library might not support all the features or have the exact same behavior in all edge cases as the standard `std::format` (e.g., complex type specifiers, locale support, chrono support are not implemented here). The goal is to provide a good subset of the most commonly used features. When `COMPAT_USE_STD_FORMAT` is active, you get the full standard behavior.

## Future Enhancements (Not Yet Implemented)

*   UTF-8 and wide character support (`wchar_t`, `char16_t`, `char32_t`).
*   Locale-aware formatting.
*   More extensive error checking for format specifier validity for types beyond basic numerics/strings.
*   Additional `std::format` type specifiers for floats (e.g., `e`, `g`, `a`) and characters (`c`).