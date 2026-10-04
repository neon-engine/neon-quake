#ifndef QUAKE_SERVER_MESSAGE_READER_HPP
#define QUAKE_SERVER_MESSAGE_READER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "qc-message-destination.hpp"
#include "qc-message-value.hpp"
#include "server-message-listener.hpp"
#include "server-message-target.hpp"

namespace quake
{
  /// Makes sense of what the game code writes for the client's side of the
  /// engine. The game code writes a message of the network a part at a
  /// time, with `WriteByte` to `WriteEntity`, see QcHost::WriteMessage().
  /// This is handed every part as it is written, puts the parts together as
  /// version 15 of the protocol has the messages, and tells a listener of
  /// each whole message, see `ServerMessageListener`. The messages it knows
  /// are those of `ServerMessageKind`.
  ///
  /// Each destination has its messages apart from the others, as each has a
  /// buffer of its own in the original: a message to one may be written
  /// while one to another is under way. The messages to one player share
  /// one place, since the game code writes a message whole before it turns
  /// to the next player. A message is whole with the last part its kind
  /// needs, and the listener is told within that call.
  ///
  /// What is written is never trusted:
  /// - A part of the wrong kind, such as a string where a coordinate is to
  ///   come, breaks the message. The listener is told, and the reader
  ///   starts over with that very part, which may start a message itself.
  /// - A first byte that no message is known for is told to the listener.
  ///   How long such a message is cannot be known, so everything is then
  ///   skipped until a byte comes that starts a message that is known. A
  ///   part of the unknown message may be taken for that byte.
  /// - A value that starts no message, since it is no byte, is told to the
  ///   listener as broken, and skipped in the same way. The listener is
  ///   told once for all that is skipped in a row.
  /// Nothing grows: of a message under way only its numbers are kept, of
  /// which no message has more than `max_numbers`, and a text is the last
  /// part of its message and is handed on at once.
  class ServerMessageReader final
  {
  public:
    /// The most numbers a message has, those of a beam: its kind, and three
    /// coordinates each for where it starts and ends.
    static constexpr std::size_t max_numbers = 7;

  private:
    /// A message under way to one destination.
    struct Pending
    {
      /// Whether a message was started and is not whole yet.
      bool is_in_message = false;

      /// Whether what comes is skipped until a message starts that is known.
      bool is_skipping = false;

      std::int32_t first_byte = 0;

      /// The player the message is for when the destination is One.
      std::int32_t client = 0;

      /// How many parts came after the first byte.
      std::size_t part_count = 0;

      /// The parts that are numbers, in the order they came.
      std::array<float, max_numbers> numbers{};
      std::size_t number_count = 0;

      /// The part that is an entity. No message has two.
      std::int32_t entity = 0;
    };

    static constexpr std::size_t destination_count = 4;

    ServerMessageListener &_listener;
    std::array<Pending, destination_count> _pending{};

    /// Takes a value for the first of a message: a byte that starts a
    /// message that is known starts it, anything else is skipped.
    void Start(Pending &pending, const ServerMessageTarget &target, const QcMessageValue &value);

    /// Tells the listener of a message that did not come as its kind has
    /// it, and forgets it.
    void Break(Pending &pending, const ServerMessageTarget &target, std::string_view why);

    /// Tells the listener of a message that is whole, and forgets it. The
    /// text is that of the last part, for a message that ends with one.
    void Deliver(Pending &pending, const ServerMessageTarget &target, std::string_view text);

  public:
    /// The listener has to outlive this.
    explicit ServerMessageReader(ServerMessageListener &listener);

    ServerMessageReader(const ServerMessageReader &) = delete;

    ServerMessageReader &operator=(const ServerMessageReader &) = delete;

    /// Takes one part of a message, with what QcHost::WriteMessage() was
    /// called with. The listener is told before this returns of a message
    /// the part made whole, and of one it broke.
    void Read(QcMessageDestination destination, std::int32_t client, const QcMessageValue &value);

    /// Whether a message to a destination was started and is not whole yet.
    [[nodiscard]] bool IsInMessage(QcMessageDestination destination) const;

    /// Ends what is under way: a message that is not whole is told to the
    /// listener as broken, and nothing is skipped any more. For when no
    /// part can follow: the level is over, or, as the game code writes a
    /// message whole within one call of a function of its own, any time
    /// the game code does not run.
    void Flush();
  };
} // quake

#endif //QUAKE_SERVER_MESSAGE_READER_HPP
