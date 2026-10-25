#include "rv/compiler.hpp"
#include <limits>
#include <sstream>
namespace rv {
Compiler::Compiler(int radix, int lines, Method m, Budget &b)
    : d(radix), n(lines), method(m), budget(b) {
    require(d >= 2 && n >= 1, "d >= 2 and n >= 1 required");
    require((m != Method::FL2025 && m != Method::FXL2026) || d == 2, "binary-only method");
    std::size_t N = 1;
    for (int k = 0; k < n; ++k) {
        require(std::size_t(d) <= budget.limits.max_states / N, "resource_limit: max_states");
        N *= std::size_t(d);
    }
    require(N <= std::size_t(std::numeric_limits<int>::max()), "resource_limit: index range");
    path = gray(d, n);
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        auto g = edge_gate(path[i], path[i + 1]);
        indices[g] = int(i);
        path_gates.push_back(std::move(g));
    }
    budget.tick();
}
std::string Compiler::key(const std::string &name, const Word &w) const {
    // Fixed-width, unambiguous encoding; compiler instances fix d and n.
    std::string out = name;
    out.push_back('\0');
    out.reserve(out.size() + w.size() * (3 + 2 * n));
    auto put16 = [&](int v) {
        out.push_back(char(v & 255));
        out.push_back(char((v >> 8) & 255));
    };
    for (const auto &g : w) {
        out.push_back(char(g.target));
        put16(g.lower);
        for (int v : g.controls)
            put16(v + 1);
    }
    return out;
}
P Compiler::memo(const std::string &k, const std::function<P()> &f) {
    auto it = cache.find(k);
    if (it != cache.end())
        return it->second;
    auto p = f();
    // Keep small reusable endpoint summaries only, with a bounded total.
    // Large routed circuits remain local to their caller, never in history.
    const auto size = p->before.size() + p->after.size();
    const auto cap = std::min<std::size_t>(budget.limits.max_word, 65536);
    if (size <= 4096 && cached_gates <= cap && size <= cap - cached_gates) {
        cache.emplace(k, p);
        cached_gates += size;
    }
    return p;
}
Word Compiler::pword(const Gate &g) const {
    Word w;
    for (int k = 0; k < n; ++k)
        if (g.controls[k] >= 0)
            for (int j = g.controls[k]; j < d - 1; ++j)
                w.push_back(gate(n, k, j));
    return w;
}
Word Compiler::norm8(const Gate &g) const {
    auto p = pword(g);
    auto top = g;
    for (auto &v : top.controls)
        if (v >= 0)
            v = d - 1;
    return joined({p, {top}, reversed(p)});
}
void Compiler::cancel(Builder &b, std::size_t pos) {
    require(pos + 1 < b.word.size() && b.word[pos] == b.word[pos + 1], "cancellation mismatch");
    b.edit(1, pos, slice(b.word, pos, pos + 2), {});
}
void Compiler::insert_pairs(Builder &b, std::size_t pos, const Word &w) {
    for (std::size_t k = 0; k < w.size(); ++k) {
        auto g = w[w.size() - 1 - k];
        b.edit(1, pos + k, {}, {g, g});
    }
}
void Compiler::commute(Builder &b, std::size_t pos) {
    auto x = b.word.at(pos), y = b.word.at(pos + 1);
    int rule = 0;
    for (int k = 0; k < n; ++k)
        if (x.controls[k] >= 0 && y.controls[k] >= 0 && x.controls[k] != y.controls[k])
            rule = 3;
    if (!rule && x.target == y.target && std::abs(x.lower - y.lower) >= 2)
        rule = 4;
    if (!rule && x.target != y.target) {
        int u = x.controls[y.target], v = y.controls[x.target];
        if ((u >= 0 && u != y.lower && u != y.lower + 1) ||
            (v >= 0 && v != x.lower && v != x.lower + 1))
            rule = 5;
    }
    if (rule) {
        require(method == Method::NF || method == Method::DC || rule == 3, "nonbinary commutation");
        // The same one basic rule as edit(), with no temporary gate arrays.
        budget.tick(2, true);
        budget.tick(b.word.size(), true);
        b.rules = checked_add(b.rules, 1);
        std::swap(b.word[pos], b.word[pos + 1]);
    } else if (x.unconditional() && y.unconditional() && x.target != y.target &&
               method != Method::DC)
        b.call(unconditional_commute(x, y), pos);
    else
        throw std::runtime_error("no legal commutation");
}
P Compiler::expand(const Gate &g, std::vector<int> lines) {
    if (lines.empty())
        for (int k = 0; k < n; ++k)
            if (k != g.target && g.controls[k] < 0)
                lines.push_back(k);
    Builder b(budget, {g});
    for (int line : lines) {
        std::size_t pos = 0;
        while (pos < b.word.size()) {
            auto h = b.word[pos];
            if (h.controls[line] < 0) {
                Word kids;
                for (int a = 0; a < d; ++a) {
                    auto child = h;
                    child.controls[line] = a;
                    kids.push_back(child);
                }
                b.edit(2, pos, {h}, kids);
                pos += d;
            } else
                ++pos;
        }
    }
    return b.finish();
}
P Compiler::gray_expand(const Gate &g) {
    std::vector<int> lines;
    for (int k = n - 1; k >= 0; --k)
        if (k != g.target && g.controls[k] < 0)
            lines.push_back(k);
    std::function<P(Gate, std::size_t, bool)> visit = [&](Gate h, std::size_t depth,
                                                          bool back) -> P {
        Builder b(budget, {h});
        if (depth == lines.size())
            return b.finish();
        int line = lines[depth];
        Word kids;
        for (int a = 0; a < d; ++a) {
            auto c = h;
            c.controls[line] = a;
            kids.push_back(c);
        }
        b.edit(2, 0, {h}, kids);
        if (back) {
            for (int end = d - 1; end > 0; --end)
                for (int p = 0; p < end; ++p)
                    commute(b, p);
            std::reverse(kids.begin(), kids.end());
        }
        std::size_t pos = 0;
        for (auto child : kids) {
            auto p = visit(child, depth + 1, back ^ (child.controls[line] % 2 != 0));
            b.call(p, pos);
            pos += p->after.size();
        }
        return b.finish();
    };
    return visit(g, 0, false);
}
P Compiler::controlled_flip(const Gate &u, const Gate &g, const Gate &h) {
    require(d == 2 && method == Method::FL2025, "FL controlled flip only");
    return memo(key("cflip", {u, g, h}), [&] {
        auto a0 = u, a1 = u;
        a0.controls[g.target] = 0;
        a1.controls[g.target] = 1;
        Word fibers{a0, a1};
        Builder b(budget, {u, g, u});
        b.edit(2, 0, {u}, fibers);
        b.edit(2, 3, {u}, fibers);
        if (g.controls[u.target] == 1) {
            commute(b, 3);
            b.edit(5, 0, {a0, a1, g, a1, a0}, {h});
        } else {
            b.edit(5, 2, {g}, {a0, a1, h, a1, a0});
            commute(b, 1);
            cancel(b, 0);
            cancel(b, 0);
            cancel(b, 2);
            cancel(b, 1);
        }
        require(b.word == Word{h}, "FL flip endpoint");
        return b.finish();
    });
}
void Compiler::remove_fibers(Builder &b, std::size_t start, std::size_t count, const Gate &match) {
    auto fibers = slice(b.word, start, start + count);
    for (auto &g : fibers) {
        if (g == match)
            continue;
        auto lit = std::find(b.word.begin() + start, b.word.end(), g);
        require(lit != b.word.end(), "fiber missing");
        std::size_t l = lit - b.word.begin();
        auto rit = std::find(lit + 1, b.word.end(), g);
        require(rit != b.word.end(), "paired fiber missing");
        std::size_t r = rit - b.word.begin();
        // The original schedule moves this fiber right by Rule 3 and then
        // cancels its partner. Check those SAME crossings, add their exact
        // count, and delete the endpoints without materializing each swap.
        bool all_rule3 = true;
        for (std::size_t at = l + 1; at < r && all_rule3; ++at) {
            bool opposite = false;
            for (int k = 0; k < n; ++k)
                opposite = opposite || (g.controls[k] >= 0 &&
                    b.word[at].controls[k] >= 0 && g.controls[k] != b.word[at].controls[k]);
            all_rule3 = opposite;
            budget.tick();
        }
        if (all_rule3) {
            budget.tick(b.word.size(), true);
            b.rules = checked_add(b.rules, Count(r - l));
            b.word.erase(b.word.begin() + r);
            b.word.erase(b.word.begin() + l);
            continue;
        }
        while (l + 1 < r) {
            commute(b, l);
            ++l;
        }
        cancel(b, l);
    }
}
P Compiler::unconditional_flip(const Gate &g, int p) {
    return memo(key("unconditional-flip", {g, gate(n, p)}), [&] {
    auto u = gate(n, p);
    auto h = g;
    h.controls[p] = 1 - h.controls[p];
    Builder b(budget, {u, g, u});
    std::vector<int> external;
    auto match = u;
    for (int k = 0; k < n; ++k)
        if (k != p && g.controls[k] >= 0) {
            external.push_back(k);
            match.controls[k] = g.controls[k];
        }
    if (!external.empty()) {
        auto ex = expand(u, external);
        b.call(ex);
        b.call(ex, ex->after.size() + 1);
        remove_fibers(b, 0, ex->after.size(), match);
    }
    b.call(controlled_flip(match, g, h));
    return b.finish();
    });
}
P Compiler::rule8(const Gate &g) {
    return memo(key("r8", {g}), [&] {
        Builder b(budget, {g});
        if (method == Method::NF)
            b.edit(8, 0, {g}, norm8(g));
        else if (method == Method::FXL2026) {
            auto p = pword(g);
            auto top = g;
            for (auto &v : top.controls)
                if (v >= 0)
                    v = 1;
            b.edit(5, 0, {g}, joined({p, {top}, p}));
            std::size_t start = p.size() + 1;
            for (std::size_t end = p.size(); end > 1; --end)
                for (std::size_t j = 0; j + 1 < end; ++j)
                    commute(b, start + j);
        } else if (method == Method::FL2025) {
            std::size_t depth = 0;
            for (int p = 0; p < n; ++p)
                if (g.controls[p] == 0) {
                    auto cur = b.word[depth], positive = cur;
                    positive.controls[p] = 1;
                    b.call(unconditional_flip(positive, p), depth, true);
                    ++depth;
                }
        } else
            throw std::runtime_error("R8 unavailable in DC");
        require(b.word == norm8(g), "R8 endpoint");
        return b.finish();
    });
}
void Compiler::sort_x(Builder &b, std::size_t start, std::size_t end) {
    for (std::size_t stop = end; stop > start + 1; --stop)
        for (std::size_t at = start; at + 1 < stop; ++at)
            if (b.word[at].target > b.word[at + 1].target)
                commute(b, at);
}
P Compiler::transport(const Gate &g, const Gate &h) {
    return memo(key("transport", {g, h}), [&] {
        Builder b(budget, {g});
        if (g == h)
            return b.finish();
        auto pa = pword(g), pb = pword(h);
        b.call(rule8(g));
        std::size_t at = pa.size();
        insert_pairs(b, at, pb);
        if (!pb.empty())
            insert_pairs(b, at + 2 * pb.size() + 1, pb);
        b.call(rule8(h), at + pb.size(), true);
        // Reduce both conjugators independently. Every distinct-line interchange
        // is derived from the applicable basic rules, not charged as a free swap.
        auto reduce = [&](Word w, bool descending) {
            Builder c(budget, w);
            cancel_exposed(c);
            for (std::size_t stop = c.word.size(); stop > 1; --stop)
                for (std::size_t p = 0; p + 1 < stop; ++p)
                    if (descending ? c.word[p].target < c.word[p + 1].target
                                   : c.word[p].target > c.word[p + 1].target)
                        commute(c, p);
            cancel_exposed(c);
            return c.finish();
        };
        Word v = joined({pa, reversed(pb)});
        auto left = reduce(v, false), right = reduce(reversed(v), true);
        b.call(left);
        b.call(right, left->after.size() + 1);
        require(right->after == reversed(left->after), "transport conjugators");
        return b.finish();
    });
}
P Compiler::toggle(const Gate &u, const Gate &g) {
    return memo(key("toggle", {u, g}), [&] {
        require(u.unconditional(), "toggle must be unconditional");
        Builder b(budget, {g});
        if (u.target != g.target) {
            int v = g.controls[u.target];
            require(v >= 0, "toggle needs control");
            if (v != u.lower && v != u.lower + 1) {
                b.edit(1, 0, {}, {u, u});
                commute(b, 1);
                return b.finish();
            }
            auto h = g;
            h.controls[u.target] = 2 * u.lower + 1 - v;
            b.call(transport(g, h));
            require(b.word == Word{u, h, u}, "toggle endpoint");
            return b.finish();
        }
        require(u.lower == g.lower, "same target toggle levels");
        std::vector<int> lines;
        for (int k = 0; k < n; ++k)
            if (g.controls[k] >= 0)
                lines.push_back(k);
        if (lines.empty()) {
            b.edit(1, 0, {}, {g, g});
            return b.finish();
        }
        auto full = expand(u, lines);
        Builder c(budget, {u, g, u});
        c.call(full);
        c.call(full, full->after.size() + 1);
        remove_fibers(c, 0, full->after.size(), g);
        cancel(c, 0);
        return reverse_proof(budget, c.finish());
    });
}
P Compiler::unconditional_commute(const Gate &u, const Gate &v) {
    return memo(key("ucomm", {u, v}), [&] {
        Builder b(budget, {u, v});
        auto ex = expand(v, {u.target});
        b.call(ex, 1);
        for (int pos = 0; pos < d; ++pos) {
            auto g = b.word[pos + 1];
            b.call(toggle(u, g), pos + 1);
            cancel(b, pos);
        }
        commute(b, u.lower);
        b.call(ex, 0, true);
        require(b.word == Word{v, u}, "unconditional commute endpoint");
        return b.finish();
    });
}
P Compiler::conjugate_word(const Word &w, const Word &toggles) {
    Builder b(budget, w);
    for (std::size_t depth = 0; depth < toggles.size(); ++depth) {
        std::size_t pos = depth;
        for (std::size_t j = 0; j < w.size(); ++j) {
            b.call(toggle(toggles[depth], b.word[pos]), pos);
            if (j) {
                cancel(b, pos - 1);
                pos -= 2;
            }
            pos += 3;
        }
    }
    return b.finish();
}
P Compiler::local_conjugation(const Word &base, const Gate &inner, const Gate &outer, int rule) {
    Word root = joined({base, {inner}, reversed(base)});
    Builder loc(budget, root);
    std::size_t width = 1;
    for (std::size_t depth = 0; depth < base.size(); ++depth) {
        std::size_t left = base.size() - 1 - depth;
        auto g = base[left];
        std::vector<int> lines;
        for (int k = 0; k < n; ++k)
            if (k != g.target && inner.controls[k] >= 0 && g.controls[k] < 0)
                lines.push_back(k);
        if (lines.empty()) {
            width += 2;
            continue;
        }
        auto ex = expand(g, lines);
        loc.call(ex, left);
        loc.call(ex, left + ex->after.size() + width);
        auto it = std::find_if(ex->after.begin(), ex->after.end(), [&](const Gate &h) {
            for (int k = 0; k < n; ++k)
                if (k != h.target && k != inner.target && inner.controls[k] >= 0 &&
                    h.controls[k] != inner.controls[k])
                    return false;
            return true;
        });
        require(it != ex->after.end(), "local fiber missing");
        remove_fibers(loc, left, ex->after.size(), *it);
        width += 2;
    }
    Builder end(budget, root);
    int actual = rule;
    if (method == Method::FL2025 || method == Method::FXL2026) {
        require(rule == 7, "binary transfer only");
        actual = 4;
    }
    end.edit(actual, 0, joined({base, {inner}}), joined({{outer}, base}));
    for (std::size_t k = base.size(); k > 0; --k)
        cancel(end, k);
    require(end.word == Word{outer}, "local conjugation end");
    auto lp = loc.finish();
    Builder ans(budget, lp->after);
    ans.call(lp, 0, true);
    ans.call(end.finish());
    return ans.finish();
}
P Compiler::braid(const Gate &x, const Gate &y) {
    return memo(key("braid", {x, y}), [&]() -> P {
        if (method == Method::DC) {
            Builder b(budget, {x, y, x});
            b.edit(x.target == y.target ? 9 : 10, 0, {x, y, x}, {y, x, y});
            return b.finish();
        }
        if (x.target == y.target && x.lower > y.lower)
            return reverse_proof(budget, braid(y, x));
        Word base;
        Gate inner, outer;
        int rule;
        if (x.target != y.target) {
            Word toggles;
            if (y.controls[x.target] == x.lower)
                toggles.push_back(gate(n, x.target, x.lower));
            if (x.controls[y.target] == y.lower)
                toggles.push_back(gate(n, y.target, y.lower));
            if (!toggles.empty()) {
                auto left = conjugate_word({x, y, x}, toggles),
                     right = conjugate_word({y, x, y}, toggles);
                Builder b(budget, left->before);
                b.call(left);
                std::size_t p = toggles.size();
                b.call(braid(b.word[p], b.word[p + 1]), p);
                b.call(right, 0, true);
                return b.finish();
            }
            auto a = gate(n, x.target, x.lower), bb = gate(n, y.target, y.lower);
            a.controls[y.target] = y.lower + 1;
            bb.controls[x.target] = x.lower + 1;
            base = {a, bb, a};
            inner = y;
            outer = x;
            rule = 7;
        } else {
            auto a = gate(n, x.target, x.lower), bb = gate(n, y.target, y.lower);
            base = {a, bb, a};
            inner = x;
            outer = y;
            rule = 6;
        }
        auto conj = local_conjugation(base, inner, outer, rule);
        Word w{x, y, x};
        require(conj->before == joined({w, {inner}, reversed(w)}), "braid local endpoint");
        Builder b(budget, w);
        b.edit(1, 3, {}, {inner, inner});
        insert_pairs(b, 4, w);
        b.call(conj);
        cancel(b, x.target == y.target ? 3 : 0);
        require(b.word == Word{y, x, y}, "braid end");
        return b.finish();
    });
}
void Compiler::cancel_exposed(Builder &b) {
    std::size_t p = 0;
    while (p + 1 < b.word.size()) {
        if (b.word[p] == b.word[p + 1]) {
            cancel(b, p);
            if (p)
                --p;
        } else
            ++p;
    }
}
void Compiler::reduce_aux(Builder &b) {
    std::size_t start = 0;
    while (start < b.word.size()) {
        if (!b.word[start].unconditional()) {
            ++start;
            continue;
        }
        std::size_t end = start;
        while (end < b.word.size() && b.word[end].unconditional())
            ++end;
        sort_x(b, start, end);
        std::size_t p = start;
        while (p + 1 < end) {
            if (b.word[p] == b.word[p + 1]) {
                cancel(b, p);
                end -= 2;
                if (p > start)
                    --p;
            } else
                ++p;
        }
        start = end;
    }
}
} // namespace rv
