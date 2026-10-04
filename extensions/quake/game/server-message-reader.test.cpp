#include "server-message-reader.hpp"

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "qc-message-destination.hpp"
#include "qc-message-kind.hpp"
#include "qc-message-value.hpp"
#include "server-message-recording-listener.test.hpp"

// The tests of ServerMessageReader with values made here, written a part at
// a time as the game code writes them.
namespace
{
  using quake::QcMessageDestination;
  using quake::QcMessageKind;
  using quake::QcMessageValue;
  using quake::ServerMessageReader;
  using quake::ServerMessageRecordingListener;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;
  using ::testing::SizeIs;

  class ServerMessageReaderTest : public ::testing::Test
  {
  protected:
    static constexpr QcMessageDestination broadcast = QcMessageDestination::Broadcast;
    static constexpr QcMessageDestination one = QcMessageDestination::One;
    static constexpr QcMessageDestination all = QcMessageDestination::All;

    ServerMessageRecordingListener _listener;
    ServerMessageReader _reader{_listener};

    static QcMessageValue Byte(const float number)
    {
      return {.kind = QcMessageKind::Byte, .number = number};
    }

    static QcMessageValue Coord(const float number)
    {
      return {.kind = QcMessageKind::Coord, .number = number};
    }

    static QcMessageValue Angle(const float number)
    {
      return {.kind = QcMessageKind::Angle, .number = number};
    }

    static QcMessageValue Long(const float number)
    {
      return {.kind = QcMessageKind::Long, .number = number};
    }

    static QcMessageValue Short(const float number)
    {
      return {.kind = QcMessageKind::Short, .number = number};
    }

    static QcMessageValue String(const std::string_view text)
    {
      return {.kind = QcMessageKind::String, .text = text};
    }

    static QcMessageValue Entity(const std::int32_t entity)
    {
      return {.kind = QcMessageKind::Entity, .entity = entity};
    }

    /// Writes values to a destination one after the other.
    void Write(
      const QcMessageDestination destination, const std::initializer_list<QcMessageValue> values,
      const std::int32_t client = 0)
    {
      for (const QcMessageValue &value : values) { _reader.Read(destination, client, value); }
    }
  };

  TEST_F(ServerMessageReaderTest, TellsOfATempEntityAtAPlaceWithItsLastCoordinate)
  {
    Write(broadcast, {Byte(23), Byte(3), Coord(1.5f), Coord(-2.0f)});
    EXPECT_THAT(_listener.messages, IsEmpty());
    EXPECT_TRUE(_reader.IsInMessage(broadcast));

    Write(broadcast, {Coord(300.25f)});
    EXPECT_THAT(_listener.messages, ElementsAre("point 3 to 0 for 0: 1.5 -2 300.25"));
    EXPECT_FALSE(_reader.IsInMessage(broadcast));
  }

  TEST_F(ServerMessageReaderTest, TellsOfEveryKindOfTempEntityAtAPlace)
  {
    for (const int kind : {0, 1, 2, 3, 4, 7, 8, 10, 11})
    {
      Write(broadcast, {Byte(23), Byte(static_cast<float>(kind)), Coord(1.0f), Coord(2.0f), Coord(3.0f)});
    }
    EXPECT_THAT(_listener.messages, ElementsAre(
                  "point 0 to 0 for 0: 1 2 3", "point 1 to 0 for 0: 1 2 3", "point 2 to 0 for 0: 1 2 3",
                  "point 3 to 0 for 0: 1 2 3", "point 4 to 0 for 0: 1 2 3", "point 7 to 0 for 0: 1 2 3",
                  "point 8 to 0 for 0: 1 2 3", "point 10 to 0 for 0: 1 2 3", "point 11 to 0 for 0: 1 2 3"));
  }

