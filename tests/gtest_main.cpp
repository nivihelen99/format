#include "gtest/gtest.h"
#include "../compat_format.hpp" // Adjust path as needed
#include <string>
#include <vector>
#include <sstream> // For cout_redirector if still needed, or use gtest features

// Helper to capture std::cout output for print/println tests
// This can be useful even with GTest if you want to verify exact console output.
struct CoutRedirector {
    std::stringstream buffer;
    std::streambuf* old_cout_rdbuf;

    CoutRedirector() : old_cout_rdbuf(std::cout.rdbuf()) {
        std::cout.rdbuf(buffer.rdbuf());
    }

    std::string get_output() {
        return buffer.str();
    }

    ~CoutRedirector() {
        std::cout.rdbuf(old_cout_rdbuf); // Restore original cout buffer
    }
};

// --- Tests for compat::format ---
TEST(CompatFormat, BasicFormatting) {
    EXPECT_EQ(compat::format("Hello, World!"), "Hello, World!");
    EXPECT_EQ(compat::format("Number: {}", 42), "Number: 42");
    EXPECT_EQ(compat::format("String: {}, Number: {}", "test", 123), "String: test, Number: 123");
    EXPECT_EQ(compat::format("{} {} {}", "one", "two", "three"), "one two three");
    EXPECT_EQ(compat::format("Bool: {} and {}", true, false), "Bool: true and false");
}

TEST(CompatFormat, StringTypes) {
    const char* c_str = "C-string";
    EXPECT_EQ(compat::format("Const char*: {}", c_str), "Const char*: C-string");
    std::string std_str = "std::string";
    EXPECT_EQ(compat::format("Std::string: {}", std_str), "Std::string: std::string");
    char c_array[] = "char array";
    EXPECT_EQ(compat::format("Char array: {}", c_array), "Char array: char array");
}

TEST(CompatFormat, EscapedBraces) {
    EXPECT_EQ(compat::format("Escaped {{}} braces: {{}}"), "Escaped {} braces: {}");
    EXPECT_EQ(compat::format("{{{}}}", 42), "{42}");
    EXPECT_EQ(compat::format("Hello {{}} {}", "World"), "Hello {} World");
}

TEST(CompatFormat, PositionalArguments) {
    EXPECT_EQ(compat::format("{1}, {0}", "zero", "one"), "one, zero");
    EXPECT_EQ(compat::format("Number: {1}, String: {0}", "text", 99), "Number: 99, String: text");
    EXPECT_EQ(compat::format("{0} {1} {0}", "A", "B"), "A B A");
}

TEST(CompatFormat, ZeroArguments) {
    EXPECT_EQ(compat::format("Zero args: "), "Zero args: ");
}

TEST(CompatFormat, MultipleArguments) {
    // Note: std::to_string for double usually gives 6 decimal places.
    // The new formatter might change this, especially with precision specifiers.
    // For now, testing with default float formatting. Float 3.0 now formats as "3" by default.
    EXPECT_EQ(compat::format("Five args: {} {} {} {} {}", 1, "two", 3.0, false, "end"), "Five args: 1 two 3 false end");
}


// --- Tests for compat::print and compat::println ---
TEST(CompatPrint, BasicPrint) {
    CoutRedirector redir;
    compat::print("Hello, {}!", "print");
    EXPECT_EQ(redir.get_output(), "Hello, print!");
}

TEST(CompatPrint, BasicPrintln) {
    CoutRedirector redir;
    compat::println("Hello, {}!", "println");
    EXPECT_EQ(redir.get_output(), "Hello, println!\n");
}

TEST(CompatPrint, PrintWithPositionalArgs) {
    CoutRedirector redir;
    compat::print("Numbers: {0}, {1}, {2}", 10, 20, 30);
    EXPECT_EQ(redir.get_output(), "Numbers: 10, 20, 30");
}

