#include "rv/compiler.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
using namespace rv;
namespace {
unsigned simulate(const Word &w, unsigned x) {
    for (const auto &g : w) {
        bool enabled = true;
        for (int k = 0; k < int(g.controls.size()); ++k)
            if (g.controls[k] >= 0 && int((x >> k) & 1) != g.controls[k])
                enabled = false;
        if (enabled) x ^= 1u << g.target;
    }
    return x;
}
void check(const Circuit &c, Method method, unsigned id) {
    Limits limits; limits.seconds = 60; limits.max_work = 200000000;
    Budget budget(limits); Compiler compiler(2, c.n, method, budget);
    auto routed = compiler.route(c.gates);
    auto normal = compiler.normalize_path(routed->after);
    for (unsigned x = 0; x < (1u << c.n); ++x) {
        require(simulate(c.gates, x) == simulate(routed->after, x), "route semantics");
        require(simulate(c.gates, x) == simulate(normal->after, x), "normal semantics");
    }
    // The independent truth-table insertion construction is only an oracle;
    // it is not used by FL/FXL or inside benchmark timing.
    require(normal->after == compiler.insertion_normal_form(c.gates), "canonical endpoint");
    std::uint64_t digest = 1469598103934665603ull;
    for (const auto &g : normal->after) {
        digest = (digest ^ unsigned(g.target + 1)) * 1099511628211ull;
        for (int v : g.controls) digest = (digest ^ unsigned(v + 2)) * 1099511628211ull;
    }
    std::cout << id << ',' << c.n << ',' << method_name(method) << ','
              << checked_add(routed->rules, normal->rules) << ',' << digest << '\n';
}
}
int main(int argc, char **) {
    try {
        unsigned id = 0;
        std::mt19937 rng(20261022);
        for (int n = 1; n <= 4; ++n) {
            std::vector<Gate> alphabet;
            int combinations = 1;
            for (int k = 1; k < n; ++k) combinations *= 3;
            for (int target = 0; target < n; ++target)
                for (int mask = 0; mask < combinations; ++mask) {
                    auto g = gate(n, target); int rest = mask;
                    for (int k = 0; k < n; ++k) if (k != target) {
                        g.controls[k] = rest % 3 - 1; rest /= 3;
                    }
                    alphabet.push_back(g);
                }
            auto run = [&](Word w) {
                Circuit c; c.n = n; c.gates = std::move(w);
                for (auto m : {Method::FL2025, Method::FXL2026}) check(c, m, id);
                ++id;
            };
            run({});
            for (const auto &g : alphabet) run({g});
            for (int rep = 0; rep < 24; ++rep) {
                Word w;
                for (int k = 0; k < 12; ++k) w.push_back(alphabet[rng() % alphabet.size()]);
                run(w);
            }
        }
        if (argc > 1) {
            for (int n = 5; n <= 7; ++n) {
                for (int rep = 0; rep < 4; ++rep) {
                    Circuit c; c.n = n;
                    for (int k = 0; k < 4; ++k) {
                        // Low-target full gates keep this correctness suite
                        // bounded; high-target expansion is covered by the
                        // separately budgeted public-circuit benchmarks.
                        auto g = gate(n, rng() % 3);
                        for (int j = 0; j < n; ++j)
                            if (j != g.target) g.controls[j] = rng() % 2;
                        c.gates.push_back(g);
                    }
                    for (auto m : {Method::FL2025, Method::FXL2026}) check(c, m, id);
                    ++id;
                }
            }
            // Exercise ordered bitset queries across 64- and 4096-bit levels.
            for (int length : {65, 4097, 8200}) {
                Circuit c; c.n = 4;
                auto g = gate(4, 0);
                for (int j = 1; j < 4; ++j) g.controls[j] = 0;
                c.gates.assign(length, g);
                for (auto m : {Method::FL2025, Method::FXL2026}) check(c, m, id);
                ++id;
            }
        }
        std::cerr << "Verified " << id << " circuits with both methods, full truth tables and canonical endpoints.\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
