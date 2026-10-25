#include "rv/compiler.hpp"
#include <iostream>
int main() {
    try {
        for (auto method : {rv::Method::FL2025, rv::Method::FXL2026}) {
            rv::Limits limits; limits.seconds = 30;
            rv::Budget budget(limits);
            rv::Compiler compiler(2, 11, method, budget);
            // 2047 symbols * 150000 positions would exceed the dense-index
            // threshold; the occurrence index must use its sparse fallback.
            rv::Word input(150000, compiler.path_gates[0]);
            auto result = compiler.normalize_path(input);
            rv::require(result->after.empty(), "sparse path endpoint");
            rv::require(result->rules == 75000, "sparse path cancellation count");
        }
        std::cout << "Sparse occurrence-index fallback: both methods passed.\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
