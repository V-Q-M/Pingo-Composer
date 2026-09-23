#include "Channel.h"

#include <algorithm>

const Pattern *Channel::At(int bar) const {
    if (bar < 0 || bar >= static_cast<int>(bars.size())) {
        return nullptr;
    }

    int index = bars[static_cast<std::size_t>(bar)];

    return index >= 0 && index < static_cast<int>(patterns.size())
               ? &patterns[static_cast<std::size_t>(index)]
               : nullptr;
}

Pattern *Channel::At(int bar) {
    return const_cast<Pattern *>(static_cast<const Channel *>(this)->At(bar));
}

void Channel::Set(int bar, int pattern) {
    if (bar < 0 || bar >= static_cast<int>(bars.size())) {
        return;
    }

    bool exists = pattern >= 0 && pattern < static_cast<int>(patterns.size());

    bars[static_cast<std::size_t>(bar)] = exists ? pattern : EMPTY;
}

int Channel::Reserve(int pattern) {
    int wanted = std::max(pattern, 0);

    while (static_cast<int>(patterns.size()) <= wanted) {
        patterns.push_back(Pattern{});
    }

    return wanted;
}

int Channel::BarsOf(int pattern) const {
    if (pattern < 0 || pattern >= static_cast<int>(patterns.size())) {
        return 0;
    }

    const int stepsPerBar = Pattern::STEPS_PER_BEAT * Pattern::BEATS_PER_BAR;

    return std::max((patterns[static_cast<std::size_t>(pattern)].Length() + stepsPerBar - 1) / stepsPerBar, 1);
}

int Channel::StartOf(int bar) const {
    if (bar < 0 || bar >= static_cast<int>(bars.size())) {
        return EMPTY;
    }

    // Blocks never lie on top of each other, so the first one found towards
    // the left that is long enough is the one this bar belongs to
    for (int start = bar; start >= 0; start--) {
        int pattern = bars[static_cast<std::size_t>(start)];

        if (pattern >= 0 && start + BarsOf(pattern) > bar) {
            return start;
        }
    }

    return EMPTY;
}
