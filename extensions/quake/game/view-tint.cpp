#include "view-tint.hpp"

#include <algorithm>

#include "qc-item.hpp"

namespace quake
{
  void ViewTint::Hurt(const float taken, const float saved)
  {
    // how hard the hit was, of which no hit counts for less than 10
    const float count = std::max(taken * 0.5f + saved * 0.5f, 10.0f);

    // three parts of a hundred for each, on top of what is there, and never
    // more than 150 of them
    _damage.amount = std::clamp(_damage.amount * 100.0f + 3.0f * count, 0.0f, 150.0f) / 100.0f;

    if (saved > taken) { _damage.red = 200.0f, _damage.green = 100.0f, _damage.blue = 100.0f; }
    else if (saved > 0.0f) { _damage.red = 220.0f, _damage.green = 50.0f, _damage.blue = 50.0f; }
    else { _damage.red = 255.0f, _damage.green = 0.0f, _damage.blue = 0.0f; }
  }

  void ViewTint::PickUp()
  {
    _bonus = {215.0f, 186.0f, 69.0f, 0.5f};
  }

  void ViewTint::SetContents(const BspContents contents)
  {
    switch (contents)
    {
      case BspContents::Lava: _contents = {255.0f, 80.0f, 0.0f, 150.0f / 255.0f};
        break;
      case BspContents::Slime: _contents = {0.0f, 25.0f, 5.0f, 150.0f / 255.0f};
        break;
      case BspContents::Water: _contents = {130.0f, 80.0f, 50.0f, 128.0f / 255.0f};
        break;
      default: _contents = {};
        break;
    }
  }

  void ViewTint::SetItems(const std::uint32_t items)
  {
    if (HasItem(items, QcItem::Quad)) { _powerup = {0.0f, 0.0f, 255.0f, 30.0f / 255.0f}; }
    else if (HasItem(items, QcItem::Suit)) { _powerup = {0.0f, 255.0f, 0.0f, 20.0f / 255.0f}; }
    else if (HasItem(items, QcItem::Invisibility)) { _powerup = {100.0f, 100.0f, 100.0f, 100.0f / 255.0f}; }
    else if (HasItem(items, QcItem::Invulnerability)) { _powerup = {255.0f, 255.0f, 0.0f, 30.0f / 255.0f}; }
    else { _powerup = {}; }
  }

  void ViewTint::Advance(const float seconds)
  {
    if (seconds <= 0.0f) { return; }

    _damage.amount = std::max(_damage.amount - seconds * damage_fade / 100.0f, 0.0f);
    _bonus.amount = std::max(_bonus.amount - seconds * bonus_fade / 100.0f, 0.0f);
  }

  void ViewTint::Clear()
  {
    _damage = {};
    _bonus = {};
  }

  ViewTint::Colour ViewTint::Mix() const
  {
    // one over the other: each takes its share of what is left to see
    Colour mixed;
    for (const Colour &layer : {_contents, _damage, _bonus, _powerup})
    {
      const float share = std::clamp(layer.amount, 0.0f, 1.0f);
      if (share <= 0.0f) { continue; }

      mixed.amount = mixed.amount + share * (1.0f - mixed.amount);
      const float part = share / mixed.amount;
      mixed.red = mixed.red * (1.0f - part) + layer.red * part;
      mixed.green = mixed.green * (1.0f - part) + layer.green * part;
      mixed.blue = mixed.blue * (1.0f - part) + layer.blue * part;
    }
    return mixed;
  }
} // quake
