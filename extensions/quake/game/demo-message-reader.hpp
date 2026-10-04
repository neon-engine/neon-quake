#ifndef QUAKE_DEMO_MESSAGE_READER_HPP
#define QUAKE_DEMO_MESSAGE_READER_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "demo-message-handler.hpp"
#include "demo-protocol.hpp"
#include "formats/byte-reader.hpp"

namespace quake
{
  /// Reads what a server sent its client: the bytes of a packet, which
  /// are messages one after the other, each a byte for its kind, see
  /// `DemoMessageKind`, and what that kind carries. A handler is told of
  /// every message, see `DemoMessageHandler`.
  ///
  /// It reads the protocol of the original, 15, that of FitzQuake, 666,
  /// and that of RMQ, 999, see `DemoProtocol`. Which one the bytes are in
  /// is said by the message that begins a level, and is remembered for the
  /// packets that follow, so one reader has to read all packets of a
  /// recording in their order.
  ///
  /// The bytes are never trusted. A message does not say how long it is,
  /// so after one that cannot be read nothing of the packet can be, and
  /// nothing of the packets after it: Read() then says what was wrong,
  /// and the recording is at its end. That is so for a first byte no
  /// message is known for, a packet that ends in the middle of a message,
  /// and a value that cannot be, such as a protocol that is not known.
  ///
  /// This is not `ServerMessageReader`, which puts together the few
  /// messages the game code writes a value at a time, in a running game.
  class DemoMessageReader final
  {
  public:
    /// The most letters of a text that are kept, as in the original. The
    /// rest of a longer text is read and dropped.
    static constexpr std::size_t max_text_size = 2047;

    /// The most names of models, and of sounds, a level may have.
    static constexpr std::size_t max_names = 32768;

  private:
    DemoProtocol _protocol{};

    /// For how many players the server is, to check the scoreboard with.
    std::int32_t _max_clients = 0;

    /// The text that was read last, kept here so that reading one makes
    /// nothing new.
    std::string _text;

    float ReadCoord(ByteReader &reader) const;

    float ReadAngle(ByteReader &reader) const;

    LevelVector ReadPlace(ByteReader &reader) const;

    /// Reads a text up to its zero into `_text`.
    const std::string &ReadText(ByteReader &reader);

    bool ReadServerInfo(ByteReader &reader, DemoMessageHandler &handler, std::string &problem);

    void ReadSound(ByteReader &reader, DemoMessageHandler &handler) const;

    void ReadClientData(ByteReader &reader, DemoMessageHandler &handler) const;

    bool ReadEntityUpdate(
      ByteReader &reader, std::uint32_t first_byte, DemoMessageHandler &handler, std::string &problem) const;

    /// Reads the state of a baseline or of a static entity. The newer
    /// form has a byte of flags first.
    DemoEntityState ReadEntityState(ByteReader &reader, bool has_flags) const;

    bool ReadTempEntity(ByteReader &reader, DemoMessageHandler &handler, std::string &problem) const;

    /// Reads the number of a player of the scoreboard. False for one the
    /// server has no place for.
    bool ReadPlayer(ByteReader &reader, std::int32_t &player, std::string &problem) const;

    /// Reads one message that is no update of an entity. `is_over` is set
    /// by the message that ends everything.
    bool ReadMessage(
      ByteReader &reader, std::int32_t kind, DemoMessageHandler &handler, bool &is_over, std::string &problem);

  public:
    /// Reads the messages of one packet and tells the handler of each.
    ///
    /// False, with the reason in `problem`, when a message could not be
    /// read. The handler was told of the messages before it, and no packet
    /// can be read after it.
    bool Read(std::span<const std::uint8_t> message, DemoMessageHandler &handler, std::string &problem);

    /// The protocol the last level began with. That of the original
    /// before any did.
    [[nodiscard]] const DemoProtocol &GetProtocol() const;

    /// Forgets the protocol, for a new recording.
    void Reset();
  };
} // quake

#endif //QUAKE_DEMO_MESSAGE_READER_HPP
