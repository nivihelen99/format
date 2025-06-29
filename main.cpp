#include "compat_format.hpp"
#include <iostream>     // For std::cout, std::endl
#include <string>       // For std::string
#include <vector>       // For std::vector
#include <cassert>      // For assert
#include <sstream>      // For std::stringstream
#include <stdexcept>    // For std::runtime_error
#include <iterator>     // For std::back_inserter

// Helper to capture output for testing print functions
struct cout_redirector {
    std::stringstream buffer;
    std::streambuf* old_cout_rdbuf;

    cout_redirector() : old_cout_rdbuf(std::cout.rdbuf()) {
        std::cout.rdbuf(buffer.rdbuf());
    }

    std::string get_output() {
        return buffer.str();
    }

    ~cout_redirector() {
        std::cout.rdbuf(old_cout_rdbuf);
    }
};

void test_format() {
    std::cout << "--- Testing compat::format ---" << std::endl;

    assert(compat::format("Hello, World!") == "Hello, World!");
    assert(compat::format("Number: {}", 42) == "Number: 42");
    assert(compat::format("String: {}, Number: {}", "test", 123) == "String: test, Number: 123");
    assert(compat::format("{} {} {}", "one", "two", "three") == "one two three");
    assert(compat::format("Bool: {} and {}", true, false) == "Bool: true and false");
    
    const char* c_str = "C-string";
    assert(compat::format("Const char*: {}", c_str) == "Const char*: C-string");

    std::string std_str = "std::string";
    assert(compat::format("Std::string: {}", std_str) == "Std::string: std::string");

    assert(compat::format("Escaped {{}} braces: {{}}") == "Escaped {} braces: {}");
    assert(compat::format("{{{}}}", 42) == "{42}"); 
    assert(compat::format("Hello {{}} {}", "World") == "Hello {} World");

    // Positional arguments
    assert(compat::format("{1}, {0}", "zero", "one") == "one, zero");
    assert(compat::format("Number: {1}, String: {0}", "text", 99) == "Number: 99, String: text");
    assert(compat::format("{0} {1} {0}", "A", "B") == "A B A");
    
    // Mixed auto and positional
    // My current implementation follows: {} uses current_arg_index then increments. {N} uses N and does not affect current_arg_index.
    // Example: "{1} then {} then {0}", args ("A", "B", "C_auto")
    // {1} -> "B" (arg_tuple[1])
    // {} -> "A" (current_arg_index is 0, uses arg_tuple[0]), current_arg_index becomes 1.
    // {0} -> "A" (arg_tuple[0])
    // Expected: "B then A then A"
    // Let's test this specific interpretation. std::format would disallow mixing.
    // The requirement says "Positional parameter, replaces in order" for {} and "{0}, {1}" for explicit.
    // It doesn't explicitly forbid mixing for this custom version.
    // My current_arg_index logic for {} is: use current_arg_index, then increment.
    // For "{1} then {} then {0}" with ("A", "B", "C_auto"):
    // 1. Placeholder "{1}": arg_to_format_idx = 1. Uses "B". current_arg_index remains 0.
    // 2. Placeholder "{}": arg_to_format_idx = current_arg_index (0). Uses "A". current_arg_index becomes 1.
    // 3. Placeholder "{0}": arg_to_format_idx = 0. Uses "A". current_arg_index remains 1.
    // Output: "B then A then A"
    assert(compat::format("{1} then {} then {0}", "A", "B") == "B then A then A");


    assert(compat::format("Zero args: ") == "Zero args: ");
    // Note: std::to_string for double usually gives 6 decimal places.
    assert(compat::format("Five args: {} {} {} {} {}", 1, "two", 3.0, false, "end") == "Five args: 1 two 3.000000 false end"); 

    std::cout << "compat::format tests passed." << std::endl;
}

