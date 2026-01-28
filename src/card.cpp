#include "card.h"
#include <format>

namespace sharkwave {

std::string Card::toString() const {
    return std::string{} + rankChar() + suitChar();
}

char Card::rankChar() const {
    switch (rank_) {
        case Rank::Two:   return '2';
        case Rank::Three: return '3';
        case Rank::Four:  return '4';
        case Rank::Five:  return '5';
        case Rank::Six:   return '6';
        case Rank::Seven: return '7';
        case Rank::Eight: return '8';
        case Rank::Nine:  return '9';
        case Rank::Ten:   return 'T';
        case Rank::Jack:  return 'J';
        case Rank::Queen: return 'Q';
        case Rank::King:  return 'K';
        case Rank::Ace:   return 'A';
    }
    return '?';
}

char Card::suitChar() const {
    switch (suit_) {
        case Suit::Clubs:    return 'c';
        case Suit::Diamonds: return 'd';
        case Suit::Hearts:   return 'h';
        case Suit::Spades:   return 's';
    }
    return '?';
}

CardSet::CardSet() {
    clear();
}

void CardSet::clear() {
    count = 0;
}

void CardSet::add(Card card) {
    if (count < MAX_CARDS && !contains(card)) {
        cards[count++] = card;
    }
}

void CardSet::remove(Card card) {
    for (size_t i = 0; i < count; ++i) {
        if (cards[i] == card) {
            for (size_t j = i; j < count - 1; ++j) {
                cards[j] = cards[j + 1];
            }
            --count;
            return;
        }
    }
}

bool CardSet::contains(Card card) const {
    for (size_t i = 0; i < count; ++i) {
        if (cards[i] == card) {
            return true;
        }
    }
    return false;
}

} // namespace sharkwave
