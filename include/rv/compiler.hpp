#pragma once
#include "rv/core.hpp"
#include <unordered_map>
namespace rv {
class Compiler {
  public:
    int d, n;
    Method method;
    Budget &budget;
    std::vector<State> path;
    Word path_gates;
    std::map<Gate, int> indices;
    Compiler(int d, int n, Method method, Budget &budget);
    P route(const Word &input);
    P normalize_path(const Word &input); // FL Lemmas 4/5; FXL Algorithms 2--4
    struct Scan {
        P proof;
        bool identity;
    };
    Scan cancel_path(const Word &input); // paper Algorithm 2, also rejects nonidentity
    Word insertion_normal_form(const Word &input);

  private:
    std::unordered_map<std::string, P> cache;
    std::size_t cached_gates = 0;
    P memo(const std::string &key, const std::function<P()> &fn);
    std::string key(const std::string &name, const Word &w) const;
    Word pword(const Gate &g) const;
    Word norm8(const Gate &g) const;
    void cancel(Builder &b, std::size_t pos);
    void commute(Builder &b, std::size_t pos);
    void insert_pairs(Builder &b, std::size_t pos, const Word &w);
    P expand(const Gate &g, std::vector<int> lines = {});
    P gray_expand(const Gate &g);
    P rule8(const Gate &g);
    P controlled_flip(const Gate &u, const Gate &g, const Gate &h);
    P unconditional_flip(const Gate &g, int p);
    P transport(const Gate &g, const Gate &h);
    P toggle(const Gate &u, const Gate &g);
    P unconditional_commute(const Gate &u, const Gate &v);
    void remove_fibers(Builder &b, std::size_t start, std::size_t count, const Gate &match);
    P local_conjugation(const Word &base, const Gate &inner, const Gate &outer, int rule);
    P conjugate_word(const Word &word, const Word &toggles);
    P braid(const Gate &x, const Gate &y);
    P square(const Gate &g, const State &next_prefix);
    P grid_route(const Gate &g);
    P fl_route(const Gate &g);
    P fl_reverse_chain(const Word &half); // FL Lemma 3
    P fl_x_swap(const Gate &x, const Gate &g); // derived Rules 7/9
    P fl_polarize(const Word &word, const std::vector<int> &lines);
    P fl_rule5_segment(const Gate &a, const Gate &z, const Word &middle);
    P fl_reduce_pair(const Word &word, int i); // FL Lemma 4
    std::size_t fl_extract_block(Builder &b, std::size_t pos, std::size_t length, int i);
    P fl_normalize_path(const Word &input); // FL Proposition 3 / Lemmas 4--5
    P fxl_route(const Gate &g);
    P indexed_normalize_path(const Word &input);
    P coordinated(const Gate &g);
    void reduce_aux(Builder &b);
    void cancel_exposed(Builder &b);
    void sort_x(Builder &b, std::size_t start, std::size_t end);
    std::size_t extract_block(Builder &b, std::size_t pos, std::size_t length, int i);
};
struct Result {
    bool equivalent = true;
    Count rule_count = 0;
};
Result normalize(const Circuit &a, Method method, Budget &budget);
Result equivalent(const Circuit &a, const Circuit &b, Method method, Budget &budget);
} // namespace rv