void test_print_println() {
    std::cout << "--- Testing compat::print and compat::println ---" << std::endl;

    {
        cout_redirector redir;
        compat::print("Hello, {}!", "print");
        assert(redir.get_output() == "Hello, print!");
    }
    {
        cout_redirector redir;
        compat::println("Hello, {}!", "println");
        assert(redir.get_output() == "Hello, println!\n");
    }
    {
        cout_redirector redir;
        compat::print("Numbers: {0}, {1}, {2}", 10, 20, 30);
        assert(redir.get_output() == "Numbers: 10, 20, 30");
    }
    {
        cout_redirector redir;
        compat::println("Escaped {{}} and arg: {}", "val");
        assert(redir.get_output() == "Escaped {} and arg: val\n");
    }

    std::cout << "compat::print and compat::println tests passed." << std::endl;
}

void test_format_to() {
    std::cout << "--- Testing compat::format_to ---" << std::endl;
    std::string s;
    compat::format_to(std::back_inserter(s), "Format to string: {}, {}", "data", 123);
    assert(s == "Format to string: data, 123");

    std::vector<char> vec;
    compat::format_to(std::back_inserter(vec), "To vector: {}", "vec_test");
    std::string vec_str(vec.begin(), vec.end());
    assert(vec_str == "To vector: vec_test");
    
    std::cout << "compat::format_to tests passed." << std::endl;
}

void test_errors() {
    std::cout << "--- Testing Error Conditions ---" << std::endl;
    bool exception_thrown = false;

    // Unmatched '{'
    try {
        compat::format("Hello {", "world");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        assert(msg.find("Unmatched '{'") != std::string::npos);
        exception_thrown = true;
    }
    assert(exception_thrown);
    exception_thrown = false;

    // Unmatched '}'
    try {
        compat::format("Hello }", "world");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        assert(msg.find("Unmatched '}'") != std::string::npos);
        exception_thrown = true;
    }
    assert(exception_thrown);
    exception_thrown = false;

    // Argument index out of bounds (too few args for auto-index)
    try {
        compat::format("Hello {} {}", "world"); // Needs 2 args, gets 1
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        assert(msg.find("Argument index out of bounds") != std::string::npos);
        exception_thrown = true;
    }
    assert(exception_thrown);
    exception_thrown = false;

    // Argument index out of bounds (explicit index too high)
    try {
        compat::format("Hello {1}", "world"); // Needs arg at index 1, only has index 0
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        assert(msg.find("Argument index out of bounds") != std::string::npos);
        exception_thrown = true;
    }
    assert(exception_thrown);
    exception_thrown = false;
    
    // Argument index out of bounds (no arguments provided but placeholder exists)
    try {
        compat::format("Hello {}"); 
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        assert(msg.find("Argument index out of bounds") != std::string::npos);
        assert(msg.find("no arguments were provided") != std::string::npos);
        exception_thrown = true;
    }
    assert(exception_thrown);
    exception_thrown = false;

    // Invalid placeholder content (not empty, not an index)
    try {
        compat::format("Hello {abc}", "world");
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        assert(msg.find("Invalid format placeholder content") != std::string::npos);
        exception_thrown = true;
    }
    assert(exception_thrown);
    exception_thrown = false;

    std::cout << "Error condition tests passed (expected exceptions were caught)." << std::endl;
}


int main() {
    test_format();
    std::cout << std::endl;
    test_print_println();
    std::cout << std::endl;
    test_format_to();
    std::cout << std::endl;
    test_errors();
    std::cout << std::endl;

    std::cout << "All basic tests completed." << std::endl;

    // Example from requirement.md
    std::cout << "\n--- Example from requirement.md ---" << std::endl;
    int x = 42;
    std::string name = "Vivek";

    compat::print("Hello {}, value = {}", name, x); 
    std::cout << std::endl; 
    
    compat::println("Hello {}, value = {}", name, x); 

    compat::print("Value: {1}, Name: {0}\n", name, x); 

    return 0;
}
