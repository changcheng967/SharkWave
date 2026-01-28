#include "deck.h"
#include <algorithm>
#include <random>

namespace sharkwave {

Deck::Deck() : position_(0), rng_(std::random_device{}()) {
    reset();
}

void Deck::reset() {
    size_t idx = 0;
    for (Suit s : allSuits) {
        for (Rank r : allRanks) {
            cards_[idx++] = Card(r, s);
        }
    }
    position_ = 0;
}

void Deck::shuffle() {
    std::shuffle(cards_.begin(), cards_.end(), rng_);
    position_ = 0;
}

Card Deck::deal() {
    if (position_ >= 52) {
        return Card(); // Return empty card if deck exhausted
    }
    return cards_[position_++];
}

} // namespace sharkwave