TEST(CompatPrint, PrintlnWithEscapedBraces) {
    CoutRedirector redir;
    compat::println("Escaped {{}} and arg: {}", "val");
    EXPECT_EQ(redir.get_output(), "Escaped {} and arg: val\n");
}

// --- Tests for compat::format_to ---
TEST(CompatFormatTo, FormatToString) {
    std::string s;
    compat::format_to(std::back_inserter(s), "Format to string: {}, {}", "data", 123);
    EXPECT_EQ(s, "Format to string: data, 123");
}

TEST(CompatFormatTo, FormatToVectorChar) {
    std::vector<char> vec;
    compat::format_to(std::back_inserter(vec), "To vector: {}", "vec_test");
    std::string vec_str(vec.begin(), vec.end());
    EXPECT_EQ(vec_str, "To vector: vec_test");
}

// --- Tests for Error Conditions ---
TEST(CompatErrorHandling, UnmatchedOpenBrace) {
    EXPECT_THROW(compat::format("Hello {", "world"), std::runtime_error);
    try {
        compat::format("Hello {", "world");
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Unmatched '{'"), std::string::npos);
    }
}

TEST(CompatErrorHandling, UnmatchedCloseBrace) {
    EXPECT_THROW(compat::format("Hello }", "world"), std::runtime_error);
     try {
        compat::format("Hello }", "world");
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Unmatched '}'"), std::string::npos);
    }
}

TEST(CompatErrorHandling, AutoIndexArgOutOfBounds) {
    EXPECT_THROW(compat::format("Hello {} {}", "world"), std::runtime_error); // Needs 2, gets 1
    try {
        compat::format("Hello {} {}", "world");
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("out of bounds"), std::string::npos);
    }
}

TEST(CompatErrorHandling, ManualIndexArgOutOfBounds) {
    EXPECT_THROW(compat::format("Hello {1}", "world"), std::runtime_error); // Needs arg 1, only has 0
     try {
        compat::format("Hello {1}", "world");
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("out of bounds"), std::string::npos);
    }
}

TEST(CompatErrorHandling, NoArgsButPlaceholderExists) {
    EXPECT_THROW(compat::format("Hello {}"), std::runtime_error);
    try {
        compat::format("Hello {}");
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("out of bounds"), std::string::npos);
        EXPECT_NE(std::string(e.what()).find("no arguments provided"), std::string::npos);
    }
}

TEST(CompatErrorHandling, InvalidPlaceholderContentNonNumericIndex) {
    // This test is for the new parser. Old parser might have different error.
    // Example: "{abc}" where "abc" is not a specifier nor a valid index.
    EXPECT_THROW(compat::format("Hello {abc}", "world"), std::runtime_error);
     try {
        compat::format("Hello {abc}", "world");
    } catch (const std::runtime_error& e) {
        // Error message from parse_placeholder_content or stoul in format_to
        EXPECT_TRUE(std::string(e.what()).find("non-numeric argument index") != std::string::npos ||
                    std::string(e.what()).find("Invalid characters at end of format specifier") != std::string::npos);
    }
}

TEST(CompatErrorHandling, MixedAutoAndManualIndexingError) {
    // Test for the new rule: disallow mixing {} and {N}
    EXPECT_THROW(compat::format("{1} then {}", "A", "B"), std::runtime_error);
    try {
        compat::format("{1} then {}", "A", "B");
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Cannot switch from manual"), std::string::npos);
    }

    EXPECT_THROW(compat::format("{} then {0}", "A", "B"), std::runtime_error);
    try {
        compat::format("{} then {0}", "A", "B");
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Cannot switch from automatic"), std::string::npos);
    }
}

