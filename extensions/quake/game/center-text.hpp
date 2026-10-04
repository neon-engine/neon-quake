#ifndef QUAKE_CENTER_TEXT_HPP
#define QUAKE_CENTER_TEXT_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "hud-picture.hpp"

namespace quake
{
  /// Text in the middle of the view, which the game code asks for with
  /// `centerprint`: what a trigger says, and the story at the end of an
  /// episode. It is drawn in letters of the console, in the screen of 320
  /// by 200 that `HudPicture` describes, with `HudAnchor::Center`.
  ///
  /// The text is cut into lines at its line breaks. Each line is put
  /// around the middle from left to right by itself. A line has room for
  /// 40 letters, and what a longer one has beyond them is left out, as in
  /// the original. The first line is at 35 of 100 parts down the screen,
  /// y 70, for a text of up to four lines, and at y 48 for a longer one.
  struct CenterText
  {
    /// For how long an ordinary text shows, the original's
    /// `scr_centertime`.
    static constexpr float hold_seconds = 2.0f;

    /// How many letters a second of the story appear, the original's
    /// `scr_printspeed`.
    static constexpr float reveal_speed = 8.0f;

    /// How many letters a line has room for.
    static constexpr std::size_t line_length = 40;

    /// An ordinary text: all of it while it has been there for less than
    /// `hold_seconds`, and nothing after.
    [[nodiscard]] static std::vector<HudPicture> Layout(std::string_view text, float seconds_shown);

    /// The story at the end of an episode, which stays and appears letter
    /// after letter: one letter at once, and `speed` more each second.
    /// Spaces count as letters, line breaks do not.
    [[nodiscard]] static std::vector<HudPicture> LayoutRevealed(
      std::string_view text,
      float seconds_shown,
      float speed = reveal_speed);

    /// The first so many letters of a text, each at its place. It is what
    /// the two above are made of.
    [[nodiscard]] static std::vector<HudPicture> LayoutLetters(std::string_view text, std::size_t letters);
  };
} // quake

#endif //QUAKE_CENTER_TEXT_HPP
