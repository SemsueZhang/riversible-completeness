#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace rv {
using Count = std::uint64_t;
using State = std::vector<int>;
struct Gate {
    int target = 0, lower = 0;
    std::vector<int> controls; // -1: absent; target entry must be -1
    bool operator==(const Gate &b) const {
        return target == b.target && lower == b.lower && controls == b.controls;
    }
    bool operator!=(const Gate &b) const {
        return !(*this == b);
    }
    bool operator<(const Gate &b) const {
        return std::tie(target, lower, controls) < std::tie(b.target, b.lower, b.controls);
    }
    bool unconditional() const {
        return std::all_of(controls.begin(), controls.end(), [](int x) { return x < 0; });
    }
    bool full() const {
        for (int k = 0; k < int(controls.size()); ++k)
            if (k != target && controls[k] < 0)
                return false;
        return true;
    }
};
using Word = std::vector<Gate>;
Word reversed(Word w);
Word slice(const Word &w, std::size_t a, std::size_t b);
Word joined(std::initializer_list<Word> words);
Gate gate(int n, int t, int lower = 0);
void require(bool condition, const std::string &message);
void validate(const Gate &g, int d, int n);
State apply(const Gate &g, State x);
std::vector<State> gray(int d, int n);
Gate edge_gate(const State &a, const State &b);
Count checked_add(Count a, Count b);

struct Limits {
    std::size_t max_states = 65536, max_word = 100000000, max_work = 2000000;
    double seconds = 60;
};
struct Budget {
    Limits limits;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::size_t work = 0;
    explicit Budget(Limits l = {}) : limits(l) {}
    void tick(std::size_t word_size = 0, bool construction = false);
};
struct RewriteSummary;
using P = std::shared_ptr<const RewriteSummary>;
struct RewriteSummary {
    Word before, after;
    Count rules = 0;
    std::size_t peak = 0;
    Count total() const;
};
struct Builder {
    Budget &budget;
    Word before, word;
    Count rules = 0;
    std::size_t peak;
    Builder(Budget &b, Word w);
    void call(P p, std::size_t pos = 0, bool backwards = false);
    void edit(int rule, std::size_t pos, Word from, Word to);
    P finish();
};
P reverse_proof(Budget &budget, P p);

enum class Method { FL2025, FXL2026, NF, DC };
std::string method_name(Method m);
Method parse_method(const std::string &s);
struct Circuit {
    int d = 2, n = 1;
    Word gates;
    std::string metadata;
};
// Coordinate relabeling: old_to_new[k] is the new index of original wire k.
// Minimize the sum of input-gate target indices; an optional second circuit
// shares the same coordinate map, minimizing the combined sum.
std::vector<int> target_frequency_order(const Circuit &a, const Circuit *b = nullptr,
                                      Budget *budget = nullptr);
Circuit permute_wires(const Circuit &c, const std::vector<int> &old_to_new,
                      Budget *budget = nullptr);
Circuit read_circuit(const std::string &path);
void write_circuit(const std::string &path, const Circuit &c);
} // namespace rv