// --- Tests for Format Specifiers (New Tests) ---
TEST(CompatFormatSpecifiers, WidthAndAlignmentStrings) {
    EXPECT_EQ(compat::format("{:<10}", "test"), "test      ");
    EXPECT_EQ(compat::format("{:>10}", "test"), "      test");
    EXPECT_EQ(compat::format("{:^10}", "test"), "   test   ");
    EXPECT_EQ(compat::format("{:*<10}", "test"), "test******");
    EXPECT_EQ(compat::format("{:*>10}", "test"), "******test");
    EXPECT_EQ(compat::format("{:*^10}", "test"), "***test***");
    EXPECT_EQ(compat::format("{:10}", "longstring"), "longstring"); // No truncation by default
    EXPECT_EQ(compat::format("{:5}", "longstring"), "longstring");
}

TEST(CompatFormatSpecifiers, WidthAndAlignmentNumbers) {
    EXPECT_EQ(compat::format("{:<10}", 123), "123       ");
    EXPECT_EQ(compat::format("{:>10}", 123), "       123");
    EXPECT_EQ(compat::format("{:^10}", 123), "   123    "); // Note: default for number might be right, check impl.
                                                       // My apply_padding defaults to left if align not specified
                                                       // For numbers, std::format usually defaults to right.
                                                       // Let's test assuming current apply_padding logic. Or refine apply_padding.
                                                       // For now, explicit align is better.
    EXPECT_EQ(compat::format("{:0>10}", 123), "0000000123"); // Zero padding (fill + right align)
    EXPECT_EQ(compat::format("{:*>10}", 123), "*******123");
    EXPECT_EQ(compat::format("{:*^10}", 123), "***123****");
}

TEST(CompatFormatSpecifiers, PrecisionFloats) {
    EXPECT_EQ(compat::format("{:.2f}", 3.14159), "3.14");
    EXPECT_EQ(compat::format("{:.0f}", 3.14159), "3");
    EXPECT_EQ(compat::format("{:.3f}", 3.14), "3.140");
    EXPECT_EQ(compat::format("{:f}", 3.14159), "3.141590"); // Default precision for 'f' is often 6
}

TEST(CompatFormatSpecifiers, WidthAndPrecisionFloats) {
    EXPECT_EQ(compat::format("{:10.2f}", 3.14159), "      3.14"); // Default align for float is right
    EXPECT_EQ(compat::format("{:<10.2f}", 3.14159), "3.14      ");
    EXPECT_EQ(compat::format("{:^10.2f}", 3.14159), "   3.14   ");
    EXPECT_EQ(compat::format("{:010.2f}", 3.14159), "0000003.14"); // Zero padding for floats
}

TEST(CompatFormatSpecifiers, EmptySpecifier) {
    EXPECT_EQ(compat::format("{:}", 123), "123");
    EXPECT_EQ(compat::format("{:}", "abc"), "abc");
}

TEST(CompatFormatSpecifiers, PositionalWithSpecifiers) {
    EXPECT_EQ(compat::format("{1:>10.2f}, {0:*<8}", "str", 3.14159), "      3.14, str*****");
}

TEST(CompatFormatSpecifiers, InvalidSpecifiers) {
    // Valid: fill 'x', align '<', width 10 for "test"
    EXPECT_EQ(compat::format("{:x<10}", "test"), "testxxxxxx");
    // Precision without digits - should throw
    EXPECT_THROW(compat::format("{:.}", 3.14), std::runtime_error);
    // Invalid char in spec
    EXPECT_THROW(compat::format("{:10#}", 3.14), std::runtime_error);
    // Width out of range (if parse_placeholder_content checks this, depends on stoi behavior)
    // std::stoi might throw std::out_of_range which is caught by parse_placeholder_content
    // EXPECT_THROW(compat::format("{:99999999999999999999}", 123), std::runtime_error); // Large width
}

