#include "rv/compiler.hpp"
#include <iostream>
#include <random>

using namespace rv;
namespace {
State mapped(const State &x, const std::vector<int> &order) {
    State result(x.size());
    for (std::size_t k = 0; k < x.size(); ++k)
        result[order[k]] = x[k];
    return result;
}
State execute(const Word &word, State x) {
    for (const auto &g : word)
        x = apply(g, std::move(x));
    return x;
}
void semantics(const Circuit &c, const std::vector<int> &order) {
    auto p = permute_wires(c, order);
    std::vector<int> inverse(c.n);
    for (int k = 0; k < c.n; ++k)
        inverse[order[k]] = k;
    require(permute_wires(p, inverse).gates == c.gates, "inverse wire map");
    require(p.gates.size() == c.gates.size(), "wire map changed gate count");
    for (const auto &x : gray(c.d, c.n))
        require(execute(p.gates, mapped(x, order)) == mapped(execute(c.gates, x), order),
                "wire permutation semantics");
}
template <class F> void rejects(F f) {
    bool caught = false;
    try {
        f();
    } catch (const std::runtime_error &) {
        caught = true;
    }
    require(caught, "invalid wire-order input accepted");
}
Count score(const Circuit &c, const std::vector<int> &order) {
    Count total = 0;
    for (const auto &g : c.gates)
        total += order[g.target];
    return total;
}
Circuit with_frequencies(const std::vector<int> &frequency) {
    Circuit c{2, int(frequency.size()), {}, ""};
    for (int k = 0; k < c.n; ++k)
        for (int j = 0; j < frequency[k]; ++j)
            c.gates.push_back(gate(c.n, k));
    return c;
}
void optimality(const Circuit &a, const Circuit *b = nullptr) {
    const auto order = target_frequency_order(a, b);
    require(order == target_frequency_order(a, b), "nondeterministic ordering");
    auto sorted = order;
    std::sort(sorted.begin(), sorted.end());
    for (int k = 0; k < a.n; ++k)
        require(sorted[k] == k, "ordering is not bijective");
    auto cost = [&](const std::vector<int> &p) {
        return score(a, p) + (b ? score(*b, p) : 0);
    };
    const auto best = cost(order);
    std::vector<int> candidate(a.n);
    std::iota(candidate.begin(), candidate.end(), 0);
    do {
        require(best <= cost(candidate), "target sum is not globally minimal");
    } while (std::next_permutation(candidate.begin(), candidate.end()));
    // Independently check stable ties using the original input's target counts.
    for (int x = 0; x < a.n; ++x)
        for (int y = x + 1; y < a.n; ++y) {
            auto frequency = [&](int t) {
                auto count = [&](const Circuit &c) {
                    return std::count_if(c.gates.begin(), c.gates.end(),
                                         [&](const Gate &g) { return g.target == t; });
                };
                return count(a) + (b ? count(*b) : 0);
            };
            if (frequency(x) == frequency(y))
                require(order[x] < order[y], "equal-frequency wires changed relative order");
        }
}
} // namespace

