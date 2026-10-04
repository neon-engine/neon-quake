#include "demo-message-reader.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "demo-bytes.test.hpp"
#include "demo-entity-bit.hpp"
#include "demo-recording-handler.test.hpp"

// The tests of DemoMessageReader with packets made here, byte by byte, as
// a server of each protocol writes them.
namespace
{
  using quake::DemoBytes;
  using quake::DemoClientData;
  using quake::DemoEntityBit;
  using quake::DemoEntityUpdate;
  using quake::DemoMessageReader;
  using quake::DemoProtocol;
  using quake::DemoRecordingHandler;
  using quake::DemoServerInfo;
  using quake::LevelVector;
  using ::testing::ElementsAre;
  using ::testing::HasSubstr;
  using ::testing::IsEmpty;

  class DemoMessageReaderTest : public ::testing::Test
  {
  protected:
    DemoRecordingHandler _handler;
    DemoMessageReader _reader;

    /// The message that begins a level, for a protocol. Only RMQ has flags.
    static DemoBytes ServerInfo(const std::int32_t version, const std::uint32_t flags = 0)
    {
      DemoBytes bytes;
      bytes.Byte(11).Long(version);
      if (version == DemoProtocol::rmq) { bytes.Long(flags); }
      bytes.Byte(4).Byte(1).Text("The Level");
      bytes.Text("maps/start.bsp").Text("*1").Text("progs/player.mdl").Text("");
      bytes.Text("weapons/r_exp3.wav").Text("misc/null.wav").Text("");
      return bytes;
    }

    /// Reads a packet that has to be read whole.
    void Read(const DemoBytes &bytes)
    {
      std::string problem;
      EXPECT_TRUE(_reader.Read(bytes.Get(), _handler, problem)) << problem;
    }

    /// Begins a level of a protocol, and forgets that it was told of it.
    void Begin(const std::int32_t version, const std::uint32_t flags = 0)
    {
      Read(ServerInfo(version, flags));
      if (!_handler.messages.empty()) { _handler.messages.pop_back(); }
    }

    /// Reads a packet that has to be refused, and gives the reason.
    std::string ReadRefused(const DemoBytes &bytes)
    {
      std::string problem;
      EXPECT_FALSE(_reader.Read(bytes.Get(), _handler, problem));
      return problem;
    }

    static std::uint32_t Bits(const std::initializer_list<DemoEntityBit> bits)
    {
      std::uint32_t all = 0;
      for (const DemoEntityBit bit : bits) { all |= static_cast<std::uint32_t>(bit); }
      return all;
    }
  };

  // The start of a level.

  TEST_F(DemoMessageReaderTest, ReadsTheStartOfALevelOfTheOriginalProtocol)
  {
    Read(ServerInfo(DemoProtocol::netquake));

    ASSERT_EQ(_handler.infos.size(), 1u);
    const DemoServerInfo &info = _handler.infos[0];
    EXPECT_EQ(info.protocol.version, 15);
    EXPECT_EQ(info.protocol.flags, 0u);
    EXPECT_EQ(info.max_clients, 4);
    EXPECT_EQ(info.game_type, 1);
    EXPECT_EQ(info.level_name, "The Level");
    EXPECT_THAT(info.model_names, ElementsAre("", "maps/start.bsp", "*1", "progs/player.mdl"));
    EXPECT_THAT(info.sound_names, ElementsAre("", "weapons/r_exp3.wav", "misc/null.wav"));
    EXPECT_EQ(_reader.GetProtocol().version, 15);
  }

  TEST_F(DemoMessageReaderTest, ReadsTheFlagsOnlyRmqHasAfterItsProtocol)
  {
    Read(ServerInfo(DemoProtocol::rmq, DemoProtocol::short_angle | DemoProtocol::int32_coord));
    ASSERT_EQ(_handler.infos.size(), 1u);
    EXPECT_EQ(_handler.infos[0].protocol.version, 999);
    EXPECT_EQ(_handler.infos[0].protocol.flags, 0x82u);
    EXPECT_EQ(_handler.infos[0].level_name, "The Level");
    EXPECT_EQ(_reader.GetProtocol().flags, 0x82u);

    // a level of FitzQuake after it has none
    Read(ServerInfo(DemoProtocol::fitzquake));
    ASSERT_EQ(_handler.infos.size(), 2u);
    EXPECT_EQ(_handler.infos[1].protocol.version, 666);
    EXPECT_EQ(_reader.GetProtocol().flags, 0u);
    EXPECT_THAT(_handler.infos[1].model_names, ElementsAre("", "maps/start.bsp", "*1", "progs/player.mdl"));
  }

