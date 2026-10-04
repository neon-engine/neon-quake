#include "status-bar.hpp"

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "center-text.hpp"
#include "formats/picture-reader.hpp"
#include "formats/real-data.test.hpp"
#include "formats/wad.hpp"
#include "intermission.hpp"
#include "qc-item.hpp"
#include "scoreboard.hpp"

namespace
{
  using quake::CenterText;
  using quake::HudPicture;
  using quake::HudPictureKind;
  using quake::Intermission;
  using quake::Picture;
  using quake::PlayerStats;
  using quake::QcItem;
  using quake::RealData;
  using quake::Scoreboard;
  using quake::StatusBar;
  using quake::StatusBarOptions;
  using quake::StatusBarSize;
  using quake::Wad;

  /// Adds the names of the pictures of a layout to those seen, and checks
  /// that every letter is one of the sheet.
  void collect(const std::vector<HudPicture> &pictures, std::set<std::string> &names)
  {
    for (const HudPicture &picture : pictures)
    {
      names.emplace(picture.name);
      if (picture.kind == HudPictureKind::Character)
      {
        EXPECT_EQ(picture.name, HudPicture::characters_name);
        EXPECT_GE(picture.character, 0);
        EXPECT_LT(picture.character, 256);
      }
    }
  }

  /// The names of every picture the layouts give, over stats of all kinds:
  /// each item alone and all at once with each weapon in the hands and
  /// through its flash, each health and each count from below nothing to
  /// above three digits, in pain and not, the scores, and the screens
  /// between levels.
  std::set<std::string> collect_all_names()
  {
    std::set<std::string> names;

    std::vector<std::uint32_t> item_sets = {0, 0xffffffffu};
    for (std::uint32_t bit = 0; bit < 32; bit++) { item_sets.push_back(1u << bit); }
    item_sets.push_back(
      static_cast<std::uint32_t>(QcItem::Invisibility) | static_cast<std::uint32_t>(QcItem::Invulnerability));

    // what is carried
    for (const std::uint32_t items : item_sets)
    {
      PlayerStats stats;
      stats.health = 100;
      stats.items = items;
      for (std::uint32_t weapon = 0; weapon < 8; weapon++)
      {
        stats.weapon = weapon == 7 ? 0 : 1u << weapon;

        StatusBar bar;
        collect(bar.Layout(stats, 5.0f, {}), names);

        bar.Watch(stats, 5.0f);
        for (int tenth = 0; tenth < 12; tenth++)
        {
          collect(bar.Layout(stats, 5.05f + 0.1f * static_cast<float>(tenth), {}), names);
        }
      }
    }

    // the numbers
    for (std::int32_t count = -120; count <= 1100; count++)
    {
      PlayerStats stats;
      stats.health = count;

      // below nothing for a player who lives
      stats.armor = 50 - count;
      stats.ammo = count + 2;
      stats.shells = count;
      stats.nails = count + 3;
      stats.rockets = count + 4;
      stats.cells = count + 5;
      stats.killed_monsters = count;
      stats.total_monsters = count + 6;
      stats.found_secrets = count + 7;
      stats.total_secrets = count + 8;
      stats.level_name = "The Level (of 1234567890)";
      stats.time = static_cast<float>(count * 7);

      StatusBar bar;
      collect(bar.Layout(stats, 5.0f, {}), names);
      bar.ShowPain(5.0f);
      collect(bar.Layout(stats, 5.1f, {}), names);
      collect(bar.Layout(stats, 5.0f, {.size = StatusBarSize::BarOnly, .shows_scores = true}), names);
      collect(Scoreboard::Layout(stats), names);
      collect(Intermission::Layout(stats, static_cast<float>(count * 7)), names);
    }

    collect(Intermission::LayoutFinale("The end.\nOf it all!", 100.0f), names);
    collect(CenterText::Layout("You need the gold key", 0.0f), names);
    return names;
  }

  TEST(StatusBarRealDataTest, DrawsOnlyPicturesTheRealDataHas)
  {
    const RealData &data = RealData::Get();
    if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

    const auto wad_bytes = data.GetBytes("gfx.wad");
    ASSERT_FALSE(wad_bytes.empty()) << "the paks have no gfx.wad";

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(wad_bytes, error)) << error;

    const std::set<std::string> names = collect_all_names();

    // the layouts were asked for enough to give all they can: the 3 bars,
    // 13 white and 11 red signs of the numbers, 7 weapons in 7 states, 4
    // kinds of ammunition, 3 of armour, 6 items, 4 runes, 14 faces, the
    // disc, the letters, and the 3 pictures of the screens
    EXPECT_EQ(names.size(), 112u);

    for (const std::string &name : names)
    {
      Picture picture;
      if (name.starts_with("gfx/"))
      {
        const auto bytes = data.GetBytes(name);
        ASSERT_FALSE(bytes.empty()) << "the paks have no " << name;
        EXPECT_TRUE(quake::read_picture_file(name, bytes, picture, error)) << name << ": " << error;
      }
      else
      {
        ASSERT_NE(wad.Find(name), nullptr) << "gfx.wad has no " << name;
        EXPECT_TRUE(wad.ReadPicture(name, picture, error)) << name << ": " << error;
      }
      EXPECT_GT(picture.width, 0) << name;
      EXPECT_GT(picture.height, 0) << name;
    }
  }

  TEST(StatusBarRealDataTest, FindsThePicturesOfTheSizesTheLayoutsCountOn)
  {
    const RealData &data = RealData::Get();
    if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

    Wad wad;
    std::string error;
    ASSERT_TRUE(wad.Read(data.GetBytes("gfx.wad"), error)) << error;

    const auto expect_size = [&](const std::string_view name, const std::int32_t width, const std::int32_t height)
    {
      Picture picture;
      ASSERT_TRUE(wad.ReadPicture(name, picture, error)) << name << ": " << error;
      EXPECT_EQ(picture.width, width) << name;
      EXPECT_EQ(picture.height, height) << name;
    };

    expect_size("sbar", 320, 24);
    expect_size("ibar", 320, 24);
    expect_size("scorebar", 320, 24);
    expect_size("num_0", 24, 24);
    expect_size("anum_9", 24, 24);
    expect_size("num_colon", 24, 24);
    expect_size("face1", 24, 24);
    expect_size("sb_shells", 24, 24);
    expect_size("sb_armor1", 24, 24);
    expect_size("inv_shotgun", 24, 16);
    expect_size("sb_key1", 16, 16);
    expect_size("sb_sigil1", 8, 16);
    expect_size("conchars", 128, 128);

    // the last weapon is twice as wide, in the original as well
    expect_size("inv_lightng", 48, 16);
  }
}
