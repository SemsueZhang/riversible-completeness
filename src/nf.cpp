#include "rv/compiler.hpp"
namespace rv {
P Compiler::coordinated(const Gate &g) {
    return memo(key("coordinated", {g}), [&] {
        Builder b(budget, {g});
        if (indices.count(g))
            return b.finish();
        auto ex = gray_expand(g);
        b.call(ex);
        int t = g.target;
        State prefix(t, 0);
        if (t && g.lower % 2 == 0)
            prefix = gray(d, t).back();
        std::size_t pos = 0;
        for (const auto &child : ex->after) {
            auto h = child;
            for (int k = 0; k < t; ++k)
                h.controls[k] = prefix[k];
            auto tr = transport(child, h);
            b.call(tr, pos);
            pos += tr->after.size();
        }
        reduce_aux(b);
        return b.finish();
    });
}
} // namespace rv