TEST(CompatFormatSpecifiers, IntegerBaseFormatting) {
    // Binary
    EXPECT_EQ(compat::format("{:b}", 0), "0");
    EXPECT_EQ(compat::format("{:b}", 10), "1010");
    EXPECT_EQ(compat::format("{:B}", 10), "1010"); // Same output for 'b' and 'B' in value, could differ with #
    EXPECT_EQ(compat::format("{:#b}", 10), "0b1010");
    EXPECT_EQ(compat::format("{:#B}", 10), "0B1010");
    EXPECT_EQ(compat::format("{:b}", -10), "-1010"); // Current impl: sign then abs binary
    EXPECT_EQ(compat::format("{:#b}", -10), "-0b1010");
    EXPECT_EQ(compat::format("{:#b}", 0), "0b0"); // std::format behavior

    // Octal
    EXPECT_EQ(compat::format("{:o}", 0), "0");
    EXPECT_EQ(compat::format("{:o}", 10), "12");
    EXPECT_EQ(compat::format("{:#o}", 10), "012");
    EXPECT_EQ(compat::format("{:o}", -10), "-12");
    EXPECT_EQ(compat::format("{:#o}", -10), "-012");
    EXPECT_EQ(compat::format("{:#o}", 0), "0"); // std::format behavior for #o with 0

    // Hexadecimal
    EXPECT_EQ(compat::format("{:x}", 0), "0");
    EXPECT_EQ(compat::format("{:x}", 26), "1a");
    EXPECT_EQ(compat::format("{:X}", 26), "1A");
    EXPECT_EQ(compat::format("{:#x}", 26), "0x1a");
    EXPECT_EQ(compat::format("{:#X}", 26), "0X1A");
    EXPECT_EQ(compat::format("{:x}", -26), "-1a");
    EXPECT_EQ(compat::format("{:X}", -26), "-1A");
    EXPECT_EQ(compat::format("{:#x}", -26), "-0x1a");
    EXPECT_EQ(compat::format("{:#X}", -26), "-0X1A");
    EXPECT_EQ(compat::format("{:#x}", 0), "0x0"); // std::format behavior
    EXPECT_EQ(compat::format("{:#X}", 0), "0X0"); // std::format behavior

    // Decimal (default)
    EXPECT_EQ(compat::format("{:d}", 10), "10");
    EXPECT_EQ(compat::format("{:d}", -10), "-10");
    EXPECT_EQ(compat::format("{}", 10), "10"); // Default type for int

    // Invalid type specifiers for integers
    EXPECT_THROW(compat::format("{:f}", 10), std::runtime_error);
    EXPECT_THROW(compat::format("{:s}", 10), std::runtime_error);
}

TEST(CompatFormatSpecifiers, IntegerBaseFormattingWithPadding) {
    // Binary with padding
    EXPECT_EQ(compat::format("{:08b}", 10), "00001010");
    EXPECT_EQ(compat::format("{:#010b}", 10), "0b00001010"); // Expected: prefix, then zeros, then value
    EXPECT_EQ(compat::format("{:#010B}", 10), "0B00001010"); // Expected: prefix, then zeros, then value. Actual: "0B10100000"
    EXPECT_EQ(compat::format("{:*>10b}", 10), "******1010");
    EXPECT_EQ(compat::format("{:*>#10b}", 10), "****0b1010"); // Corrected expectation: 10 - (len("0b")+len("1010")) = 10-6=4 asterisks
    EXPECT_EQ(compat::format("{:08b}", -10), "-0001010");
    EXPECT_EQ(compat::format("{:#010b}", -10), "-0b0001010"); // Corrected expectation: 10 - (len("-0b")+len("1010")) = 10-7=3 zeros

    // Octal with padding
    EXPECT_EQ(compat::format("{:08o}", 10), "00000012");
    EXPECT_EQ(compat::format("{:#08o}", 10), "00000012"); // My impl: prefix '0' replaces a padding '0' if it fits.
                                                      // std::format: "00000o12" (prefix not counted in width for zero pad logic as much)
                                                      // Let's test current behavior: apply_padding_internal treats "0" and "12"
                                                      // For "{:#08o}", 10 -> prefix "0", value "12". apply_padding_internal("12", "0", spec)
                                                      // sign="", prefix="0", value="12". If width 8, fill '0'.
                                                      // result: "0" + "00000" + "12" = "00000012" - this is good.
    EXPECT_EQ(compat::format("{:#08o}", 010), "00000010"); // Test with actual octal input (value 8)
    EXPECT_EQ(compat::format("{:#8o}", 0), "       0"); // Default align right for numbers
    EXPECT_EQ(compat::format("{:#08o}", 0), "00000000"); // #o for 0 is "0", then padded.

    // Hex with padding
    EXPECT_EQ(compat::format("{:08x}", 26), "0000001a");
    EXPECT_EQ(compat::format("{:#08x}", 26), "0x00001a");
    EXPECT_EQ(compat::format("{:#08X}", 26), "0X00001A");
    EXPECT_EQ(compat::format("{:*>10x}", 26), "********1a");
    EXPECT_EQ(compat::format("{:*>#10x}", 26), "******0x1a"); // Corrected expectation: 10 - len("0x1a") = 6 asterisks
    EXPECT_EQ(compat::format("{:08x}", -26), "-000001a");
    EXPECT_EQ(compat::format("{:#08x}", -26), "-0x0001a");
}

