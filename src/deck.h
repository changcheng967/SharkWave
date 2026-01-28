#pragma once

#include "card.h"
#include <array>
#include <random>

namespace sharkwave {

class Deck {
public:
    Deck();
    void shuffle();
    Card deal();
    void reset();
    size_t cardsRemaining() const { return 52 - position_; }

private:
    std::array<Card, 52> cards_;
    size_t position_;
    std::mt19937 rng_;
};

} // namespace sharkwave
