#ifndef QUAKE_FRAME_LIMIT_DELAY_HPP
#define QUAKE_FRAME_LIMIT_DELAY_HPP

#include <optional>

namespace quake
{
  /// Holds back the frame limit a player chooses until the slider was left
  /// alone for a while: every step of it would else change how fast the
  /// menu itself is drawn while it is still being moved. What is asked for
  /// last is handed out once, a second after it was asked for, or at once
  /// when the player leaves the slider.
  class FrameLimitDelay
  {
  public:
    /// How long the slider is left alone before its limit holds.
    static constexpr double seconds = 1.0;

    /// The limit that holds already, which is not waited for.
    constexpr void Start(const int limit)
    {
      _limit = limit;
      _waiting = false;
    }

    /// A limit the player chose at a time, in seconds. The wait starts
    /// anew with every other limit; the same one again changes nothing.
    constexpr void Ask(const int limit, const double now)
    {
      if (limit == _limit) { return; }

      _limit = limit;
      _due = now + seconds;
      _waiting = true;
    }

    /// The limit to make so now, once, when its wait is over.
    [[nodiscard]] constexpr std::optional<int> Take(const double now)
    {
      if (!_waiting || now < _due) { return std::nullopt; }

      _waiting = false;
      return _limit;
    }

    /// The limit that waits, now, whatever the time: the player left the
    /// slider, so nothing is moved any more. Nothing when none waits.
    [[nodiscard]] constexpr std::optional<int> TakeNow()
    {
      if (!_waiting) { return std::nullopt; }

      _waiting = false;
      return _limit;
    }

    [[nodiscard]] constexpr bool IsWaiting() const { return _waiting; }

  private:
    int _limit = 0;
    double _due = 0.0;
    bool _waiting = false;
  };
} // quake

#endif //QUAKE_FRAME_LIMIT_DELAY_HPP
