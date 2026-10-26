#include "rv/core.hpp"

namespace rv {
std::vector<int> target_frequency_order(const Circuit &a, const Circuit *b, Budget *budget) {
    require(a.d >= 2 && a.n >= 1 && a.n <= 64, "invalid circuit dimensions");
    if (b)
        require(a.d == b->d && a.n == b->n, "circuit dimensions differ");
    std::vector<Count> frequency(a.n, 0);
    auto count = [&](const Circuit &c) {
        if (budget)
            budget->tick(c.gates.size());
        for (const auto &g : c.gates) {
            if (budget)
                budget->tick(c.gates.size());
            require(g.target >= 0 && g.target < c.n, "invalid target in wire ordering");
            frequency[g.target] = checked_add(frequency[g.target], 1);
        }
    };
    count(a);
    if (b)
        count(*b);
    std::vector<int> ranked(a.n), order(a.n);
    std::iota(ranked.begin(), ranked.end(), 0);
    // An inverted pair with frequencies f < g at indices i < j costs
    // (g-f)*(j-i) more. Descending frequencies therefore minimize the sum.
    // Starting from input order and using a stable sort fixes all ties.
    std::stable_sort(ranked.begin(), ranked.end(), [&](int x, int y) {
        return frequency[x] > frequency[y];
    });
    for (int index = 0; index < a.n; ++index)
        order[ranked[index]] = index;
    return order;
}

Circuit permute_wires(const Circuit &c, const std::vector<int> &order, Budget *budget) {
    require(c.d >= 2 && c.n >= 1 && c.n <= 64, "invalid circuit dimensions");
    require(order.size() == std::size_t(c.n), "wire permutation dimension mismatch");
    std::vector<bool> seen(c.n, false);
    for (int target : order) {
        require(target >= 0 && target < c.n && !seen[target], "invalid wire permutation");
        seen[target] = true;
    }
    if (budget)
        budget->tick(c.gates.size());
    Circuit out;
    out.d = c.d;
    out.n = c.n;
    out.metadata = c.metadata;
    out.gates.reserve(c.gates.size());
    for (const auto &g : c.gates) {
        if (budget)
            budget->tick(c.gates.size());
        validate(g, c.d, c.n);
        auto mapped = gate(c.n, order[g.target], g.lower);
        for (int k = 0; k < c.n; ++k)
            mapped.controls[order[k]] = g.controls[k];
        out.gates.push_back(std::move(mapped));
    }
    return out;
}
} // namespace rv
