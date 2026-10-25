#include "rv/compiler.hpp"
namespace rv {
namespace {
void pair_insert(Builder &b, std::size_t pos, const Word &w) {
    for (std::size_t k = 0; k < w.size(); ++k) {
        auto g = w[w.size() - 1 - k];
        b.edit(1, pos + k, {}, {g, g});
    }
}
} // namespace
Result normalize(const Circuit &a, Method method, Budget &budget) {
    Compiler compiler(a.d, a.n, method, budget);
    auto routed = compiler.route(a.gates);
    if (method != Method::DC) {
        auto norm = compiler.normalize_path(routed->after);
        // Both stages already validated their own endpoints. Composing their
        // counts does not require replaying/copying the entire routed circuit.
        return {true, checked_add(routed->total(), norm->total())};
    }
    // Section IV is an equality construction. Its NF adapter synthesizes the
    // unique target by Algorithm 1, then connects it by direct cancellation.
    Word target = compiler.insertion_normal_form(a.gates);
    auto scan = compiler.cancel_path(joined({routed->after, reversed(target)}));
    require(scan.identity, "DC normal-form target mismatch");
    Builder b(budget, a.gates);
    b.call(routed);
    pair_insert(b, b.word.size(), target);
    b.call(scan.proof);
    require(b.word == target, "DC normal-form endpoint");
    return {true, b.finish()->total()};
}
Result equivalent(const Circuit &a, const Circuit &b, Method method, Budget &budget) {
    require(a.d == b.d && a.n == b.n, "circuit dimensions differ");
    Compiler compiler(a.d, a.n, method, budget);
    auto ra = compiler.route(a.gates), rb = compiler.route(b.gates);
    Count routing = checked_add(ra->total(), rb->total());
    if (method != Method::DC) {
        auto na = compiler.normalize_path(ra->after), nb = compiler.normalize_path(rb->after);
        Count total = checked_add(routing, checked_add(na->total(), nb->total()));
        if (na->after != nb->after)
            return {false, total};
        Builder out(budget, a.gates);
        out.call(ra);
        out.call(na);
        out.call(nb, 0, true);
        out.call(rb, 0, true);
        require(out.word == b.gates, "equivalence endpoint");
        return {true, out.finish()->total()};
    }
    auto scan = compiler.cancel_path(joined({ra->after, reversed(rb->after)}));
    if (!scan.identity)
        return {false, checked_add(routing, scan.proof->total())};
    Builder out(budget, a.gates);
    out.call(ra);
    pair_insert(out, out.word.size(), rb->after);
    out.call(scan.proof);
    out.call(rb, 0, true);
    require(out.word == b.gates, "DC equivalence endpoint");
    return {true, out.finish()->total()};
}
} // namespace rv
