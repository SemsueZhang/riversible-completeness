#include "rv/compiler.hpp"
namespace rv {
std::size_t Compiler::extract_block(Builder &b, std::size_t pos, std::size_t length, int i) {
    // FXL Algorithm 3, bounded indices and its written leftmost redex priority.
    require(length >= 1 && indices.at(b.word.at(pos)) == i, "extract input");
    std::size_t emitted = 0, tail = length - 1;
    for (;;) {
        budget.tick(b.word.size());
        std::size_t begin = pos + emitted + 1;
        bool chain = true;
        for (std::size_t k = 0; k < tail; ++k)
            if (indices.at(b.word[begin + k]) != i + 1 + int(k)) {
                chain = false;
                break;
            }
        if (chain)
            return tail + 1;
        if (tail && indices.at(b.word[begin]) >= i + 2) {
            commute(b, begin - 1);
            ++emitted;
            --tail;
            continue;
        }
        bool changed = false;
        for (std::size_t k = 0; k < tail; ++k) {
            std::size_t at = begin + k;
            int x = indices.at(b.word[at]);
            if (k + 1 < tail && b.word[at] == b.word[at + 1]) {
                cancel(b, at);
                tail -= 2;
                changed = true;
                break;
            }
            if (k + 2 < tail && b.word[at] == b.word[at + 2] &&
                indices.at(b.word[at + 1]) == x + 1) {
                b.call(braid(b.word[at], b.word[at + 1]), at);
                changed = true;
                break;
            }
            if (k + 1 < tail && std::abs(x - indices.at(b.word[at + 1])) >= 2) {
                commute(b, at);
                changed = true;
                break;
            }
        }
        require(changed, "block extraction has no legal redex");
    }
}
P Compiler::normalize_path(const Word &input) {
    if (method == Method::FL2025 || method == Method::FXL2026)
        return indexed_normalize_path(input);
    Builder b(budget, input);
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
            std::size_t old = b.word.size();
            std::size_t len = extract_block(b, first, second - first, i);
            active -= old - b.word.size();
            std::size_t pos = first;
            while (indices.at(b.word[pos]) != i)
                ++pos;
            if (len == 1) {
                cancel(b, pos);
                active -= 2;
            } else {
                for (std::size_t at = pos + len - 1; at > pos + 1; --at)
                    commute(b, at);
                b.call(braid(b.word[pos], b.word[pos + 1]), pos);
            }
        }
        std::size_t pos = 0;
        while (pos < active && indices.at(b.word[pos]) != i)
            ++pos;
        if (pos < active) {
            std::size_t old = b.word.size();
            std::size_t len = extract_block(b, pos, active - pos, i);
            active -= old - b.word.size();
            active -= len;
        }
    }
    require(active == 0, "unextracted normal-form prefix");
    return b.finish();
}
Word Compiler::insertion_normal_form(const Word &input) {
    std::vector<int> image(path.size()), current(path.size());
    std::iota(current.begin(), current.end(), 0);
    std::map<State, int> rank;
    for (std::size_t r = 0; r < path.size(); ++r)
        rank[path[r]] = int(r);
    for (std::size_t r = 0; r < path.size(); ++r) {
        budget.tick();
        auto x = path[r];
        for (auto &g : input) {
            budget.tick();
            x = apply(g, std::move(x));
        }
        image[r] = rank.at(x);
    }
    std::vector<Word> blocks(path.size());
    std::size_t total = 0;
    for (std::size_t r = 0; r < path.size(); ++r) {
        budget.tick();
        auto it = std::find(current.begin() + r, current.end(), image[r]);
        require(it != current.end(), "nonpermutation image");
        std::size_t l = it - current.begin();
        total += l - r;
        budget.tick(total);
        std::rotate(current.begin() + r, it, it + 1);
        for (std::size_t j = r; j < l; ++j)
            blocks[r].push_back(path_gates[j]);
    }
    Word nf;
    nf.reserve(total);
    for (std::size_t r = blocks.size(); r > 0; --r)
        nf.insert(nf.end(), blocks[r - 1].begin(), blocks[r - 1].end());
    return nf;
}
} // namespace rv
