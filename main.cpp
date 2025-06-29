#include "compat_format.hpp"
#include <iostream> // For std::cout, std::endl (though not strictly needed for println)
#include <string>   // For std::string

int main() {
    std::cout << "--- Demonstrating compat::print and compat::println ---" << std::endl;

    // Basic usage
    compat::println("Hello, World!");
    compat::print("This is a simple print statement without a newline.");
    compat::println(" This is on the same line, then a newline.");

    // Using arguments
    std::string name = "compat::format";
    int version = 1;
    compat::println("Welcome to {} version {}!", name, version);

    // Positional arguments
    compat::println("Positional: {1} comes before {0}.", "first", "Second");

    // Escaped braces
    compat::println("To print braces, use {{ and }}: e.g., {{example}}");

    // Demonstrating some basic format specifiers
    double value = 123.456789;
    compat::println("Value with default float formatting: {}", value); // This line was okay
    // Corrected lines:
    compat::println("Value with spec ':.2f': {:.2f}", value);
    compat::println("Value with spec ':10.2f': {:10.2f}", value);
    compat::println("Value with spec ':*<10.2f': {:*<10.2f}", value);
    
    std::string text = "text";
    compat::println("String '{}' with spec ':*>10': '{:*>10}'", text, text);
    compat::println("String '{}' with spec ':^10': '{:^10}'", text, text);


    // Example from original requirement.md (if any specific one was there)
    // For instance:
    int x = 42;
    std::string user_name = "Vivek";

    compat::print("User: {}, Value: {}", user_name, x);
    std::cout << std::endl; // Manual newline after print

    compat::println("User: {}, Value: {} (with println)", user_name, x);

    compat::println("Value: {1}, User: {0} (positional with println)", user_name, x);

    compat::println("--- End of demonstration ---");

    return 0;
}
