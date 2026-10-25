#include "rv/compiler.hpp"
namespace rv {
P Compiler::fxl_route(const Gate &g) {
    return memo(key("fxl", {g}), [&] {
        Builder b(budget, {g});
        for (;;) {
            bool changed = false;
            Word previous = std::move(b.word);
            b.word.clear();
            b.word.reserve(previous.size());
            std::size_t logical_size = previous.size();
            for (const auto &h : previous) {
                budget.tick(logical_size);
                if (indices.count(h)) {
                    b.word.push_back(h);
                    continue;
                }
                changed = true;
                P p;
                if (h.full()) {
                    auto representative = h;
                    for (int k = 0; k < h.target; ++k)
                        representative.controls[k] = (k == h.target - 1 ? 1 : 0);
                    auto tr = transport(h, representative);
                    Builder local(budget, {h});
                    local.call(tr);
                    sort_x(local, (local.word.size() + 1) / 2, local.word.size());
                    p = local.finish();
                } else { // Algorithm 1's Gray optimization: refine, then explicit swaps.
                    Builder local(budget, {h});
                    auto ex = expand(h);
                    local.call(ex);
                    std::vector<int> free;
                    for (int k = 0; k < n; ++k)
                        if (k != h.target && h.controls[k] < 0)
                            free.push_back(k);
                    auto values = gray(2, int(free.size()));
                    std::size_t at = 0;
                    for (const auto &tuple : values) {
                        auto wanted = h;
                        for (std::size_t k = 0; k < free.size(); ++k)
                            wanted.controls[free[k]] = tuple[k];
                        auto it = std::find(local.word.begin() + at, local.word.end(), wanted);
                        require(it != local.word.end(), "Gray factor missing");
                        std::size_t found = it - local.word.begin();
                        while (found > at) {
                            commute(local, found - 1);
                            --found;
                        }
                        ++at;
                    }
                    p = local.finish();
                }
                require(p->before == Word{h}, "FXL round template input");
                // Algorithm 1 explicitly builds C' from left to right. Do
                // exactly that instead of repeatedly inserting into C.
                const auto context = logical_size - 1;
                require(p->peak <= budget.limits.max_word - context,
                        "resource_limit: contextual peak word");
                logical_size = context + p->after.size();
                budget.tick(logical_size, true);
                b.peak = std::max(b.peak, context + p->peak);
                b.rules = checked_add(b.rules, p->rules);
                b.word.insert(b.word.end(), p->after.begin(), p->after.end());
            }
            if (!changed)
                break;
            reduce_aux(b);
        }
        return b.finish();
    });
}
} // namespace rv