  TEST_F(ServerMessageReaderTest, TellsOfABeamWithItsEntityItsStartAndItsEnd)
  {
    for (const int kind : {5, 6, 9, 13})
    {
      Write(broadcast, {
              Byte(23), Byte(static_cast<float>(kind)), Entity(7), Coord(1.0f), Coord(2.0f), Coord(3.0f), Coord(4.0f),
              Coord(5.0f),
            });
      EXPECT_TRUE(_reader.IsInMessage(broadcast));
      Write(broadcast, {Coord(6.0f)});
    }
    EXPECT_THAT(_listener.messages, ElementsAre(
                  "beam 5 to 0 for 0: entity 7 from 1 2 3 to 4 5 6", "beam 6 to 0 for 0: entity 7 from 1 2 3 to 4 5 6",
                  "beam 9 to 0 for 0: entity 7 from 1 2 3 to 4 5 6",
                  "beam 13 to 0 for 0: entity 7 from 1 2 3 to 4 5 6"));
  }

  TEST_F(ServerMessageReaderTest, TellsOfAnExplosionWithItsTwoColours)
  {
    Write(broadcast, {Byte(23), Byte(12), Coord(1.0f), Coord(2.0f), Coord(3.0f), Byte(230.0f)});
    EXPECT_THAT(_listener.messages, IsEmpty());
    Write(broadcast, {Byte(5.0f)});
    EXPECT_THAT(_listener.messages, ElementsAre("explosion to 0 for 0: 1 2 3, colours 230 and 5"));
  }

  TEST_F(ServerMessageReaderTest, TellsOfAMessageThatIsOnlyItsFirstByteAtOnce)
  {
    Write(all, {Byte(30)});
    Write(all, {Byte(33)});
    Write(all, {Byte(27)});
    Write(all, {Byte(28)});
    EXPECT_THAT(_listener.messages, ElementsAre(
                  "intermission to 2 for 0", "sellscreen to 2 for 0", "killedmonster to 2 for 0",
                  "foundsecret to 2 for 0"));
    EXPECT_FALSE(_reader.IsInMessage(all));
  }

  TEST_F(ServerMessageReaderTest, TellsOfAMessageWithATextWithTheText)
  {
    Write(all, {Byte(31)});
    EXPECT_THAT(_listener.messages, IsEmpty());
    Write(all, {String("The end.")});
    Write(all, {Byte(34), String("A camera.")});
    Write(one, {Byte(26), String("In the middle.")}, 1);
    Write(one, {Byte(8), String("A line.\n")}, 1);
    Write(one, {Byte(9), String("bf\n")}, 1);
    EXPECT_THAT(_listener.messages, ElementsAre(
                  "finale to 2 for 0: The end.", "cutscene to 2 for 0: A camera.",
                  "centerprint to 1 for 1: In the middle.", "print to 1 for 1: A line.\n",
                  "stufftext to 1 for 1: bf\n"));
  }

  TEST_F(ServerMessageReaderTest, TellsOfTheMusicAStatTheAnglesAndTheView)
  {
    Write(all, {Byte(32), Byte(3), Byte(3)});
    Write(all, {Byte(3), Byte(14), Long(100000.0f)});
    Write(one, {Byte(10), Angle(10.0f), Angle(90.0f), Angle(0.0f)}, 1);
    Write(one, {Byte(5), Entity(42)}, 1);
    // a short is what the network carries for an entity
    Write(one, {Byte(5), Short(43.0f)}, 1);
    EXPECT_THAT(_listener.messages, ElementsAre(
                  "cdtrack to 2 for 0: 3 then 3", "updatestat to 2 for 0: 14 is 100000",
                  "setangle to 1 for 1: 10 90 0", "setview to 1 for 1: 42", "setview to 1 for 1: 43"));
    EXPECT_THAT(_listener.problems, IsEmpty());
  }

  TEST_F(ServerMessageReaderTest, ReadsTwoMessagesBackToBack)
  {
    Write(all, {Byte(32), Byte(2), Byte(4), Byte(30), Byte(23), Byte(11), Coord(1.0f), Coord(2.0f), Coord(3.0f)});
    EXPECT_THAT(_listener.messages, ElementsAre(
                  "cdtrack to 2 for 0: 2 then 4", "intermission to 2 for 0", "point 11 to 2 for 0: 1 2 3"));
  }