TEST(CompatFormatSpecifiers, FloatHashFlag) {
    EXPECT_EQ(compat::format("{:#f}", 3.0), "3.000000"); // Default precision 6, # ensures . is there
    EXPECT_EQ(compat::format("{:#.0f}", 3.0), "3.");     // Precision 0, # ensures . is there
    EXPECT_EQ(compat::format("{:.0f}", 3.0), "3");      // Precision 0, no #
    EXPECT_EQ(compat::format("{:#f}", 3.14), "3.140000"); // Already has ., default prec
    EXPECT_EQ(compat::format("{:#.2f}", 3.14159), "3.14");
    // EXPECT_EQ(compat::format("{:#g}", 3.0), "3."); // General format 'g' not implemented
    // EXPECT_EQ(compat::format("{:g}", 3.0), "3");   // General format 'g' not implemented
    // Test with inf/nan - hash should not add a decimal point
    double inf_val = std::numeric_limits<double>::infinity();
    double nan_val = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(compat::format("{:f}", inf_val), "inf"); // Or "infinity" depending on system
    EXPECT_EQ(compat::format("{:#f}", inf_val), "inf");
    // Note: NaN string representation can vary ("nan", "nan(ind)", etc.)
    // We'll rely on stringstream's output for these.
    std::string nan_str_default = compat::format("{}", nan_val);
    EXPECT_EQ(compat::format("{:#f}", nan_val), nan_str_default);

}

// --- Tests for User-Defined Types ---
TEST(CompatFormatUserDefined, PointFormatting) {
    compat::internal::Point p{10, 20};
    EXPECT_EQ(compat::format("{}", p), "(10, 20)");

    // Test with width and alignment
    // Point p{10, 20} -> "(10, 20)" which has length 8
    EXPECT_EQ(compat::format("{:>15}", p), "       (10, 20)"); // 15 - 8 = 7 spaces
    EXPECT_EQ(compat::format("{:*<15}", p), "(10, 20)*******"); // 7 asterisks
    EXPECT_EQ(compat::format("{:*^15}", p), "***(10, 20)****"); // 3 leading, 4 trailing asterisks for 7 total

    compat::internal::Point p_neg{-5, -150}; // "(-5, -150)" which has length 10
    EXPECT_EQ(compat::format("{}", p_neg), "(-5, -150)");
    // For {:015}, Point has spec.type='\0', so apply_padding_internal treats it as numeric for default alignment.
    // Default align becomes '>', spec.fill='0'.
    // Expected: "00000(-5, -150)"
    EXPECT_EQ(compat::format("{:015}", p_neg), "00000(-5, -150)");
    EXPECT_EQ(compat::format("{:0<15}", p_neg), "(-5, -150)00000");
    EXPECT_EQ(compat::format("{:0>15}", p_neg), "00000(-5, -150)");


    // What if a user's formatter for Point itself used compat::format for its members?
    // That's up to the user's Point::format specialization. The current one uses std::to_string.
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
