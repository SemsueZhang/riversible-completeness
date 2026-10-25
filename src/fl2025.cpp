#include "rv/compiler.hpp"
#include <set>

namespace rv {
namespace {
// A left half H represents H[0] ... H[k] ... H[0], with one central gate.
Word palindrome(const Word &half) {
    require(!half.empty(), "FL empty palindrome half");
    return joined({half, reversed(slice(half, 0, half.size() - 1))});
}
bool opposite_control(const Gate &a, const Gate &b) {
    for (std::size_t k = 0; k < a.controls.size(); ++k)
        if (a.controls[k] >= 0 && b.controls[k] >= 0 && a.controls[k] != b.controls[k])
            return true;
    return false;
}
// Check the hypotheses of Lemma 1 by walking edge endpoints, not by computing
// a circuit truth table. This also avoids invoking Lemma 3 on a self-intersecting
// coordinate walk after a deletion. The paper leaves the choice of g,h open.
bool simple_edge_chain(const Word &half, State start, const State &end, Budget &budget) {
    std::set<State> seen{start};
    for (const auto &g : half) {
        budget.tick();
        if (!g.full())
            return false;
        for (int k = 0; k < int(start.size()); ++k)
            if (k != g.target && start[k] != g.controls[k])
                return false;
        start[g.target] ^= 1;
        if (!seen.insert(start).second)
            return false;
    }
    return start == end;
}
} // namespace

P Compiler::fl_reverse_chain(const Word &half) {
    require(method == Method::FL2025 && !half.empty(), "FL Lemma 3 input");
    return memo(key("fl-lemma3", half), [&] {
        for (std::size_t i = 0; i < half.size(); ++i) {
            require(half[i].full(), "FL Lemma 3 needs full controls");
            if (i + 1 < half.size()) {
                require(half[i].target != half[i + 1].target &&
                            !opposite_control(half[i], half[i + 1]),
                        "FL Lemma 3 adjacent-edge hypothesis");
            }
            for (std::size_t j = i + 2; j < half.size(); ++j) {
                budget.tick();
                require(opposite_control(half[i], half[j]),
                        "FL Lemma 3 nonadjacent-edge hypothesis");
            }
        }
        Builder b(budget, palindrome(half));
        // Lemma 3: braid the central triple, commute the two copies of the
        // last gate outwards, and repeat on the shorter interior palindrome.
        for (std::size_t depth = 0, length = half.size(); length > 1; ++depth, --length) {
            b.call(braid(b.word[depth + length - 2], b.word[depth + length - 1]),
                   depth + length - 2);
            for (std::size_t at = depth + length - 2; at > depth; --at)
                commute(b, at - 1);
            for (std::size_t at = depth + length; at < depth + 2 * length - 2; ++at)
                commute(b, at);
        }
        require(b.word == palindrome(reversed(half)), "FL Lemma 3 endpoint");
        return b.finish();
    });
}

P Compiler::fl_x_swap(const Gate &x, const Gate &g) {
    require(method == Method::FL2025 && x.unconditional(), "FL X movement");
    return memo(key("fl-x-swap", {x, g}), [&] {
        Builder b(budget, {x, g});
        auto changed = g;
        if (x.target != g.target) {
            require(g.controls[x.target] >= 0, "FL Rule 9 needs the X line as control");
            changed.controls[x.target] ^= 1;
            // X G -> X G X X -> G' X. The middle proof expands the
            // paper's polarity-change derivation into original Rules 1--5.
            b.edit(1, 2, {}, {x, x});
            b.call(unconditional_flip(g, x.target));
        } else {
            // Derived Rule 7: refine X on G's controls, move G across the
            // distinct fibers by Rule 3, then undo the refinement. Crossing
            // an identical copy changes no word and incurs no rule count.
            std::vector<int> lines;
            for (int k = 0; k < n; ++k)
                if (g.controls[k] >= 0)
                    lines.push_back(k);
            if (!lines.empty()) {
                auto ex = expand(x, lines);
                b.call(ex);
                for (std::size_t at = ex->after.size(); at > 0; --at)
                    if (b.word[at - 1] != b.word[at])
                        commute(b, at - 1);
                b.call(ex, 1, true);
            }
        }
        require(b.word == Word{changed, x}, "FL X movement endpoint");
        return b.finish();
    });
}

P Compiler::fl_polarize(const Word &word, const std::vector<int> &lines) {
    Builder b(budget, word);
    for (std::size_t depth = 0; depth < lines.size(); ++depth) {
        auto x = gate(n, lines[depth]);
        b.edit(1, depth, {}, {x, x});
        for (std::size_t j = 0; j < word.size(); ++j) {
            std::size_t at = depth + 1 + j;
            b.call(fl_x_swap(x, b.word[at + 1]), at);
        }
    }
    return b.finish();
}

P Compiler::fl_rule5_segment(const Gate &a, const Gate &z, const Word &middle) {
    require(method == Method::FL2025 && a.target == z.target && !middle.empty(),
            "FL Theorem 2 Rule 5 segment");
    // Input: A E B_k ... B_1 ... B_k E A. The B order is the order
    // AFTER the second Lemma 3 in Theorem 2, not the original path order.
    Word before = joined({{a, z}, palindrome(middle), {z, a}});
    return memo(key("fl-multi-rule5", before), [&] {
        int q = a.target;
        std::vector<int> qs, flips;
        for (const auto &g : middle) {
            require(g.target != q &&
                        std::find(qs.begin(), qs.end(), g.target) == qs.end(),
                    "FL Rule 5 needs distinct middle coordinates");
            qs.push_back(g.target);
        }
        // Normalize A to A_0 and E to A_1, and the q controls to positive.
        // Ascending wire order fixes the otherwise unspecified X schedule.
        for (int k = 0; k < n; ++k)
            if ((k == q && middle.front().controls[q] == 0) ||
                (std::find(qs.begin(), qs.end(), k) != qs.end() && a.controls[k] == 1))
                flips.push_back(k);
        auto polar = fl_polarize(before, flips);
        auto a0 = a, a1 = a;
        for (int k : qs) {
            a0.controls[k] = 0;
            a1.controls[k] = 1;
        }
        Word bs, changed_bs, changed_original;
        for (std::size_t i = 0; i < qs.size(); ++i) {
            auto g = a0;
            g.target = qs[i];
            g.controls[q] = 1;
            for (std::size_t j = 0; j < qs.size(); ++j)
                g.controls[qs[j]] = j == i ? -1 : (j > i ? 1 : 0);
            bs.push_back(g);
            g.controls[q] = 0;
            changed_bs.push_back(g);
            auto original = middle[i];
            require(original.controls[q] >= 0, "FL Rule 5 missing pivot control");
            original.controls[q] ^= 1;
            changed_original.push_back(original);
        }
        Word normalized_long = joined({{a0, a1}, palindrome(bs), {a1, a0}});
        Word normalized_short = palindrome(changed_bs);
        auto undo = fl_polarize(palindrome(changed_original), flips);
        Builder b(budget, before);
        b.call(polar);
        // Builder checks the complete template at this position. Regardless
        // of |Q|, this is ONE application of the original basic Rule 5.
        b.edit(5, flips.size(), normalized_long, normalized_short);
        b.call(undo, 0, true);
        require(b.word == palindrome(changed_original), "FL multi-Q endpoint");
        return b.finish();
    });
}

P Compiler::fl_route(const Gate &g) {
    return memo(key("fl-theorem2", {g}), [&]() -> P {
        require(method == Method::FL2025 && d == 2, "FL binary routing");
        if (!g.full()) {
            Builder b(budget, {g});
            auto ex = expand(g);
            b.call(ex);
            std::size_t pos = 0;
            for (const auto &child : ex->after) {
                auto p = fl_route(child);
                b.call(p, pos);
                pos += p->after.size();
            }
            return b.finish();
        }
        if (indices.count(g)) {
            Builder b(budget, {g});
            return b.finish();
        }
        State x = g.controls;
        x[g.target] = 0;
        State y = x;
        y[g.target] = 1;
        auto a = std::find(path.begin(), path.end(), x) - path.begin();
        auto z = std::find(path.begin(), path.end(), y) - path.begin();
        if (a > z) {
            std::swap(a, z);
            std::swap(x, y);
        }
        Word half = slice(path_gates, std::size_t(a), std::size_t(z));
        const Word wanted = palindrome(half);
        Builder b(budget, wanted);
        while (half.size() > 1) {
            budget.tick(b.word.size());
            std::size_t first = half.size(), last = half.size(), best = 0;
            Word next;
            // Deterministic choice among the paper's legal repeated-coordinate
            // pairs: largest |Q| first, then leftmost. Preserve a simple edge
            // chain so every subsequent Lemma 1/3 invocation is applicable.
            for (std::size_t left = 0; left < half.size(); ++left) {
                std::set<int> used{half[left].target};
                for (std::size_t right = left + 1; right < half.size(); ++right) {
                    budget.tick();
                    if (half[right].target == half[left].target) {
                        Word candidate = slice(half, 0, left);
                        for (std::size_t j = left + 1; j < right; ++j) {
                            auto h = half[j];
                            h.controls[half[left].target] ^= 1;
                            candidate.push_back(h);
                        }
                        auto tail = slice(half, right + 1, half.size());
                        candidate.insert(candidate.end(), tail.begin(), tail.end());
                        std::size_t width = right - left - 1;
                        if (width > best && simple_edge_chain(candidate, x, y, budget)) {
                            first = left;
                            last = right;
                            best = width;
                            next = std::move(candidate);
                        }
                        break;
                    }
                    if (!used.insert(half[right].target).second)
                        break;
                }
            }
            require(first < half.size(), "FL Theorem 2: no admissible coordinate pair");
            const std::size_t tail = half.size() - last - 1;
            const std::size_t block = last - first;
            // Theorem 2, before equation (1): Lemma 3 on the suffix at h.
            b.call(fl_reverse_chain(slice(half, last, half.size())), last);
            // Move [M_g ... M_(h-1)] past the reversed suffix, on BOTH
            // halves. Each adjacent interchange is checked and charged.
            for (std::size_t j = 0; j < tail; ++j) {
                for (std::size_t at = first + block + j; at > first + j; --at) {
                    std::size_t left_swap = at - 1;
                    std::size_t mirror_swap = b.word.size() - 2 - left_swap;
                    commute(b, left_swap);
                    commute(b, mirror_swap);
                }
            }
            // Equation (1) -> A E B_(h-1) ... B_(g+1) ||.
            Word inner = slice(half, first + 1, last + 1);
            b.call(fl_reverse_chain(inner), first + tail + 1);
            auto middle = reversed(slice(half, first + 1, last));
            b.call(fl_rule5_segment(half[first], half[last], middle), first + tail);
            // Equation (2) -> (3): reverse the remaining central chain.
            Word remaining = reversed(slice(half, last + 1, half.size()));
            for (auto h : middle) {
                h.controls[half[first].target] ^= 1;
                remaining.push_back(h);
            }
            b.call(fl_reverse_chain(remaining), first);
            half = std::move(next);
            require(b.word == palindrome(half), "FL coordinate-deletion endpoint");
        }
        require(b.word == Word{g}, "FL Theorem 2 final gate");
        auto result = reverse_proof(budget, b.finish());
        require(result->after == wanted, "FL Theorem 2 path word");
        return result;
    });
}

std::size_t Compiler::fl_extract_block(Builder &b, std::size_t pos,
                                       std::size_t length, int i) {
    // Lemma 5, implementing Cases 1--4 of Lemma 4 at a growing left chain.
    std::size_t start = pos, end = pos + length;
    require(length && indices.at(b.word.at(start)) == i, "FL Lemma 5 input");
    for (std::size_t k = start + 1; k < end; ++k)
        require(indices.at(b.word[k]) > i, "FL Lemma 5 index hypothesis");
    for (;;) {
        budget.tick(b.word.size());
        bool cancelled = false;
        for (std::size_t at = start + 1; at + 1 < end; ++at)
            if (b.word[at] == b.word[at + 1]) {
                cancel(b, at); // Case 1, leftmost interior pair.
                end -= 2;
                cancelled = true;
                break;
            }
        if (cancelled)
            continue;
        std::size_t at = start + 1;
        while (at < end && indices.at(b.word[at]) == i + int(at - start))
            ++at;
        if (at == end)
            return end - start;
        int x = indices.at(b.word[at]);
        int top = indices.at(b.word[at - 1]);
        if (x >= top + 2) {
            for (std::size_t k = at; k > start; --k)
                commute(b, k - 1); // Case 2.
            ++start;
            continue;
        }
        require(x >= i + 1 && x < top, "FL Lemma 5 case partition");
        std::size_t triple = start + std::size_t(x - i);
        for (std::size_t k = at; k > triple + 2; --k)
            commute(b, k - 1); // Case 4 -> Case 3.
        b.call(braid(b.word[triple], b.word[triple + 1]), triple);
        for (std::size_t k = triple; k > start; --k)
            commute(b, k - 1); // Case 3 -> Case 2, emit M_(x+1).
        ++start;
    }
}

P Compiler::fl_reduce_pair(const Word &word, int i) {
    Builder b(budget, word);
    require(word.size() >= 2 && indices.at(word.front()) == i &&
                indices.at(word.back()) == i, "FL Lemma 4 endpoints");
    for (std::size_t k = 1; k + 1 < word.size(); ++k)
        require(indices.at(word[k]) > i, "FL ascending Proposition 3 schedule");
    // First use the paper's two simple cases, before Cases 1--4.
    bool distant = true;
    for (std::size_t k = 1; k + 1 < word.size(); ++k)
        distant = distant && indices.at(word[k]) >= i + 2;
    if (distant) {
        for (std::size_t k = 0; k + 2 < b.word.size(); ++k)
            commute(b, k);
        cancel(b, b.word.size() - 2);
        return b.finish();
    }
    if (word.size() % 2 == 1) {
        Word half = slice(word, 0, word.size() / 2 + 1);
        bool chain = true;
        for (std::size_t k = 0; k < half.size(); ++k)
            chain = chain && indices.at(half[k]) == i + int(k);
        if (chain && word == palindrome(half)) {
            b.call(fl_reverse_chain(half));
            return b.finish();
        }
    }
    // Cases 1--4 emit gates to the left until the left occurrence starts a
    // consecutive block. The closing M_i is then eliminated (k=0) or handled
    // by Case 4/3 (k>0). No FXL extraction routine is used.
    std::size_t length = fl_extract_block(b, 0, b.word.size() - 1, i);
    std::size_t start = b.word.size() - 1 - length;
    if (length == 1)
        cancel(b, start);
    else {
        for (std::size_t at = b.word.size() - 1; at > start + 2; --at)
            commute(b, at - 1);
        b.call(braid(b.word[start], b.word[start + 1]), start);
    }
    return b.finish();
}

P Compiler::fl_normalize_path(const Word &input) {
    Builder b(budget, input);
    for (const auto &g : input)
        require(indices.count(g), "FL Proposition 3 input alphabet");
    std::size_t active = input.size();
    for (int i = 0; i < int(path_gates.size()); ++i) {
        for (;;) {
            budget.tick(b.word.size());
            std::size_t first = active, second = active;
            for (std::size_t k = 0; k < active; ++k)
                if (indices.at(b.word[k]) == i) {
                    if (first == active)
                        first = k;
                    else {
                        second = k;
                        break;
                    }
                }
            if (second == active)
                break;
            auto reduction = fl_reduce_pair(slice(b.word, first, second + 1), i);
            b.call(reduction, first);
            active -= reduction->before.size() - reduction->after.size();
        }
        std::size_t pos = 0;
        while (pos < active && indices.at(b.word[pos]) != i)
            ++pos;
        if (pos < active) {
            std::size_t old = b.word.size();
            std::size_t length = fl_extract_block(b, pos, active - pos, i);
            active -= old - b.word.size();
            active -= length;
        }
    }
    require(active == 0, "FL Proposition 3 remaining prefix");
    return b.finish();
}
} // namespace rv
