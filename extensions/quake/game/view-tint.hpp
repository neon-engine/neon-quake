#ifndef QUAKE_VIEW_TINT_HPP
#define QUAKE_VIEW_TINT_HPP

#include <array>
#include <cstdint>

#include "formats/bsp-contents.hpp"

namespace quake
{
  /// The colour the original lays over everything a player sees: red for a
  /// moment when hurt, gold when something is picked up, the colour of the
  /// water, the slime, or the lava the eyes are in, and that of what the
  /// player carries, the quad, the suit, the ring, the pentagram.
  ///
  /// Each of them is a colour and how much of it there is. Those of a hit
  /// and of a pickup fade by themselves. All of them are mixed into one,
  /// one over the other, as the original does it.
  class ViewTint final
  {
  public:
    /// A colour, each part from 0 to 255, and how much of it is laid over
    /// what is seen, from 0 for none to 1 for nothing else.
    struct Colour
    {
      float red = 0.0f;
      float green = 0.0f;
      float blue = 0.0f;
      float amount = 0.0f;
    };

  private:
    Colour _damage;
    Colour _bonus;
    Colour _contents;
    Colour _powerup;

  public:
    /// How fast the red of a hit and the gold of a pickup fade, in parts of
    /// a hundred a second.
    static constexpr float damage_fade = 150.0f;
    static constexpr float bonus_fade = 100.0f;

    /// A hit: how much the body took and how much the armour, as the game
    /// code counts them. A hit the armour took alone is brownish, one the
    /// body took alone red.
    void Hurt(float taken, float saved);

    /// Something was picked up: a flash of gold.
    void PickUp();

    /// What the eyes of the player are in.
    void SetContents(BspContents contents);

    /// What the player carries, as the bits of the field `items`.
    void SetItems(std::uint32_t items);

    /// Lets `seconds` pass: the flashes fade.
    void Advance(float seconds);

    /// Forgets the flashes, for a new level.
    void Clear();

    /// The one colour all of it comes to.
    [[nodiscard]] Colour Mix() const;
  };
} // quake

#endif //QUAKE_VIEW_TINT_HPP