int main() {
    try {
        std::size_t checked = 0;
        std::size_t objectives = 0;
        for (int n = 1; n <= 5; ++n) {
            int patterns = 1;
            for (int k = 0; k < n; ++k)
                patterns *= 3;
            for (int code = 0; code < patterns; ++code) {
                std::vector<int> frequency(n);
                int rest = code;
                for (auto &f : frequency) {
                    f = rest % 3;
                    rest /= 3;
                }
                optimality(with_frequencies(frequency));
                ++objectives;
            }
        }
        for (const auto &frequency : {std::vector<int>{0, 0, 0, 0, 0, 0},
                                      {6, 5, 4, 3, 2, 1}, {1, 2, 3, 4, 5, 6},
                                      {2, 5, 0, 5, 2, 0}}) {
            optimality(with_frequencies(frequency));
            ++objectives;
        }
        const auto example = with_frequencies({2, 5, 0, 5});
        require(target_frequency_order(example) == std::vector<int>({2, 0, 3, 1}),
                "known target-frequency ordering");
        require(score(example, {0, 1, 2, 3}) == 20 &&
                    score(example, target_frequency_order(example)) == 9,
                "known target-sum minimum");
        const auto other = with_frequencies({7, 0, 3, 1});
        optimality(example, &other);
        ++objectives;
        std::mt19937 rng(20261026);
        for (int d : {2, 3, 4})
            for (int n = 1; n <= 4; ++n) {
                std::vector<Gate> alphabet;
                int combinations = 1;
                for (int k = 1; k < n; ++k)
                    combinations *= d + 1;
                for (int t = 0; t < n; ++t)
                    for (int lower = 0; lower < d - 1; ++lower)
                        for (int code = 0; code < combinations; ++code) {
                            auto g = gate(n, t, lower);
                            int rest = code;
                            for (int k = 0; k < n; ++k)
                                if (k != t) {
                                    g.controls[k] = rest % (d + 1) - 1;
                                    rest /= d + 1;
                                }
                            alphabet.push_back(g);
                        }
                std::vector<int> order(n);
                std::iota(order.begin(), order.end(), 0);
                do {
                    semantics(Circuit{d, n, {}, ""}, order);
                    ++checked;
                    for (const auto &g : alphabet) {
                        semantics(Circuit{d, n, {g}, ""}, order);
                        ++checked;
                    }
                    Circuit c{d, n, {}, ""};
                    for (int j = 0; j < 24; ++j)
                        c.gates.push_back(alphabet[rng() % alphabet.size()]);
                    semantics(c, order);
                    ++checked;
                    semantics(c, target_frequency_order(c));
                    ++checked;
                } while (std::next_permutation(order.begin(), order.end()));
            }
        auto controlled = gate(3, 2);
        controlled.controls[1] = 1;
        Circuit c{2, 3, {gate(3, 0), controlled, gate(3, 2), controlled}, "source"};
        const auto original = c.gates;
        for (const auto &bad : {std::vector<int>{}, {0, 1, 1}, {0, 1, 3}, {0, -1, 2}})
            rejects([&] { permute_wires(c, bad); });
        auto p = permute_wires(c, target_frequency_order(c));
        require(c.gates == original, "source circuit mutated");
        require(p.metadata == c.metadata, "source metadata lost");
        auto invalid = c;
        invalid.gates[0].target = 3;
        rejects([&] { permute_wires(invalid, {0, 1, 2}); });
        rejects([&] { target_frequency_order(invalid); });
        invalid = c;
        invalid.gates[0].controls[0] = 1;
        rejects([&] { permute_wires(invalid, {0, 1, 2}); });
        for (const auto &bad : {Circuit{2, 0, {}, ""}, Circuit{2, 65, {}, ""},
                                Circuit{1, 3, {}, ""}}) {
            rejects([&] { target_frequency_order(bad); });
            rejects([&] { permute_wires(bad, {}); });
            rejects([&] { target_frequency_order(c, &bad); });
        }
        const Circuit wrong_d{3, 3, {}, ""};
        rejects([&] { target_frequency_order(c, &wrong_d); });
        for (auto method : {Method::FL2025, Method::FXL2026}) {
            const auto order = target_frequency_order(c);
            auto p = permute_wires(c, order);
            Limits limits;
            limits.max_work = 20000000;
            Budget budget(limits);
            Compiler compiler(2, c.n, method, budget);
            auto route = compiler.route(p.gates);
            auto normal = compiler.normalize_path(route->after);
            for (const auto &x : gray(2, c.n))
                require(execute(normal->after, mapped(x, order)) ==
                            mapped(execute(c.gates, x), order),
                        "ordered normal form semantics");
            require(normal->after == compiler.insertion_normal_form(p.gates),
                    "ordered canonical endpoint");
        }
        for (auto method : {Method::FL2025, Method::FXL2026, Method::NF, Method::DC}) {
            auto controlled = gate(2, 1);
            controlled.controls[0] = 1;
            Circuit a{2, 2, {gate(2, 0), controlled}, ""}, b = a;
            b.gates.push_back(gate(2, 1));
            b.gates.push_back(gate(2, 1));
            require(target_frequency_order(a) != target_frequency_order(b),
                    "paired test must need a shared map");
            const auto order = target_frequency_order(a, &b);
            require(order == std::vector<int>({1, 0}), "combined target frequencies");
            auto pa = permute_wires(a, order), pb = permute_wires(b, order);
            Budget yes;
            require(equivalent(pa, pb, method, yes).equivalent, "ordered equivalent pair");
            b.gates.push_back(gate(2, 1));
            const auto negative_order = target_frequency_order(a, &b);
            pa = permute_wires(a, negative_order);
            pb = permute_wires(b, negative_order);
            Budget no;
            require(!equivalent(pa, pb, method, no).equivalent, "ordered unequal pair");
        }
        std::cout << "Verified " << objectives << " globally minimal target-sum cases and "
                  << checked << " wire-map cases with full state enumeration, ordered normal "
                     "forms and paired equivalence.\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
