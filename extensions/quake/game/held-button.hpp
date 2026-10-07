#ifndef QUAKE_HELD_BUTTON_HPP
#define QUAKE_HELD_BUTTON_HPP

namespace quake
{
  /// Whether a button is held, read so that a short while in which it
  /// seems up does not count. The engine's user interface takes the whole
  /// keyboard away from the game for a frame in which Tab goes down, and
  /// so for every repeat of a Tab that is held (engine issue #481): the
  /// scores, on Tab, went out for that frame at every repeat.
  ///
  /// A button let go counts as held for `grace_seconds` more, on the clock
  /// of the frames that are drawn.
  class HeldButton
  {
  public:
    /// How long a button seems up before it counts as let go: longer than
    /// a frame at any rate the game is played at, and too short to see.
    static constexpr float grace_seconds = 0.1f;

    /// Whether the button counts as held in a frame of `frame_time`
    /// seconds, in which it is down or seems up.
    constexpr bool Update(const bool is_down, const float frame_time)
    {
      if (is_down)
      {
        _is_held = true;
        _up_for = 0.0f;
        return true;
      }

      if (!_is_held) { return false; }

      _up_for += frame_time;
      if (_up_for >= grace_seconds) { _is_held = false; }
      return _is_held;
    }

  private:
    bool _is_held = false;
    float _up_for = 0.0f;
  };
} // quake

#endif //QUAKE_HELD_BUTTON_HPP
