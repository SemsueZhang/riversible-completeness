#include "rv/core.hpp"
#include <limits>
namespace rv {
Word reversed(Word w) {
    std::reverse(w.begin(), w.end());
    return w;
}
Word slice(const Word &w, std::size_t a, std::size_t b) {
    require(a <= b && b <= w.size(), "invalid word slice");
    return Word(w.begin() + a, w.begin() + b);
}
Word joined(std::initializer_list<Word> ws) {
    Word r;
    for (const auto &w : ws)
        r.insert(r.end(), w.begin(), w.end());
    return r;
}
Gate gate(int n, int t, int lower) {
    return Gate{t, lower, std::vector<int>(n, -1)};
}
void require(bool c, const std::string &m) {
    if (!c)
        throw std::runtime_error(m);
}
void validate(const Gate &g, int d, int n) {
    require(d >= 2 && n >= 1 && int(g.controls.size()) == n, "invalid circuit dimensions");
    require(g.target >= 0 && g.target < n && g.lower >= 0 && g.lower < d - 1,
            "invalid target/level");
    for (int k = 0; k < n; ++k)
        require(g.controls[k] >= -1 && g.controls[k] < d && (k != g.target || g.controls[k] == -1),
                "invalid controls");
}
State apply(const Gate &g, State x) {
    for (std::size_t k = 0; k < x.size(); ++k)
        if (g.controls[k] >= 0 && g.controls[k] != x[k])
            return x;
    if (x[g.target] == g.lower)
        ++x[g.target];
    else if (x[g.target] == g.lower + 1)
        --x[g.target];
    return x;
}
std::vector<State> gray(int d, int n) {
    if (n == 0)
        return {State{}};
    auto prefix = gray(d, n - 1);
    std::vector<State> r;
    for (int j = 0; j < d; ++j)
        for (std::size_t k = 0; k < prefix.size(); ++k) {
            State x = prefix[j % 2 ? prefix.size() - 1 - k : k];
            x.push_back(j);
            r.push_back(std::move(x));
        }
    return r;
}
Gate edge_gate(const State &a, const State &b) {
    require(a.size() == b.size(), "edge dimension");
    Gate g = gate(int(a.size()), 0);
    int t = -1;
    for (int k = 0; k < int(a.size()); ++k)
        if (a[k] != b[k]) {
            require(t < 0 && std::abs(a[k] - b[k]) == 1, "not a grid edge");
            t = k;
        }
    require(t >= 0, "degenerate edge");
    g.target = t;
    g.lower = std::min(a[t], b[t]);
    g.controls = a;
    g.controls[t] = -1;
    return g;
}
Count checked_add(Count a, Count b) {
    require(b <= std::numeric_limits<Count>::max() - a, "rule count overflow");
    return a + b;
}
void Budget::tick(std::size_t w, bool construction) {
    require(w <= limits.max_word, "resource_limit: max_word");
    if (construction)
        require(++work <= limits.max_work, "resource_limit: max_work");
    if (limits.seconds > 0 &&
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() >
            limits.seconds)
        throw std::runtime_error("resource_limit: seconds");
}
Count RewriteSummary::total() const {
    return rules;
}
Builder::Builder(Budget &b, Word w) : budget(b), before(w), word(std::move(w)), peak(word.size()) {
    budget.tick(word.size());
}
void Builder::call(P p, std::size_t pos, bool rev) {
    const auto &a = rev ? p->after : p->before;
    const auto &z = rev ? p->before : p->after;
    require(pos <= word.size() && a.size() <= word.size() - pos &&
                std::equal(a.begin(), a.end(), word.begin() + pos),
            "rewrite does not match active word");
    std::size_t new_size = word.size() - a.size() + z.size();
    budget.tick(new_size, true);
    require(p->peak <= budget.limits.max_word - (word.size() - a.size()),
            "resource_limit: contextual peak word");
    peak = std::max(peak, word.size() - a.size() + p->peak);
    rules = checked_add(rules, p->rules);
    // Equal-length rewrites (in particular swaps and braids) must not move
    // the suffix. For a length change, move it only once.
    const auto common = std::min(a.size(), z.size());
    std::copy_n(z.begin(), common, word.begin() + pos);
    if (a.size() > z.size())
        word.erase(word.begin() + pos + z.size(), word.begin() + pos + a.size());
    else if (z.size() > a.size())
        word.insert(word.begin() + pos + a.size(), z.begin() + common, z.end());
}
void Builder::edit(int rule, std::size_t pos, Word a, Word b) {
    require(rule > 0 && rule < 12, "unknown basic rule");
    budget.tick(std::max(a.size(), b.size()), true);
    require(pos <= word.size() && a.size() <= word.size() - pos &&
                std::equal(a.begin(), a.end(), word.begin() + pos),
            "rewrite does not match active word");
    auto size = word.size() - a.size() + b.size();
    budget.tick(size, true);
    peak = std::max(peak, size);
    rules = checked_add(rules, 1);
    const auto common = std::min(a.size(), b.size());
    std::copy_n(b.begin(), common, word.begin() + pos);
    if (a.size() > b.size())
        word.erase(word.begin() + pos + b.size(), word.begin() + pos + a.size());
    else if (b.size() > a.size())
        word.insert(word.begin() + pos + a.size(), b.begin() + common, b.end());
}
P Builder::finish() {
    budget.tick(word.size(), true);
    auto p = std::make_shared<RewriteSummary>();
    p->before = std::move(before);
    p->after = std::move(word);
    p->rules = rules;
    p->peak = peak;
    return p;
}
P reverse_proof(Budget &b, P p) {
    Builder out(b, p->after);
    out.call(p, 0, true);
    return out.finish();
}
std::string method_name(Method m) {
    switch (m) {
    case Method::FL2025:
        return "fl2025";
    case Method::FXL2026:
        return "fxl2026";
    case Method::NF:
        return "nf";
    case Method::DC:
        return "dc";
    }
    throw std::runtime_error("bad method");
}
Method parse_method(const std::string &s) {
    if (s == "fl2025")
        return Method::FL2025;
    if (s == "fxl2026")
        return Method::FXL2026;
    if (s == "nf")
        return Method::NF;
    if (s == "dc")
        return Method::DC;
    throw std::runtime_error("method must be fl2025, fxl2026, nf, or dc");
}
} // namespace rv
