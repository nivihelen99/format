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

    compat::println("\n--- New Integer Format Specifiers ---");
    int num = 42;
    compat::println("Decimal: {:d}", num);
    compat::println("Binary: {:b}", num);
    compat::println("Binary (#): {:#b}", num);
    compat::println("Binary (#B): {:#B}", num);
    compat::println("Octal: {:o}", num);
    compat::println("Octal (#): {:#o}", num);
    compat::println("Hex (lower): {:x}", num);
    compat::println("Hex (lower, #): {:#x}", num);
    compat::println("Hex (upper): {:X}", num);
    compat::println("Hex (upper, #): {:#X}", num);

    compat::println("Number -10 in binary: {:b}", -10);
    compat::println("Number -10 in hex (#): {:#x}", -10);

    compat::println("Zero in binary (#): {:#b}", 0);
    compat::println("Zero in octal (#): {:#o}", 0);
    compat::println("Zero in hex (#): {:#x}", 0);

    compat::println("Padded hex: {:#08x}", num);
    compat::println("Padded binary: {:012b}", num);
    compat::println("Left-aligned hex: {:#<8x}", num);


    compat::println("\n--- User-Defined Type Formatting (Point) ---");
    compat::internal::Point pt{7, -3}; // Assuming Point is in compat::internal for this example
    compat::println("Default Point: {}", pt);
    compat::println("Padded Point: {:*>15}", pt);
    compat::println("Centered Point: {:^15}", pt);


    compat::println("\n--- End of demonstration ---");

    return 0;
}
