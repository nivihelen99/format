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
    // For now, testing with default float formatting.
    EXPECT_EQ(compat::format("Five args: {} {} {} {} {}", 1, "two", 3.0, false, "end"), "Five args: 1 two 3.000000 false end");
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


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