  TEST_F(ServerMessageReaderTest, KeepsTheMessagesOfTwoDestinationsApart)
  {
    Write(broadcast, {Byte(23), Byte(3), Coord(1.0f)});
    Write(all, {Byte(31)});
    Write(broadcast, {Coord(2.0f)});
    Write(one, {Byte(26)}, 1);
    Write(all, {String("The end.")});
    Write(broadcast, {Coord(3.0f)});
    Write(one, {String("Hello.")}, 1);

    EXPECT_THAT(_listener.messages, ElementsAre(
                  "finale to 2 for 0: The end.", "point 3 to 0 for 0: 1 2 3", "centerprint to 1 for 1: Hello."));
    EXPECT_THAT(_listener.problems, IsEmpty());
  }

  TEST_F(ServerMessageReaderTest, BreaksAMessageToOnePlayerWhenAnotherPlayerIsWrittenTo)
  {
    Write(one, {Byte(10), Angle(1.0f)}, 1);
    Write(one, {Byte(26), String("Hello.")}, 2);
    EXPECT_THAT(_listener.messages, ElementsAre(HasSubstr("broken to 1 for 1: 10"), "centerprint to 1 for 2: Hello."));
  }

  TEST_F(ServerMessageReaderTest, BreaksAMessageWithAPartOfTheWrongKindAndStartsOver)
  {
    Write(broadcast, {Byte(23), Byte(3), Coord(1.0f), String("no coordinate"), Coord(2.0f), Coord(3.0f)});
    ASSERT_THAT(_listener.messages, SizeIs(1));
    EXPECT_THAT(_listener.messages[0], HasSubstr("broken to 0 for 0: 23, a string came where a coord is to come"));
    EXPECT_FALSE(_reader.IsInMessage(broadcast));

    // what followed was skipped without a word, and the next message is read
    Write(broadcast, {Byte(27)});
    EXPECT_THAT(_listener.messages, ElementsAre(::testing::_, "killedmonster to 0 for 0"));
  }

  TEST_F(ServerMessageReaderTest, TakesTheByteThatBreaksAMessageForTheStartOfTheNext)
  {
    // a temp entity that ends after one coordinate, and a whole one
    Write(broadcast, {Byte(23), Byte(3), Coord(1.0f)});
    Write(broadcast, {Byte(23), Byte(2), Coord(4.0f), Coord(5.0f), Coord(6.0f)});
    EXPECT_THAT(_listener.messages, ElementsAre(HasSubstr("broken to 0 for 0: 23"), "point 2 to 0 for 0: 4 5 6"));
  }

  TEST_F(ServerMessageReaderTest, TellsOfAMessageItDoesNotKnowAndSkipsToTheNextItKnows)
  {
    // 19 is the damage a player took, which the game code never writes
    Write(all, {Byte(19), Byte(200), Byte(100), Coord(1.0f), Coord(2.0f), Coord(3.0f), Byte(30)});
    EXPECT_THAT(_listener.messages, ElementsAre("unknown to 2 for 0: 19", "intermission to 2 for 0"));
  }

  TEST_F(ServerMessageReaderTest, TellsOfATempEntityOfAKindItDoesNotKnow)
  {
    Write(broadcast, {Byte(23), Byte(14), Coord(1.0f), Coord(2.0f), Coord(3.0f), Byte(28)});
    EXPECT_THAT(_listener.messages, ElementsAre(
                  "broken to 0 for 0: 23, a temp entity of kind 14 is not known", "foundsecret to 0 for 0"));
  }

