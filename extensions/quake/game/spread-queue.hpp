#ifndef QUAKE_SPREAD_QUEUE_HPP
#define QUAKE_SPREAD_QUEUE_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <utility>
#include <vector>

namespace quake
{
  /// Things that all come due at one moment and are handed on a few in
  /// each frame, over a short while, in place of all at once.
  ///
  /// The lights of a level tick ten times a second, and at a tick dozens of
  /// pictures of light have to go to the renderer again. All in one frame,
  /// that frame is late, ten times a second. Spread over the frames of the
  /// next 80 milliseconds, each frame carries a few, and a light flickers
  /// that much later than its tick, which nobody sees.
  ///
  /// A thing is two numbers, which model and which of its pictures. One
  /// that still waits when it comes due again waits on, once: what is
  /// handed on is whatever it holds by then.
  class SpreadQueue
  {
  public:
    using Item = std::pair<std::size_t, std::size_t>;

    /// Over how long what came due is handed on, in seconds: within a tick
    /// of the lights, which is a tenth of a second.
    static constexpr double seconds = 0.08;

    /// How many are handed on in a frame at the least, so that it ends also
    /// when no time passes.
    static constexpr std::size_t least = 2;

    /// Something came due. Nothing changes when it waits already.
    void Add(const Item &item)
    {
      if (std::ranges::find(_waiting, item) != _waiting.end()) { return; }

      _waiting.push_back(item);
      _due = std::max(_due, _waiting.size());
    }

    /// What is handed on in a frame that lasts `frame_seconds`: its share of
    /// what came due, the oldest first.
    [[nodiscard]] std::vector<Item> Take(const double frame_seconds)
    {
      if (_waiting.empty())
      {
        _due = 0;
        return {};
      }

      const double share = frame_seconds > 0.0 ? std::min(frame_seconds / seconds, 1.0) : 0.0;
      const auto wanted = static_cast<std::size_t>(std::ceil(static_cast<double>(_due) * share));
      const std::size_t count = std::min(std::max(wanted, least), _waiting.size());

      std::vector<Item> taken(_waiting.begin(), _waiting.begin() + static_cast<std::ptrdiff_t>(count));
      _waiting.erase(_waiting.begin(), _waiting.begin() + static_cast<std::ptrdiff_t>(count));
      if (_waiting.empty()) { _due = 0; }
      return taken;
    }

    [[nodiscard]] std::size_t GetWaiting() const { return _waiting.size(); }

    void Clear()
    {
      _waiting.clear();
      _due = 0;
    }

  private:
    std::deque<Item> _waiting;

    // how many came due together, which is what a frame takes its share of
    std::size_t _due = 0;
  };
} // quake

#endif //QUAKE_SPREAD_QUEUE_HPP
