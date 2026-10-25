#include "rv/compiler.hpp"
#include <limits>
#include <set>

namespace rv {
namespace {
// Hierarchical bitsets support ordered position queries without per-node
// allocations. Each upper-level bit marks a nonempty word in the level below.
class Positions {
    std::vector<std::vector<std::uint64_t>> levels;
    std::set<int> sparse;
    bool use_sparse = false;
    int universe = 0;
    static int lowbit(std::uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
        return __builtin_ctzll(x);
#else
        int k = 0; while (!(x & 1)) { ++k; x >>= 1; } return k;
#endif
    }
    int find(std::size_t level, int from) const {
        if (level >= levels.size()) return -1;
        auto word = std::size_t(from) / 64;
        if (word >= levels[level].size()) return -1;
        auto bits = levels[level][word] & (~std::uint64_t(0) << (from % 64));
        if (bits) return int(word * 64) + lowbit(bits);
        int following = find(level + 1, int(word + 1));
        if (following < 0) return -1;
        return following * 64 + lowbit(levels[level][following]);
    }
  public:
    explicit Positions(std::size_t size = 0, bool sparse_mode = false)
        : use_sparse(sparse_mode), universe(int(size)) {
        if (use_sparse) return;
        while (size) {
            size = (size + 63) / 64;
            levels.emplace_back(size, 0);
            if (size == 1) break;
        }
    }
    void set(int p, bool wanted) {
        if (use_sparse) {
            if (wanted) sparse.insert(p); else sparse.erase(p);
            return;
        }
        for (std::size_t level = 0; level < levels.size(); ++level) {
            auto &word = levels[level][p / 64];
            auto old = word;
            auto bit = std::uint64_t(1) << (p % 64);
            if (wanted) word |= bit; else word &= ~bit;
            if (bool(old) == bool(word)) break;
            wanted = word != 0; p /= 64;
        }
    }
    int first_ge(int p) const {
        if (use_sparse) {
            if (p < 0) return -1;
            auto it = sparse.lower_bound(p);
            return it == sparse.end() ? -1 : *it;
        }
        return p < 0 || p >= universe ? -1 : find(0, p);
    }
};
// Nodes never change order: swaps/braids change labels, cancellations unlink
// nodes. Stable original positions therefore also order the live redexes.
// Only the current word is stored, not a history of intermediate words.
class PathState {
  public:
    std::vector<int> value, prev, next;
    std::vector<Positions> occurrences;
    Positions redex, equal_pair, broken_chain;
    int head = -1, tail = -1;
    std::size_t length;
    PathState(std::vector<int> input, std::size_t alphabet)
        : value(std::move(input)), prev(value.size()), next(value.size()),
          occurrences(alphabet, Positions(value.size(),
              alphabet * ((value.size() + 63) / 64) > 4 * 1024 * 1024)), redex(value.size()),
          equal_pair(value.size()), broken_chain(value.size()), length(value.size()) {
        require(length <= std::size_t(std::numeric_limits<int>::max()), "path index overflow");
        for (int p = 0; p < int(length); ++p) {
            prev[p] = p - 1;
            next[p] = p + 1 < int(length) ? p + 1 : -1;
            occurrences[value[p]].set(p, true);
        }
        if (length) { head = 0; tail = int(length) - 1; }
        for (int p = 0; p < int(length); ++p) refresh(p);
    }
    static bool before(int p, int stop) { return p >= 0 && (stop < 0 || p < stop); }
    void refresh(int p) {
        if (p < 0) return;
        if (value[p] < 0 || next[p] < 0) {
            redex.set(p, false); equal_pair.set(p, false);
            broken_chain.set(p, false);
            return;
        }
        int q = next[p], r = next[q];
        equal_pair.set(p, value[p] == value[q]);
        broken_chain.set(p, value[q] != value[p] + 1);
        redex.set(p, value[p] == value[q] || std::abs(value[p] - value[q]) >= 2 ||
            (r >= 0 && value[p] == value[r] && value[q] == value[p] + 1));
    }
    void relabel(const int *positions, const int *labels, std::size_t size) {
        for (std::size_t j = 0; j < size; ++j) {
            int p = positions[j];
            if (value[p] == labels[j]) continue;
            occurrences[value[p]].set(p, false);
            value[p] = labels[j];
            occurrences[value[p]].set(p, true);
        }
        // All calls change a contiguous live interval. Only that interval and
        // its two predecessors can change a redex; refresh each once.
        int p = positions[0];
        if (prev[p] >= 0) p = prev[p];
        if (prev[p] >= 0) p = prev[p];
        for (;;) {
            refresh(p);
            if (p == positions[size - 1]) break;
            p = next[p];
        }
    }
    void erase(int p) {
        int a = prev[p], b = next[p];
        occurrences[value[p]].set(p, false);
        value[p] = -1;
        if (a >= 0) next[a] = b; else head = b;
        if (b >= 0) prev[b] = a; else tail = a;
        --length;
        refresh(p); refresh(a);
        if (a >= 0) refresh(prev[a]);
    }
    bool chain(int pivot, int stop, int i) const {
        int first = next[pivot];
        if (first == stop) return true;
        if (first < 0 || value[first] != i + 1) return false;
        int broken = broken_chain.first_ge(first);
        int last = stop < 0 ? tail : prev[stop];
        return broken < 0 || broken >= last;
    }
};
} // namespace

P Compiler::indexed_normalize_path(const Word &input) {
    require(method == Method::FL2025 || method == Method::FXL2026, "indexed binary normalization");
    std::vector<int> labels;
    labels.reserve(input.size());
    for (const auto &g : input) {
        budget.tick();
        labels.push_back(indices.at(g));
    }
    PathState s(std::move(labels), path_gates.size());
    Count count = 0;
    std::size_t peak = input.size();
    auto charge = [&](Count rules, std::size_t local_before, std::size_t local_peak) {
        require(local_before <= s.length, "indexed rewrite size");
        const auto context = s.length - local_before;
        require(local_peak <= budget.limits.max_word - context,
                "resource_limit: contextual peak word");
        budget.tick(context + local_peak, true);
        peak = std::max(peak, context + local_peak);
        count = checked_add(count, rules);
    };
    auto swap = [&](int p, int q) {
        require(p >= 0 && s.next[p] == q && std::abs(s.value[p] - s.value[q]) >= 2,
                "indexed Rule 3 precondition");
        charge(1, 2, 2);
        int positions[]{p, q}, labels[]{s.value[q], s.value[p]};
        s.relabel(positions, labels, 2);
    };
    auto cancel = [&](int p) {
        int q = s.next[p];
        require(q >= 0 && s.value[p] == s.value[q], "indexed cancellation");
        charge(1, 2, 2);
        s.erase(q); s.erase(p);
    };
    auto apply_braid = [&](int p) {
        int q = s.next[p], r = q < 0 ? -1 : s.next[q];
        require(r >= 0 && s.value[p] == s.value[r] &&
                    std::abs(s.value[p] - s.value[q]) == 1, "indexed braid");
        int x = s.value[p], y = s.value[q];
        auto proof = braid(path_gates[x], path_gates[y]);
        charge(proof->rules, 3, proof->peak);
        int positions[]{p, q, r}, labels[]{y, x, y};
        s.relabel(positions, labels, 3);
    };
    auto extract_fxl = [&](int pivot, int stop, int i) {
        while (!s.chain(pivot, stop, i)) {
            budget.tick(s.length);
            int first = s.next[pivot];
            require(PathState::before(first, stop), "FXL indexed empty tail");
            if (s.value[first] >= i + 2) {
                swap(pivot, first); pivot = first;
                continue;
            }
            // Same leftmost priority as Algorithm 3: cancel, braid, commute.
            int p = s.redex.first_ge(first);
            require(PathState::before(p, stop),
                    "FXL indexed missing redex");
            int q = s.next[p], r = q < 0 ? -1 : s.next[q];
            require(PathState::before(q, stop), "FXL indexed redex crosses block");
            if (s.value[p] == s.value[q]) cancel(p);
            else if (PathState::before(r, stop) && s.value[p] == s.value[r] &&
                     s.value[q] == s.value[p] + 1) apply_braid(p);
            else { require(std::abs(s.value[p] - s.value[q]) >= 2, "FXL block boundary"); swap(p, q); }
        }
        return pivot;
    };
    auto extract_fl = [&](int pivot, int stop, int i) {
        for (;;) {
            budget.tick(s.length);
            int first = s.next[pivot];
            int eq = s.equal_pair.first_ge(first);
            if (PathState::before(eq, stop) &&
                PathState::before(s.next[eq], stop)) {
                cancel(eq); continue; // Lemma 4, Case 1, leftmost interior pair.
            }
            if (s.chain(pivot, stop, i)) return pivot;
            int broken = s.broken_chain.first_ge(pivot);
            require(broken >= 0, "FL indexed chain boundary");
            int at = s.next[broken];
            require(PathState::before(at, stop), "FL indexed block boundary");
            int x = s.value[at], top = s.value[s.prev[at]];
            if (x >= top + 2) {
                for (int p = at; p != pivot; p = s.prev[p]) swap(s.prev[p], p);
                pivot = s.next[pivot]; // Case 2.
                continue;
            }
            require(x >= i + 1 && x < top, "FL indexed case partition");
            int triple = s.occurrences[x].first_ge(pivot);
            int third = s.next[s.next[triple]];
            for (int p = at; p != third; p = s.prev[p]) swap(s.prev[p], p);
            apply_braid(triple); // Case 4 -> Case 3.
            for (int p = triple; p != pivot; p = s.prev[p]) swap(s.prev[p], p);
            pivot = s.next[pivot];
        }
    };
    auto extract = [&](int pivot, int stop, int i) {
        return method == Method::FL2025 ? extract_fl(pivot, stop, i)
                                       : extract_fxl(pivot, stop, i);
    };
    int stop = -1;
    for (int i = 0; i < int(path_gates.size()); ++i) {
        for (;;) {
            budget.tick(s.length);
            int first = s.occurrences[i].first_ge(0);
            if (!PathState::before(first, stop)) break;
            int second = s.occurrences[i].first_ge(first + 1);
            if (!PathState::before(second, stop)) break;
            if (method == Method::FL2025) {
                int near = i + 1 < int(s.occurrences.size())
                    ? s.occurrences[i + 1].first_ge(first) : -1;
                bool distant = near < 0 || near >= second;
                if (distant) {
                    Count steps = 1;
                    for (int p = s.next[first]; p != second; p = s.next[p]) {
                        budget.tick(); steps = checked_add(steps, 1);
                    }
                    charge(steps, 2, 2); // Same Rule-3 swaps followed by Rule 1.
                    s.erase(second); s.erase(first);
                    continue;
                }
                // FL Lemma 4 simple case (ii), before its four general cases.
                int l = first, r = second, expected = i;
                Word half;
                while (l >= 0 && r >= 0 && l <= r &&
                       s.value[l] == expected && s.value[r] == expected) {
                    half.push_back(path_gates[expected]);
                    if (l == r) break;
                    l = s.next[l]; r = s.prev[r]; ++expected;
                }
                if (l == r && l >= 0 && !half.empty() && s.value[l] == expected) {
                    auto proof = fl_reverse_chain(half);
                    charge(proof->rules, proof->before.size(), proof->peak);
                    std::vector<int> positions, after;
                    int p = first;
                    for (const auto &g : proof->after) {
                        positions.push_back(p); after.push_back(indices.at(g)); p = s.next[p];
                    }
                    require(positions.back() == second, "FL palindrome interval");
                    s.relabel(positions.data(), after.data(), positions.size());
                    continue;
                }
            }
            int pivot = extract(first, second, i);
            if (s.next[pivot] == second) cancel(pivot);
            else {
                int first_chain = s.next[pivot];
                for (int p = second; s.prev[p] != first_chain; p = s.prev[p]) swap(s.prev[p], p);
                apply_braid(pivot);
            }
        }
        int first = s.occurrences[i].first_ge(0);
        if (PathState::before(first, stop)) stop = extract(first, stop, i);
    }
    require(s.head == stop, "indexed unextracted prefix");
    auto result = std::make_shared<RewriteSummary>();
    result->before = input;
    result->after.reserve(s.length);
    for (int p = s.head; p >= 0; p = s.next[p]) result->after.push_back(path_gates[s.value[p]]);
    result->rules = count; result->peak = peak;
    return result;
}
} // namespace rv
