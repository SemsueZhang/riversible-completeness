#include "rv/compiler.hpp"
namespace rv {
P Compiler::square(const Gate &g, const State &next) {
    require(method == Method::DC, "grid squares belong to the DC route");
    // g crosses slices on q. next differs by one lower-coordinate grid edge.
    int q = g.target, p = -1;
    for (int k = 0; k < n; ++k)
        if (k != q && g.controls[k] != next[k]) {
            require(p < 0 && std::abs(g.controls[k] - next[k]) == 1, "square prefix");
            p = k;
        }
    require(p >= 0, "empty square move");
    auto center = g;
    center.controls[p] = next[p];
    int i = std::min(g.controls[p], next[p]);
    auto u = gate(n, p, i);
    for (int k = 0; k < n; ++k)
        if (k != p && k != q)
            u.controls[k] = g.controls[k];
    auto a = u, c = u;
    a.controls[q] = g.lower;
    c.controls[q] = g.lower + 1;
    return memo(key("square", {g, center}), [&] {
        Builder b(budget, {g});
        bool forward = g.controls[p] == i;
        if (forward) {
            b.edit(1, 0, {}, {u, u});
            b.edit(1, 3, {}, {u, u});
            b.edit(11, 1, {u, g, u}, {center});
        } else
            b.edit(11, 0, {g}, {u, center, u});
        Word fibers;
        for (int v = 0; v < d; ++v) {
            auto f = u;
            f.controls[q] = v;
            fibers.push_back(f);
        }
        b.edit(2, 0, {u}, fibers);
        b.edit(2, d + 1, {u}, fibers);
        for (int v = 0; v < d; ++v) {
            if (v == g.lower || v == g.lower + 1)
                continue;
            auto h = fibers[v];
            auto l = std::find(b.word.begin(), b.word.end(), h);
            auto r = std::find(l + 1, b.word.end(), h);
            require(r != b.word.end(), "square fiber");
            std::size_t at = l - b.word.begin(), end = r - b.word.begin();
            while (at + 1 < end) {
                commute(b, at);
                ++at;
            }
            cancel(b, at);
        }
        commute(b, 3);
        require(b.word == Word{a, c, center, c, a}, "square endpoint");
        return b.finish();
    });
}
P Compiler::grid_route(const Gate &g) {
    require(method == Method::DC, "grid routing is not an FL2025 implementation");
    return memo(key("grid", {g}), [&] {
        Builder b(budget, {g});
        if (indices.count(g))
            return b.finish();
        if (!g.full()) {
            auto ex = expand(g);
            b.call(ex);
            std::size_t pos = 0;
            for (const auto &child : ex->after) {
                auto p = grid_route(child);
                b.call(p, pos);
                pos += p->after.size();
            }
            return b.finish();
        }
        int t = g.target;
        require(t > 0, "off-path lowest target");
        auto prefixes = gray(d, t);
        State prefix(g.controls.begin(), g.controls.begin() + t);
        auto it = std::find(prefixes.begin(), prefixes.end(), prefix);
        require(it != prefixes.end(), "prefix not found");
        int at = int(it - prefixes.begin()), end = g.lower % 2 == 0 ? int(prefixes.size()) - 1 : 0,
            delta = at < end ? 1 : -1;
        std::size_t center_pos = 0;
        while (at != end) {
            budget.tick(b.word.size());
            at += delta;
            State next = b.word[center_pos].controls;
            for (int k = 0; k < t; ++k)
                next[k] = prefixes[at][k];
            auto p = square(b.word[center_pos], next);
            b.call(p, center_pos);
            center_pos += 2;
        }
        for (const auto &h : b.word)
            require(indices.count(h), "grid output is not path gate");
        return b.finish();
    });
}
P Compiler::route(const Word &input) {
    for (auto &g : input)
        validate(g, d, n);
    Builder b(budget, input);
    if (method == Method::NF) {
        cancel_exposed(b);
        for (int target = n - 1; target >= 0; --target) {
            std::size_t pos = 0;
            while (pos < b.word.size()) {
                budget.tick(b.word.size());
                auto g = b.word[pos];
                if (g.target != target || indices.count(g)) {
                    ++pos;
                    continue;
                }
                b.call(coordinated(g), pos);
                cancel_exposed(b);
                pos = 0;
            }
        }
    } else if (method == Method::FL2025) {
        // Theorem 2 first refines the entire circuit to full-control gates.
        // The following pass then reverses each coordinate-deletion proof.
        std::size_t pos = 0;
        for (const auto &g : input) {
            auto ex = expand(g);
            b.call(ex, pos);
            pos += ex->after.size();
        }
        const Word full = b.word;
        pos = 0;
        for (const auto &g : full) {
            auto p = fl_route(g);
            b.call(p, pos);
            pos += p->after.size();
        }
    } else {
        std::size_t pos = 0;
        for (const auto &g : input) {
            P p;
            if (method == Method::FXL2026)
                p = fxl_route(g);
            else
                p = grid_route(g);
            b.call(p, pos);
            pos += p->after.size();
        }
    }
    for (auto &g : b.word)
        require(indices.count(g), "routing did not terminate in path alphabet");
    return b.finish();
}
} // namespace rv