  TEST_F(DemoMessageReaderTest, RefusesALevelOfAProtocolItDoesNotKnowOrForNoPlayer)
  {
    EXPECT_THAT(ReadRefused(ServerInfo(16)), HasSubstr("protocol 16"));
    EXPECT_THAT(_handler.messages, IsEmpty());

    DemoBytes nobody;
    nobody.Byte(11).Long(15).Byte(0).Byte(0).Text("x").Text("").Text("");
    EXPECT_THAT(ReadRefused(nobody), HasSubstr("no player"));
    EXPECT_THAT(_handler.messages, IsEmpty());
  }

  TEST_F(DemoMessageReaderTest, ChangesTheProtocolWithTheMessageOfTheVersion)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes version;
    version.Byte(4).Long(666);
    Read(version);
    EXPECT_EQ(_reader.GetProtocol().version, 666);

    DemoBytes unknown;
    unknown.Byte(4).Long(3);
    EXPECT_THAT(ReadRefused(unknown), HasSubstr("protocol 3"));

    _reader.Reset();
    EXPECT_EQ(_reader.GetProtocol().version, 15);
  }

  // The messages that are the same in every protocol.

  TEST_F(DemoMessageReaderTest, ReadsTheMessagesOfTheClockAndTheView)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(1);
    bytes.Byte(7).Float(12.5f);
    bytes.Byte(5).Short(300);
    bytes.Byte(10).Angle(45.0f).Angle(-90.0f).Angle(0.0f);
    bytes.Byte(24).Byte(1);
    bytes.Byte(24).Byte(0);
    bytes.Byte(25).Byte(3);
    Read(bytes);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre("time 12.5", "view 300", "angles 45 -90 0", "pause true", "pause false", "signon 3"));
  }

  TEST_F(DemoMessageReaderTest, ReadsTheMessagesWithText)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(8).Text("a line\n");
    bytes.Byte(26).Text("in the middle");
    bytes.Byte(9).Text("bf\n");
    bytes.Byte(12).Byte(63).Text("mmamam");
    bytes.Byte(31).Text("the end");
    bytes.Byte(34).Text("a camera");
    bytes.Byte(8).Text("");
    Read(bytes);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre(
        "print a line\n", "center in the middle", "command bf\n", "style 63 mmamam", "finale the end",
        "cutscene a camera", "print "));
  }

  TEST_F(DemoMessageReaderTest, KeepsOnlyAsMuchOfALongTextAsTheOriginal)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(8).Text(std::string(5000, 'x'));
    bytes.Byte(27);
    Read(bytes);

    ASSERT_EQ(_handler.messages.size(), 2u);
    EXPECT_EQ(_handler.messages[0].size(), std::string("print ").size() + DemoMessageReader::max_text_size);
    EXPECT_EQ(_handler.messages[1], "killed");
  }

  TEST_F(DemoMessageReaderTest, ReadsTheMessagesOfTheStatusBarAndTheScoreboard)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(3).Byte(12).Long(-70000);
    bytes.Byte(13).Byte(3).Text("ranger");
    bytes.Byte(14).Byte(0).Short(-2);
    bytes.Byte(17).Byte(1).Byte(0x4d);
    bytes.Byte(27).Byte(28);
    bytes.Byte(32).Byte(5).Byte(6);
    bytes.Byte(30).Byte(33);
    Read(bytes);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre(
        "stat 12 -70000", "name 3 ranger", "frags 0 -2", "colors 1 77", "killed", "secret", "music 5 6",
        "intermission", "sell"));
  }

  TEST_F(DemoMessageReaderTest, RefusesAPlayerOfTheScoreboardTheServerHasNoPlaceFor)
  {
    Begin(DemoProtocol::netquake);

    for (const std::uint32_t kind : {13u, 14u, 17u})
    {
      DemoBytes bytes;
      bytes.Byte(kind).Byte(4).Short(0);
      EXPECT_THAT(ReadRefused(bytes), HasSubstr("player 4")) << kind;
    }
    EXPECT_THAT(_handler.messages, IsEmpty());
  }

  TEST_F(DemoMessageReaderTest, ReadsDamageAndParticles)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(19).Byte(10).Byte(25).Place({1.0f, -2.0f, 3.5f});
    bytes.Byte(18).Place({8.0f, 16.0f, -24.0f}).Char(16).Char(-32).Char(8).Byte(20).Byte(73);
    // a count of 255 is the cloud of an explosion
    bytes.Byte(18).Place({0.0f, 0.0f, 0.0f}).Char(0).Char(0).Char(0).Byte(255).Byte(0);
    Read(bytes);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre(
        "damage 10 25 from 1 -2 3.5", "particles 20 of 73 at 8 16 -24 to 1 -2 0.5",
        "particles 1024 of 0 at 0 0 0 to 0 0 0"));
  }

  TEST_F(DemoMessageReaderTest, ReadsNothingAfterTheServerWent)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(27).Byte(2).Byte(28).Byte(99);
    Read(bytes);
    EXPECT_THAT(_handler.messages, ElementsAre("killed", "disconnect"));
  }

  // Sounds.

  TEST_F(DemoMessageReaderTest, ReadsASoundWithAndWithoutItsVolumeAndAttenuation)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    // entity 5 on channel 2, both in one short
    bytes.Byte(6).Byte(0).Short(5 << 3 | 2).Byte(7).Place({1.0f, 2.0f, 3.0f});
    bytes.Byte(6).Byte(3).Byte(51).Byte(128).Short(1023 << 3 | 7).Byte(255).Place({0.0f, 0.0f, 0.0f});
    bytes.Byte(16).Short(5 << 3 | 2);
    Read(bytes);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre(
        "sound 7 of 5 on 2 volume 1 attenuation 1 at 1 2 3",
        "sound 255 of 1023 on 7 volume 0.2 attenuation 2 at 0 0 0", "stop 5 on 2"));
  }

  TEST_F(DemoMessageReaderTest, ReadsASoundOfFitzQuakeWithALargeEntityAndALargeSound)
  {
    Begin(DemoProtocol::fitzquake);

    DemoBytes bytes;
    bytes.Byte(6).Byte(8).Short(9000).Byte(4).Byte(7).Place({0.0f, 0.0f, 0.0f});
    bytes.Byte(6).Byte(16).Short(5 << 3 | 1).Short(700).Place({0.0f, 0.0f, 0.0f});
    bytes.Byte(6).Byte(8 | 16 | 1).Byte(255).Short(40000).Byte(0).Short(60000).Place({0.0f, 0.0f, 0.0f});
    Read(bytes);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre(
        "sound 7 of 9000 on 4 volume 1 attenuation 1 at 0 0 0",
        "sound 700 of 5 on 1 volume 1 attenuation 1 at 0 0 0",
        "sound 60000 of 40000 on 0 volume 1 attenuation 1 at 0 0 0"));
  }

  TEST_F(DemoMessageReaderTest, ReadsAStaticSoundInBothForms)
  {
    Begin(DemoProtocol::fitzquake);

    DemoBytes bytes;
    bytes.Byte(29).Place({1.0f, 2.0f, 3.0f}).Byte(200).Byte(255).Byte(64);
    bytes.Byte(44).Place({4.0f, 5.0f, 6.0f}).Short(1000).Byte(51).Byte(192);
    Read(bytes);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre(
        "static sound 200 volume 1 attenuation 1 at 1 2 3", "static sound 1000 volume 0.2 attenuation 3 at 4 5 6"));
  }

  // What a server says of the player.

  TEST_F(DemoMessageReaderTest, ReadsClientDataWithNothingButWhatIsAlwaysThere)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(15).Short(0).Long(0x90001001).Short(-5).Byte(1).Byte(2).Byte(3).Byte(4).Byte(5).Byte(32);
    bytes.Byte(27);
    Read(bytes);

    ASSERT_THAT(_handler.messages, ElementsAre("clientdata", "killed"));
    const DemoClientData &data = _handler.client_data[0];
    EXPECT_EQ(data.view_height, 22.0f);
    EXPECT_EQ(data.ideal_pitch, 0.0f);
    EXPECT_EQ(data.punch_angles, (LevelVector{}));
    EXPECT_EQ(data.velocity, (LevelVector{}));
    EXPECT_EQ(data.items, 0x90001001u);
    EXPECT_FALSE(data.is_on_ground);
    EXPECT_FALSE(data.is_in_water);
    EXPECT_EQ(data.weapon_frame, 0);
    EXPECT_EQ(data.armor, 0);
    EXPECT_EQ(data.weapon_model, 0);
    EXPECT_EQ(data.health, -5);
    EXPECT_EQ(data.ammo, 1);
    EXPECT_EQ(data.shells, 2);
    EXPECT_EQ(data.nails, 3);
    EXPECT_EQ(data.rockets, 4);
    EXPECT_EQ(data.cells, 5);
    EXPECT_EQ(data.active_weapon, 32u);
    EXPECT_EQ(data.weapon_alpha, 0);
  }

  TEST_F(DemoMessageReaderTest, ReadsClientDataWithEveryBitOfTheOriginal)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(15).Short(0x7eff);
    bytes.Char(-8).Char(10);
    // the kick and the speed take turns, axis by axis
    bytes.Char(-3).Char(2).Char(4).Char(-5).Char(6).Char(7);
    bytes.Long(7);
    bytes.Byte(9).Byte(150).Byte(33);
    bytes.Short(100).Byte(50).Byte(25).Byte(200).Byte(5).Byte(99).Byte(4);
    bytes.Byte(28);
    Read(bytes);

    ASSERT_THAT(_handler.messages, ElementsAre("clientdata", "secret"));
    const DemoClientData &data = _handler.client_data[0];
    EXPECT_EQ(data.view_height, -8.0f);
    EXPECT_EQ(data.ideal_pitch, 10.0f);
    EXPECT_EQ(data.punch_angles, (LevelVector{-3.0f, 4.0f, 6.0f}));
    EXPECT_EQ(data.velocity, (LevelVector{32.0f, -80.0f, 112.0f}));
    EXPECT_EQ(data.items, 7u);
    EXPECT_TRUE(data.is_on_ground);
    EXPECT_TRUE(data.is_in_water);
    EXPECT_EQ(data.weapon_frame, 9);
    EXPECT_EQ(data.armor, 150);
    EXPECT_EQ(data.weapon_model, 33);
    EXPECT_EQ(data.health, 100);
    EXPECT_EQ(data.ammo, 50);
    EXPECT_EQ(data.cells, 99);
    EXPECT_EQ(data.active_weapon, 4u);
  }

  TEST_F(DemoMessageReaderTest, ReadsClientDataWithTheHighBytesAndTheAlphaOfFitzQuake)
  {
    Begin(DemoProtocol::fitzquake);

    DemoBytes bytes;
    // the bits for the weapon, its frame, and the armour, and two more bytes of bits
    bytes.Byte(15).Short(0xf000).Byte(0xff).Byte(0x03);
    bytes.Long(0);
    bytes.Byte(1).Byte(2).Byte(3);
    bytes.Short(100).Byte(4).Byte(5).Byte(6).Byte(7).Byte(8).Byte(1);
    // weapon, armour, ammo, shells, nails, rockets, cells, weapon frame, alpha
    bytes.Byte(1).Byte(2).Byte(1).Byte(1).Byte(1).Byte(1).Byte(1).Byte(3).Byte(128);
    bytes.Byte(27);
    Read(bytes);

    ASSERT_THAT(_handler.messages, ElementsAre("clientdata", "killed"));
    const DemoClientData &data = _handler.client_data[0];
    EXPECT_EQ(data.weapon_frame, 1 + 3 * 256);
    EXPECT_EQ(data.armor, 2 + 2 * 256);
    EXPECT_EQ(data.weapon_model, 3 + 256);
    EXPECT_EQ(data.ammo, 4 + 256);
    EXPECT_EQ(data.shells, 5 + 256);
    EXPECT_EQ(data.nails, 6 + 256);
    EXPECT_EQ(data.rockets, 7 + 256);
    EXPECT_EQ(data.cells, 8 + 256);
    EXPECT_EQ(data.weapon_alpha, 128);
  }

  // The entities.

  TEST_F(DemoMessageReaderTest, ReadsAnUpdateThatSaysNothingButWhichEntity)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(0x80).Byte(17);
    // the bit for an entity that moves in steps has no value
    bytes.Byte(0x80 | 0x20).Byte(18);
    Read(bytes);

    ASSERT_EQ(_handler.updates.size(), 2u);
    EXPECT_EQ(_handler.updates[0].entity, 17);
    EXPECT_EQ(_handler.updates[0].bits, 0x80u);
    EXPECT_EQ(_handler.updates[1].entity, 18);
    EXPECT_TRUE(_handler.updates[1].Has(DemoEntityBit::Step));
  }

  TEST_F(DemoMessageReaderTest, ReadsAnUpdateWithEveryBitOfTheOriginal)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(0xdf).Byte(0x7f);
    bytes.Short(1000);
    bytes.Byte(3).Byte(4).Byte(5).Byte(6).Byte(7);
    // a coordinate and its angle take turns
    bytes.Coord(100.5f).Angle(90.0f).Coord(-200.0f).Angle(-45.0f).Coord(4095.875f).Angle(180.0f);
    bytes.Byte(27);
    Read(bytes);

    ASSERT_THAT(_handler.messages, ElementsAre("update 1000 bits 0x7fdf", "killed"));
    const DemoEntityUpdate &update = _handler.updates[0];
    EXPECT_EQ(update.model, 3);
    EXPECT_EQ(update.frame, 4);
    EXPECT_EQ(update.colormap, 5);
    EXPECT_EQ(update.skin, 6);
    EXPECT_EQ(update.effects, 7);
    EXPECT_EQ(update.origin, (LevelVector{100.5f, -200.0f, 4095.875f}));
    EXPECT_EQ(update.angles, (LevelVector{90.0f, -45.0f, -180.0f}));
  }

  TEST_F(DemoMessageReaderTest, ReadsAnUpdateWithWhatFitzQuakeAdded)
  {
    Begin(DemoProtocol::rmq, DemoProtocol::edict_scale);

    const std::uint32_t bits = Bits({
      DemoEntityBit::Signal, DemoEntityBit::MoreBits, DemoEntityBit::Extend1, DemoEntityBit::Frame,
      DemoEntityBit::Alpha, DemoEntityBit::Frame2, DemoEntityBit::Model2, DemoEntityBit::LerpFinish,
      DemoEntityBit::Scale, DemoEntityBit::Extend2,
    });
    DemoBytes bytes;
    bytes.Byte(bits).Byte(bits >> 8).Byte(bits >> 16).Byte(0);
    bytes.Byte(9);
    bytes.Byte(44);
    // alpha, scale, the high bytes of the frame and the model, and the time of the next frame
    bytes.Byte(128).Byte(32).Byte(2).Byte(1).Byte(51);
    bytes.Byte(27);
    Read(bytes);

    ASSERT_THAT(_handler.messages, ElementsAre(::testing::StartsWith("update 9 bits"), "killed"));
    const DemoEntityUpdate &update = _handler.updates[0];
    EXPECT_EQ(update.frame, 44);
    EXPECT_EQ(update.alpha, 128);
    EXPECT_EQ(update.scale, 32);
    EXPECT_EQ(update.frame_high, 2);
    EXPECT_EQ(update.model_high, 1);
    EXPECT_FLOAT_EQ(update.lerp_finish, 0.2f);
    EXPECT_FALSE(update.Has(DemoEntityBit::Model));
  }

  TEST_F(DemoMessageReaderTest, RefusesAnUpdateOfTheOriginalProtocolWithABitItHasNot)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(0x81).Byte(0x80).Byte(5).Float(1.0f).Float(0.5f);
    EXPECT_THAT(ReadRefused(bytes), HasSubstr("Nehahra"));
    EXPECT_THAT(_handler.messages, IsEmpty());
  }

  TEST_F(DemoMessageReaderTest, ReadsABaselineAndAStaticEntityOfTheOriginal)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(22).Short(600).Byte(3).Byte(4).Byte(5).Byte(6);
    bytes.Coord(1.0f).Angle(90.0f).Coord(2.0f).Angle(0.0f).Coord(3.0f).Angle(-90.0f);
    bytes.Byte(20).Byte(7).Byte(8).Byte(0).Byte(1);
    bytes.Coord(-1.0f).Angle(0.0f).Coord(-2.0f).Angle(45.0f).Coord(-3.0f).Angle(0.0f);
    Read(bytes);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre(
        "baseline 600: model 3 frame 4 colormap 5 skin 6 at 1 2 3 turned 90 0 -90 alpha 0 scale 16",
        "static: model 7 frame 8 colormap 0 skin 1 at -1 -2 -3 turned 0 45 0 alpha 0 scale 16"));
  }

  TEST_F(DemoMessageReaderTest, ReadsABaselineAndAStaticEntityOfFitzQuakeWithTheirFlags)
  {
    Begin(DemoProtocol::rmq);

    DemoBytes place;
    place.Coord(1.0f).Angle(0.0f).Coord(2.0f).Angle(0.0f).Coord(3.0f).Angle(0.0f);

    DemoBytes bytes;
    // no flag: as the original
    bytes.Byte(42).Short(1).Byte(0).Byte(3).Byte(4).Byte(5).Byte(6).Add(place);
    // a large model, a large frame, an alpha, and a scale
    bytes.Byte(42).Short(40000).Byte(15).Short(300).Short(400).Byte(5).Byte(6).Add(place).Byte(77).Byte(8);
    // only a large frame and an alpha
    bytes.Byte(43).Byte(2 | 4).Byte(9).Short(1000).Byte(0).Byte(0).Add(place).Byte(1);
    Read(bytes);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre(
        "baseline 1: model 3 frame 4 colormap 5 skin 6 at 1 2 3 turned 0 0 0 alpha 0 scale 16",
        "baseline 40000: model 300 frame 400 colormap 5 skin 6 at 1 2 3 turned 0 0 0 alpha 77 scale 8",
        "static: model 9 frame 1000 colormap 0 skin 0 at 1 2 3 turned 0 0 0 alpha 1 scale 16"));
  }

  // What is shown for a moment.

  TEST_F(DemoMessageReaderTest, ReadsEveryKindOfTempEntity)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    for (const std::uint32_t kind : {0u, 1u, 2u, 3u, 4u, 7u, 8u, 10u, 11u})
    {
      bytes.Byte(23).Byte(kind).Place({1.0f, 2.0f, 3.0f});
    }
    bytes.Byte(23).Byte(12).Place({4.0f, 5.0f, 6.0f}).Byte(100).Byte(8);
    for (const std::uint32_t kind : {5u, 6u, 9u, 13u})
    {
      bytes.Byte(23).Byte(kind).Short(77).Place({1.0f, 2.0f, 3.0f}).Place({4.0f, 5.0f, 6.0f});
    }
    Read(bytes);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre(
        "point 0 at 1 2 3", "point 1 at 1 2 3", "point 2 at 1 2 3", "point 3 at 1 2 3", "point 4 at 1 2 3",
        "point 7 at 1 2 3", "point 8 at 1 2 3", "point 10 at 1 2 3", "point 11 at 1 2 3",
        "explosion 100 8 at 4 5 6", "beam 5 of 77 from 1 2 3 to 4 5 6", "beam 6 of 77 from 1 2 3 to 4 5 6",
        "beam 9 of 77 from 1 2 3 to 4 5 6", "beam 13 of 77 from 1 2 3 to 4 5 6"));
  }

  TEST_F(DemoMessageReaderTest, RefusesATempEntityItDoesNotKnow)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(27).Byte(23).Byte(14).Place({1.0f, 2.0f, 3.0f});
    const std::string problem = ReadRefused(bytes);
    EXPECT_THAT(problem, HasSubstr("Temp entity 14"));
    EXPECT_THAT(problem, HasSubstr("at byte 1"));
    EXPECT_THAT(_handler.messages, ElementsAre("killed"));
  }

  // What FitzQuake added.

  TEST_F(DemoMessageReaderTest, ReadsTheSkyTheFlashAndTheFog)
  {
    Begin(DemoProtocol::fitzquake);

    DemoBytes bytes;
    bytes.Byte(37).Text("dusk_");
    bytes.Byte(40);
    bytes.Byte(41).Byte(51).Byte(255).Byte(0).Byte(102).Short(250);
    // a time that is negative is none
    bytes.Byte(41).Byte(0).Byte(0).Byte(0).Byte(0).Short(-100);
    Read(bytes);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre("sky dusk_", "flash", "fog 0.2 1 0 0.4 over 2.5", "fog 0 0 0 0 over 0"));
  }

  // How coordinates and angles are written.

  TEST_F(DemoMessageReaderTest, ReadsCoordinatesAsTheFlagsOfRmqSay)
  {
    // a temp entity at a place, for each way to write a coordinate
    Begin(DemoProtocol::rmq, DemoProtocol::float_coord);
    DemoBytes floats;
    floats.Byte(23).Byte(3).Float(1.25f).Float(-70000.5f).Float(0.001f);
    Read(floats);

    Begin(DemoProtocol::rmq, DemoProtocol::int32_coord);
    DemoBytes longs;
    longs.Byte(23).Byte(3).Long(16).Long(-160000).Long(1);
    Read(longs);

    Begin(DemoProtocol::rmq, DemoProtocol::coord_24_bit);
    DemoBytes shorts_and_bytes;
    shorts_and_bytes.Byte(23).Byte(3).Short(100).Byte(0).Short(-30000).Byte(51).Short(0).Byte(255);
    Read(shorts_and_bytes);

    Begin(DemoProtocol::rmq, 0);
    DemoBytes shorts;
    shorts.Byte(23).Byte(3).Short(8).Short(-32768).Short(1);
    Read(shorts);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre(
        "point 3 at 1.25 -70000.5 0.001", "point 3 at 1 -10000 0.0625", "point 3 at 100 -29999.8 1",
        "point 3 at 1 -4096 0.125"));
  }

  TEST_F(DemoMessageReaderTest, TakesTheWiderWayToWriteACoordinateWhenTheFlagsNameSeveral)
  {
    Begin(DemoProtocol::rmq, DemoProtocol::float_coord | DemoProtocol::int32_coord | DemoProtocol::coord_24_bit);
    DemoBytes floats;
    floats.Byte(23).Byte(3).Float(1.5f).Float(2.5f).Float(3.5f);
    Read(floats);

    Begin(DemoProtocol::rmq, DemoProtocol::int32_coord | DemoProtocol::coord_24_bit);
    DemoBytes longs;
    longs.Byte(23).Byte(3).Long(32).Long(48).Long(64);
    Read(longs);

    EXPECT_THAT(_handler.messages, ElementsAre("point 3 at 1.5 2.5 3.5", "point 3 at 2 3 4"));
  }

  TEST_F(DemoMessageReaderTest, ReadsAnglesAsTheFlagsOfRmqSay)
  {
    Begin(DemoProtocol::rmq, DemoProtocol::float_angle);
    DemoBytes floats;
    floats.Byte(10).Float(12.25f).Float(-359.5f).Float(720.0f);
    Read(floats);

    Begin(DemoProtocol::rmq, DemoProtocol::short_angle);
    DemoBytes shorts;
    shorts.Byte(10).Short(16384).Short(-8192).Short(128);
    Read(shorts);

    // a float wins over a short
    Begin(DemoProtocol::rmq, DemoProtocol::float_angle | DemoProtocol::short_angle);
    DemoBytes both;
    both.Byte(10).Float(1.0f).Float(2.0f).Float(3.0f);
    Read(both);

    Begin(DemoProtocol::rmq, 0);
    DemoBytes chars;
    chars.Byte(10).Char(64).Char(-32).Char(1);
    Read(chars);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre("angles 12.25 -359.5 720", "angles 90 -45 0.703125", "angles 1 2 3", "angles 90 -45 1.40625"));
  }

  TEST_F(DemoMessageReaderTest, ReadsEveryPlaceAndAngleOfALevelAsItsFlagsSay)
  {
    // as the third recording of the game has them: coordinates as longs,
    // angles as shorts
    Begin(DemoProtocol::rmq, DemoProtocol::short_angle | DemoProtocol::int32_coord);

    DemoBytes bytes;
    bytes.Byte(22).Short(2).Byte(1).Byte(0).Byte(0).Byte(0);
    bytes.Long(160).Short(16384).Long(320).Short(0).Long(-160).Short(-16384);
    bytes.Byte(0x80 | 0x02 | 0x10).Byte(2).Long(1600).Short(8192);
    bytes.Byte(6).Byte(0).Short(2 << 3).Byte(1).Long(16).Long(32).Long(48);
    bytes.Byte(19).Byte(1).Byte(2).Long(-16).Long(-32).Long(-48);
    bytes.Byte(18).Long(16).Long(16).Long(16).Char(0).Char(0).Char(0).Byte(1).Byte(2);
    bytes.Byte(29).Long(160).Long(160).Long(160).Byte(1).Byte(255).Byte(64);
    bytes.Byte(23).Byte(6).Short(1).Long(16).Long(32).Long(48).Long(64).Long(80).Long(96);
    Read(bytes);

    EXPECT_THAT(
      _handler.messages,
      ElementsAre(
        "baseline 2: model 1 frame 0 colormap 0 skin 0 at 10 20 -10 turned 90 0 -90 alpha 0 scale 16",
        "update 2 bits 0x92", "sound 1 of 2 on 0 volume 1 attenuation 1 at 1 2 3", "damage 1 2 from -1 -2 -3",
        "particles 1 of 2 at 1 1 1 to 0 0 0", "static sound 1 volume 1 attenuation 1 at 10 10 10",
        "beam 6 of 1 from 1 2 3 to 4 5 6"));
    ASSERT_EQ(_handler.updates.size(), 1u);
    EXPECT_EQ(_handler.updates[0].origin[0], 100.0f);
    EXPECT_EQ(_handler.updates[0].angles[1], 45.0f);
  }

  // What cannot be read.

  TEST_F(DemoMessageReaderTest, RefusesAMessageItDoesNotKnowAndSaysWhereItIs)
  {
    Begin(DemoProtocol::netquake);

    DemoBytes bytes;
    bytes.Byte(27).Byte(28).Byte(99).Byte(27);
    const std::string problem = ReadRefused(bytes);
    EXPECT_THAT(problem, HasSubstr("Message 99 is not known"));
    EXPECT_THAT(problem, HasSubstr("at byte 2 of a packet of 4 bytes"));
    EXPECT_THAT(problem, HasSubstr("after message 28"));
    EXPECT_THAT(_handler.messages, ElementsAre("killed", "secret"));

    // every first byte without the bit of an update that is no message
    for (const std::uint32_t kind : {0u, 21u, 35u, 36u, 38u, 39u, 45u, 50u, 86u, 127u})
    {
      DemoBytes unknown;
      unknown.Byte(kind).Long(0).Long(0);
      EXPECT_THAT(ReadRefused(unknown), HasSubstr("is not known")) << kind;
    }
  }

  TEST_F(DemoMessageReaderTest, RefusesAPacketThatEndsInTheMiddleOfAMessageWhereverItEnds)
  {
    // one of every message, in a protocol where every value is wide
    const std::uint32_t flags = DemoProtocol::float_angle | DemoProtocol::float_coord;
    const std::uint32_t update_bits = 0x00ffffffu & ~static_cast<std::uint32_t>(DemoEntityBit::Extend2);

    DemoBytes place;
    place.Float(1.0f).Float(2.0f).Float(3.0f);
    DemoBytes state;
    state.Float(1.0f).Float(0.0f).Float(2.0f).Float(0.0f).Float(3.0f).Float(0.0f);

    std::vector<DemoBytes> messages;
    const auto add = [&messages]() -> DemoBytes & { return messages.emplace_back(); };
    add() = ServerInfo(DemoProtocol::rmq, flags);
    add().Byte(1);
    add().Byte(3).Byte(1).Long(2);
    add().Byte(4).Long(999);
    add().Byte(5).Short(1);
    add().Byte(6).Byte(3 | 8 | 16).Byte(1).Byte(2).Short(3).Byte(4).Short(5).Add(place);
    add().Byte(7).Float(1.0f);
    add().Byte(8).Text("print");
    add().Byte(9).Text("command");
    add().Byte(10).Float(1.0f).Float(2.0f).Float(3.0f);
    add().Byte(12).Byte(1).Text("m");
    add().Byte(13).Byte(0).Text("name");
    add().Byte(14).Byte(0).Short(1);
    add().Byte(15).Short(0xffff).Byte(0xff).Byte(0x03).Char(1).Char(2).Char(3).Char(4).Char(5).Char(6).Char(7)
         .Char(8).Long(1).Byte(1).Byte(2).Byte(3).Short(4).Byte(5).Byte(6).Byte(7).Byte(8).Byte(9).Byte(10)
         .Byte(1).Byte(2).Byte(3).Byte(4).Byte(5).Byte(6).Byte(7).Byte(8).Byte(9);
    add().Byte(16).Short(8);
    add().Byte(17).Byte(0).Byte(1);
    add().Byte(18).Add(place).Char(1).Char(2).Char(3).Byte(4).Byte(5);
    add().Byte(19).Byte(1).Byte(2).Add(place);
    add().Byte(20).Byte(1).Byte(2).Byte(3).Byte(4).Add(state);
    add().Byte(22).Short(1).Byte(1).Byte(2).Byte(3).Byte(4).Add(state);
    add().Byte(23).Byte(3).Add(place);
    add().Byte(23).Byte(12).Add(place).Byte(1).Byte(2);
    add().Byte(23).Byte(5).Short(1).Add(place).Add(place);
    add().Byte(24).Byte(1);
    add().Byte(25).Byte(1);
    add().Byte(26).Text("center");
    add().Byte(27);
    add().Byte(28);
    add().Byte(29).Add(place).Byte(1).Byte(2).Byte(3);
    add().Byte(30);
    add().Byte(31).Text("finale");
    add().Byte(32).Byte(1).Byte(2);
    add().Byte(33);
    add().Byte(34).Text("cutscene");
    add().Byte(37).Text("sky");
    add().Byte(40);
    add().Byte(41).Byte(1).Byte(2).Byte(3).Byte(4).Short(5);
    add().Byte(42).Short(1).Byte(15).Short(1).Short(2).Byte(3).Byte(4).Add(state).Byte(5).Byte(6);
    add().Byte(43).Byte(15).Short(1).Short(2).Byte(3).Byte(4).Add(state).Byte(5).Byte(6);
    add().Byte(44).Add(place).Short(1).Byte(2).Byte(3);
    add().Byte(update_bits).Byte(update_bits >> 8).Byte(update_bits >> 16).Short(300).Byte(1).Byte(2).Byte(3)
         .Byte(4).Byte(5).Add(state).Byte(6).Byte(7).Byte(8).Byte(9).Byte(10);
    add().Byte(2);

    DemoBytes packet;
    std::vector<std::size_t> ends;
    for (const DemoBytes &message : messages)
    {
      packet.Add(message);
      ends.push_back(packet.bytes.size());
    }

    // whole, every message is read
    Read(packet);
    EXPECT_EQ(_handler.messages.size(), messages.size() - 2);
    EXPECT_EQ(_handler.messages.back(), "disconnect");

    // cut anywhere, the messages that are whole are read, and the cut one
    // is refused and not handed on
    std::size_t whole = 0;
    for (std::size_t size = 0; size < packet.bytes.size(); size++)
    {
      while (ends[whole] <= size) { whole++; }
      const std::size_t start = whole == 0 ? 0 : ends[whole - 1];

      _reader.Reset();
      _handler.messages.clear();
      std::string problem;
      const bool is_read = _reader.Read(std::span(packet.bytes).first(size), _handler, problem);
      // a nop and the change of the version tell the handler nothing
      const std::size_t told = whole - (whole > 1 ? 1 : 0) - (whole > 3 ? 1 : 0);
      EXPECT_EQ(_handler.messages.size(), told) << size;
      if (size == start)
      {
        EXPECT_TRUE(is_read) << size << ": " << problem;
        continue;
      }
      EXPECT_FALSE(is_read) << size;
      EXPECT_THAT(problem, HasSubstr("cut short")) << size;
      EXPECT_THAT(problem, HasSubstr(std::format("at byte {} of", start))) << size;
    }
  }

  TEST_F(DemoMessageReaderTest, ReadsBytesOfNoMeaningWithoutHarm)
  {
    // bytes that follow no rule, of every protocol: whatever comes of it,
    // nothing is read out of range
    std::uint32_t seed = 12345;
    for (const std::int32_t version : {DemoProtocol::netquake, DemoProtocol::fitzquake, DemoProtocol::rmq})
    {
      for (std::size_t round = 0; round < 2000; round++)
      {
        _reader.Reset();
        _handler = {};
        std::string problem;
        ASSERT_TRUE(_reader.Read(ServerInfo(version, seed & 0xff).Get(), _handler, problem));

        std::vector<std::uint8_t> bytes(1 + round % 97);
        for (std::uint8_t &byte : bytes)
        {
          seed = seed * 1664525u + 1013904223u;
          byte = static_cast<std::uint8_t>(seed >> 24);
        }
        if (!_reader.Read(bytes, _handler, problem)) { EXPECT_FALSE(problem.empty()); }
      }
    }
  }
}