  TEST_F(ServerMessageReaderTest, TellsOnceOfValuesThatStartNoMessage)
  {
    Write(all, {String("lost"), Coord(1.0f), Entity(3), Long(9.0f), Angle(1.0f), Short(2.0f)});
    EXPECT_THAT(_listener.messages, ElementsAre("broken to 2 for 0: -1, a string came where a message is to start"));

    Write(all, {Byte(28)});
    Write(all, {Coord(1.0f)});
    EXPECT_THAT(_listener.messages, SizeIs(3));
    EXPECT_EQ(_listener.counts["foundsecret"], 1u);
    EXPECT_EQ(_listener.counts["broken"], 2u);
  }

  TEST_F(ServerMessageReaderTest, StaysSmallAndStandingOnGarbage)
  {
    // every kind of value with every number of a byte and some that are
    // none, to every destination and one that there is not
    std::uint32_t seed = 1;
    const float odd[] = {-1.0f, 256.0f, 1e30f, -1e30f, std::numeric_limits<float>::quiet_NaN(), 0.5f};
    for (int round = 0; round < 200000; round++)
    {
      seed = seed * 1664525u + 1013904223u;
      const std::uint32_t bits = seed >> 8;
      QcMessageValue value{.kind = static_cast<QcMessageKind>(bits % 8)};
      value.number = (bits >> 3) % 16 == 0 ? odd[(bits >> 7) % 6] : static_cast<float>((bits >> 7) % 40);
      value.text = "text";
      value.entity = static_cast<std::int32_t>(bits >> 12) - 2000;
      const auto destination = static_cast<QcMessageDestination>((bits >> 16) % 5);
      _reader.Read(destination, static_cast<std::int32_t>((bits >> 20) % 3), value);
    }
    _reader.Flush();

    // messages came out whole by chance, and none is told of twice
    EXPECT_GT(_listener.counts["intermission"], 0u);
    EXPECT_GT(_listener.counts["broken"], 0u);
    EXPECT_GT(_listener.counts["unknown"], 0u);
    EXPECT_LT(_listener.messages.size(), 200000u);
    for (const QcMessageDestination destination : {broadcast, one, all, QcMessageDestination::Init})
    {
      EXPECT_FALSE(_reader.IsInMessage(destination));
    }
  }

  TEST_F(ServerMessageReaderTest, TellsOfAMessageThatNeverEndedWhenFlushed)
  {
    Write(broadcast, {Byte(23), Byte(6), Entity(1), Coord(1.0f)});
    Write(all, {Byte(31)});
    Write(one, {Byte(27)}, 1);
    EXPECT_THAT(_listener.messages, ElementsAre("killedmonster to 1 for 1"));

    _reader.Flush();
    EXPECT_THAT(_listener.messages, ElementsAre(
                  "killedmonster to 1 for 1",
                  "broken to 0 for 0: 23, the message ended after 3 of its parts after the first byte",
                  "broken to 2 for 0: 31, the message ended after 0 of its parts after the first byte"));
    EXPECT_FALSE(_reader.IsInMessage(broadcast));

    // nothing is left of them, and nothing is told twice
    _reader.Flush();
    EXPECT_THAT(_listener.messages, SizeIs(3));
    Write(broadcast, {Byte(28)});
    EXPECT_EQ(_listener.counts["foundsecret"], 1u);
  }

  TEST_F(ServerMessageReaderTest, StopsSkippingWhenFlushed)
  {
    Write(all, {Byte(200), Byte(201)});
    _reader.Flush();
    Write(all, {Byte(201)});
    EXPECT_THAT(_listener.messages, ElementsAre("unknown to 2 for 0: 200", "unknown to 2 for 0: 201"));
  }

  TEST_F(ServerMessageReaderTest, TakesANumberForTheByteTheNetworkCarries)
  {
    // 286 is 30 as a byte, and a char is carried as a byte is
    Write(all, {Byte(286.0f)});
    Write(all, {Byte(32), {.kind = QcMessageKind::Char, .number = -1.0f}, Byte(2.9f)});
    EXPECT_THAT(_listener.messages, ElementsAre("intermission to 2 for 0", "cdtrack to 2 for 0: 255 then 2"));
  }
}
