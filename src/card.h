#pragma once

#include <cstdint>
#include <string>
#include <array>

namespace sharkwave {

enum class Suit : uint8_t {
    Clubs,
    Diamonds,
    Hearts,
    Spades
};

enum class Rank : uint8_t {
    Two = 2,
    Three,
    Four,
    Five,
    Six,
    Seven,
    Eight,
    Nine,
    Ten,
    Jack,
    Queen,
    King,
    Ace
};

class Card {
public:
    constexpr Card() : rank_(Rank::Two), suit_(Suit::Clubs) {}
    constexpr Card(Rank rank, Suit suit) : rank_(rank), suit_(suit) {}

    constexpr Rank rank() const { return rank_; }
    constexpr Suit suit() const { return suit_; }

    std::string toString() const;
    char rankChar() const;
    char suitChar() const;

    constexpr bool operator==(const Card& other) const {
        return rank_ == other.rank_ && suit_ == other.suit_;
    }

    constexpr bool operator!=(const Card& other) const {
        return !(*this == other);
    }

    constexpr auto operator<=>(const Card& other) const {
        return rank_ <=> other.rank_;
    }

private:
    Rank rank_;
    Suit suit_;
};

class CardSet {
public:
    static constexpr size_t MAX_CARDS = 7;

    CardSet();
    void clear();
    void add(Card card);
    void remove(Card card);
    bool contains(Card card) const;
    size_t size() const { return count; }
    bool isEmpty() const { return count == 0; }

    Card cards[MAX_CARDS];
    size_t count = 0;
};

constexpr Suit allSuits[] = {Suit::Clubs, Suit::Diamonds, Suit::Hearts, Suit::Spades};
constexpr Rank allRanks[] = {
    Rank::Two, Rank::Three, Rank::Four, Rank::Five, Rank::Six,
    Rank::Seven, Rank::Eight, Rank::Nine, Rank::Ten,
    Rank::Jack, Rank::Queen, Rank::King, Rank::Ace
};

} // namespace sharkwave
