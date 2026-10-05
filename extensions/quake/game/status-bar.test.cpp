#include "status-bar.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "hud-picture.test.hpp"
#include "qc-item.hpp"
#include "scoreboard.hpp"

namespace
{
  using quake::HudPicture;
  using quake::HudPictureKind;
  using quake::MakeLetter;
  using quake::MakeBackground;
  using quake::MakePicture;
  using quake::PlayerStats;
  using quake::QcItem;
  using quake::Scoreboard;
  using quake::StatusBar;
  using quake::StatusBarOptions;
  using quake::StatusBarSize;
  using ::testing::Contains;
  using ::testing::ElementsAre;
  using ::testing::ElementsAreArray;
  using ::testing::IsEmpty;
  using ::testing::Not;

  /// Several items as the bits of the stats.
  template <typename... Items>
  std::uint32_t bits(const Items... items)
  {
    return (static_cast<std::uint32_t>(items) | ...);
  }

  /// A player as a level starts with: the shotgun in the hands, 25 shells,
  /// and the axe.
  PlayerStats make_fresh_player()
  {
    PlayerStats stats;
    stats.health = 100;
    stats.ammo = 25;
    stats.shells = 25;
    stats.weapon = bits(QcItem::Shotgun);
    stats.items = bits(QcItem::Shotgun, QcItem::Shells, QcItem::Axe);
    return stats;
  }

  constexpr StatusBarOptions bar_only = {.size = StatusBarSize::BarOnly};

  /// The name of the picture drawn at a place. Empty when there is none.
  std::string_view find_at(const std::vector<HudPicture> &pictures, const std::int32_t x, const std::int32_t y)
  {
    for (const HudPicture &picture : pictures)
    {
      if (picture.kind == HudPictureKind::Picture && picture.x == x && picture.y == y && picture.name != "sbar" &&
        picture.name != "ibar")
      {
        return picture.name;
      }
    }
    return {};
  }

  TEST(StatusBarTest, DrawsBothBarsForAPlayerInFullHealthWithTheShotgun)
  {
    const StatusBar bar;

    EXPECT_THAT(bar.Layout(make_fresh_player(), 5.0f, {}), ElementsAre(
      // what is carried: the shotgun in the hands, and the four counts
      MakeBackground("ibar", 0, 152),
      MakePicture("inv2_shotgun", 0, 160),
      MakeLetter(20, 18, 152),
      MakeLetter(23, 26, 152),
      MakeLetter(18, 74, 152),
      MakeLetter(18, 122, 152),
      MakeLetter(18, 170, 152),

      // no armour, in red; the best face; the health; the shells
      MakeBackground("sbar", 0, 176),
      MakePicture("anum_0", 72, 176),
      MakePicture("face1", 112, 176),
      MakePicture("num_1", 136, 176),
      MakePicture("num_0", 160, 176),
      MakePicture("num_0", 184, 176),
      MakePicture("sb_shells", 224, 176),
      MakePicture("num_2", 272, 176),
      MakePicture("num_5", 296, 176)));
  }

  TEST(StatusBarTest, WritesLowHealthAndLowAmmoInRed)
  {
    PlayerStats stats = make_fresh_player();
    stats.health = 25;
    stats.ammo = 10;

    const StatusBar bar;
    EXPECT_THAT(bar.Layout(stats, 0.0f, bar_only), ElementsAre(
      MakeBackground("sbar", 0, 176),
      MakePicture("anum_0", 72, 176),
      MakePicture("face4", 112, 176),
      MakePicture("anum_2", 160, 176),
      MakePicture("anum_5", 184, 176),
      MakePicture("sb_shells", 224, 176),
      MakePicture("anum_1", 272, 176),
      MakePicture("anum_0", 296, 176)));

    // one more of each is white again
    stats.health = 26;
    stats.ammo = 11;
    const auto pictures = bar.Layout(stats, 0.0f, bar_only);
    EXPECT_THAT(pictures, Contains(MakePicture("num_6", 184, 176)));
    EXPECT_THAT(pictures, Contains(MakePicture("num_1", 296, 176)));
  }

  TEST(StatusBarTest, ChoosesTheFaceByABandOfHealth)
  {
    const StatusBar bar;
    PlayerStats stats = make_fresh_player();

    const auto face_at = [&](const std::int32_t health)
    {
      stats.health = health;
      return find_at(bar.Layout(stats, 0.0f, bar_only), 112, 176);
    };

    EXPECT_EQ(face_at(250), "face1");
    EXPECT_EQ(face_at(80), "face1");
    EXPECT_EQ(face_at(79), "face2");
    EXPECT_EQ(face_at(60), "face2");
    EXPECT_EQ(face_at(59), "face3");
    EXPECT_EQ(face_at(40), "face3");
    EXPECT_EQ(face_at(39), "face4");
    EXPECT_EQ(face_at(20), "face4");
    EXPECT_EQ(face_at(19), "face5");
    EXPECT_EQ(face_at(1), "face5");
  }

  TEST(StatusBarTest, ShowsTheFaceInPainForAFifthOfASecondAfterAHit)
  {
    StatusBar bar;
    PlayerStats stats = make_fresh_player();
    stats.health = 45;

    const auto face_at = [&](const float time) { return find_at(bar.Layout(stats, time, bar_only), 112, 176); };

    EXPECT_EQ(face_at(10.0f), "face3");

    bar.ShowPain(10.0f);
    EXPECT_EQ(face_at(10.0f), "face_p3");
    EXPECT_EQ(face_at(10.15f), "face_p3");
    EXPECT_EQ(face_at(10.25f), "face3");

    // a level that began anew is before the hit
    EXPECT_EQ(face_at(1.0f), "face3");

    // each band has its own
    stats.health = 100;
    bar.ShowPain(20.0f);
    EXPECT_EQ(face_at(20.1f), "face_p1");
    stats.health = 5;
    EXPECT_EQ(face_at(20.1f), "face_p5");

    bar.Forget();
    EXPECT_EQ(face_at(20.1f), "face5");
  }

  TEST(StatusBarTest, ShowsAFaceOfItsOwnForWhatLastsForAWhile)
  {
    StatusBar bar;
    PlayerStats stats = make_fresh_player();
    const std::uint32_t usual = stats.items;

    const auto face_with = [&](const std::uint32_t more)
    {
      stats.items = usual | more;
      return find_at(bar.Layout(stats, 0.0f, bar_only), 112, 176);
    };

    EXPECT_EQ(face_with(bits(QcItem::Invisibility)), "face_invis");
    EXPECT_EQ(face_with(bits(QcItem::Quad)), "face_quad");
    EXPECT_EQ(face_with(bits(QcItem::Invulnerability)), "face_invul2");
    EXPECT_EQ(face_with(bits(QcItem::Invisibility, QcItem::Invulnerability)), "face_inv2");

    // the two together come before the quad, and the quad before one alone
    EXPECT_EQ(face_with(bits(QcItem::Invisibility, QcItem::Invulnerability, QcItem::Quad)), "face_inv2");
    EXPECT_EQ(face_with(bits(QcItem::Invisibility, QcItem::Quad)), "face_quad");

    // the suit has none, and pain does not show through one
    EXPECT_EQ(face_with(bits(QcItem::Suit)), "face1");
    bar.ShowPain(0.0f);
    EXPECT_EQ(face_with(bits(QcItem::Quad)), "face_quad");
  }

  TEST(StatusBarTest, ShowsThePictureOfTheArmourWornAndItsNumber)
  {
    const StatusBar bar;
    PlayerStats stats = make_fresh_player();
    const std::uint32_t usual = stats.items;

    stats.armor = 100;
    stats.items = usual | bits(QcItem::Armor1);
    auto pictures = bar.Layout(stats, 0.0f, bar_only);
    EXPECT_EQ(find_at(pictures, 0, 176), "sb_armor1");
    EXPECT_THAT(pictures, Contains(MakePicture("num_1", 24, 176)));
    EXPECT_THAT(pictures, Contains(MakePicture("num_0", 48, 176)));
    EXPECT_THAT(pictures, Contains(MakePicture("num_0", 72, 176)));

    stats.armor = 150;
    stats.items = usual | bits(QcItem::Armor2);
    EXPECT_EQ(find_at(bar.Layout(stats, 0.0f, bar_only), 0, 176), "sb_armor2");

    stats.armor = 25;
    stats.items = usual | bits(QcItem::Armor3);
    pictures = bar.Layout(stats, 0.0f, bar_only);
    EXPECT_EQ(find_at(pictures, 0, 176), "sb_armor3");
    EXPECT_THAT(pictures, Contains(MakePicture("anum_2", 48, 176)));
    EXPECT_THAT(pictures, Contains(MakePicture("anum_5", 72, 176)));

    // without armour there is the number alone
    stats.armor = 0;
    stats.items = usual;
    EXPECT_EQ(find_at(bar.Layout(stats, 0.0f, bar_only), 0, 176), "");
  }

  TEST(StatusBarTest, Shows666InRedForAPlayerWhoCanNotBeHurt)
  {
    const StatusBar bar;
    PlayerStats stats = make_fresh_player();
    stats.armor = 100;
    stats.items |= bits(QcItem::Armor2, QcItem::Invulnerability);

    EXPECT_THAT(bar.Layout(stats, 0.0f, bar_only), ElementsAre(
      MakeBackground("sbar", 0, 176),
      MakePicture("anum_6", 24, 176),
      MakePicture("anum_6", 48, 176),
      MakePicture("anum_6", 72, 176),
      MakePicture("disc", 0, 176),
      MakePicture("face_invul2", 112, 176),
      MakePicture("num_1", 136, 176),
      MakePicture("num_0", 160, 176),
      MakePicture("num_0", 184, 176),
      MakePicture("sb_shells", 224, 176),
      MakePicture("num_2", 272, 176),
      MakePicture("num_5", 296, 176)));
  }

  TEST(StatusBarTest, ShowsThePictureOfTheAmmunitionInUseAndNoneForTheAxe)
  {
    const StatusBar bar;
    PlayerStats stats = make_fresh_player();
    const std::uint32_t weapons = bits(QcItem::Shotgun, QcItem::Axe);

    const auto ammo_with = [&](const std::uint32_t more)
    {
      stats.items = weapons | more;
      return find_at(bar.Layout(stats, 0.0f, bar_only), 224, 176);
    };

    EXPECT_EQ(ammo_with(bits(QcItem::Shells)), "sb_shells");
    EXPECT_EQ(ammo_with(bits(QcItem::Nails)), "sb_nails");
    EXPECT_EQ(ammo_with(bits(QcItem::Rockets)), "sb_rocket");
    EXPECT_EQ(ammo_with(bits(QcItem::Cells)), "sb_cells");

    // the axe: no picture, and a count of nothing in red
    stats.weapon = bits(QcItem::Axe);
    stats.ammo = 0;
    EXPECT_EQ(ammo_with(0), "");
    EXPECT_THAT(bar.Layout(stats, 0.0f, bar_only), Contains(MakePicture("anum_0", 296, 176)));
  }

  TEST(StatusBarTest, ShowsEveryWeaponCarriedAndLightsTheOneInTheHands)
  {
    const StatusBar bar;
    PlayerStats stats = make_fresh_player();
    stats.items |= bits(
      QcItem::SuperShotgun, QcItem::Nailgun, QcItem::SuperNailgun, QcItem::GrenadeLauncher,
      QcItem::RocketLauncher, QcItem::Lightning);
    stats.weapon = bits(QcItem::RocketLauncher);

    const auto pictures = bar.Layout(stats, 0.0f, {});
    EXPECT_EQ(find_at(pictures, 0, 160), "inv_shotgun");
    EXPECT_EQ(find_at(pictures, 24, 160), "inv_sshotgun");
    EXPECT_EQ(find_at(pictures, 48, 160), "inv_nailgun");
    EXPECT_EQ(find_at(pictures, 72, 160), "inv_snailgun");
    EXPECT_EQ(find_at(pictures, 96, 160), "inv_rlaunch");
    EXPECT_EQ(find_at(pictures, 120, 160), "inv2_srlaunch");
    EXPECT_EQ(find_at(pictures, 144, 160), "inv_lightng");

    // one that is not carried leaves its place empty
    stats.items &= ~bits(QcItem::Nailgun);
    EXPECT_EQ(find_at(bar.Layout(stats, 0.0f, {}), 48, 160), "");
  }

  TEST(StatusBarTest, FlashesAWeaponForASecondAfterItWasPickedUp)
  {
    StatusBar bar;
    PlayerStats stats = make_fresh_player();
    bar.Watch(stats, 0.0f);

    // the nailgun is picked up at 10 seconds, and is not in the hands
    stats.items |= bits(QcItem::Nailgun);
    bar.Watch(stats, 10.0f);
    bar.Watch(stats, 10.05f);

    const auto nailgun_at = [&](const float time) { return find_at(bar.Layout(stats, time, {}), 48, 160); };

    EXPECT_EQ(nailgun_at(10.0f), "inva1_nailgun");
    EXPECT_EQ(nailgun_at(10.05f), "inva1_nailgun");
    EXPECT_EQ(nailgun_at(10.15f), "inva2_nailgun");
    EXPECT_EQ(nailgun_at(10.25f), "inva3_nailgun");
    EXPECT_EQ(nailgun_at(10.35f), "inva4_nailgun");
    EXPECT_EQ(nailgun_at(10.45f), "inva5_nailgun");
    EXPECT_EQ(nailgun_at(10.55f), "inva1_nailgun");
    EXPECT_EQ(nailgun_at(10.95f), "inva5_nailgun");
    EXPECT_EQ(nailgun_at(11.05f), "inv_nailgun");

    // the shotgun, got at the start, is long at rest in the hands
    EXPECT_EQ(find_at(bar.Layout(stats, 10.25f, {}), 0, 160), "inv2_shotgun");

    // in the hands it flashes the same, and then is lit
    stats.weapon = bits(QcItem::Nailgun);
    EXPECT_EQ(nailgun_at(10.25f), "inva3_nailgun");
    EXPECT_EQ(nailgun_at(11.05f), "inv2_nailgun");

    // a level that began anew is before the weapon was got
    EXPECT_EQ(nailgun_at(2.0f), "inv2_nailgun");
  }

  TEST(StatusBarTest, FlashesAgainAWeaponThatWasLostAndGotAgainAndForgetsOnRequest)
  {
    StatusBar bar;
    PlayerStats stats = make_fresh_player();

    // what a player starts a level with is new
    bar.Watch(stats, 0.0f);
    EXPECT_EQ(find_at(bar.Layout(stats, 0.35f, {}), 0, 160), "inva4_shotgun");

    stats.items &= ~bits(QcItem::Shotgun);
    bar.Watch(stats, 20.0f);
    stats.items |= bits(QcItem::Shotgun);
    bar.Watch(stats, 30.0f);
    EXPECT_EQ(find_at(bar.Layout(stats, 30.15f, {}), 0, 160), "inva2_shotgun");

    bar.Forget();
    EXPECT_EQ(find_at(bar.Layout(stats, 30.15f, {}), 0, 160), "inv2_shotgun");
  }

  TEST(StatusBarTest, WritesTheFourCountsInSmallDigits)
  {
    const StatusBar bar;
    PlayerStats stats;
    stats.health = 100;
    stats.shells = 100;
    stats.nails = 7;
    stats.rockets = 1500;
    stats.cells = -3;

    EXPECT_THAT(bar.Layout(stats, 0.0f, {}), ElementsAre(
      MakeBackground("ibar", 0, 152),
      MakeLetter(19, 10, 152),
      MakeLetter(18, 18, 152),
      MakeLetter(18, 26, 152),
      MakeLetter(25, 74, 152),
      MakeLetter(27, 106, 152),
      MakeLetter(27, 114, 152),
      MakeLetter(27, 122, 152),
      MakeLetter(18, 170, 152),
      MakeBackground("sbar", 0, 176),
      MakePicture("anum_0", 72, 176),
      MakePicture("face1", 112, 176),
      MakePicture("num_1", 136, 176),
      MakePicture("num_0", 160, 176),
      MakePicture("num_0", 184, 176),
      MakePicture("anum_0", 296, 176)));
  }

  TEST(StatusBarTest, ShowsTheKeysWhatLastsForAWhileAndTheRunes)
  {
    const StatusBar bar;
    PlayerStats stats = make_fresh_player();
    const std::uint32_t usual = stats.items;

    stats.items = usual | bits(QcItem::Key1, QcItem::Key2);
    auto pictures = bar.Layout(stats, 0.0f, {});
    EXPECT_EQ(find_at(pictures, 192, 160), "sb_key1");
    EXPECT_EQ(find_at(pictures, 208, 160), "sb_key2");
    EXPECT_EQ(find_at(pictures, 224, 160), "");

    stats.items = usual | bits(QcItem::Key2, QcItem::Invisibility, QcItem::Invulnerability, QcItem::Suit, QcItem::Quad);
    pictures = bar.Layout(stats, 0.0f, {});
    EXPECT_EQ(find_at(pictures, 192, 160), "");
    EXPECT_EQ(find_at(pictures, 208, 160), "sb_key2");
    EXPECT_EQ(find_at(pictures, 224, 160), "sb_invis");
    EXPECT_EQ(find_at(pictures, 240, 160), "sb_invuln");
    EXPECT_EQ(find_at(pictures, 256, 160), "sb_suit");
    EXPECT_EQ(find_at(pictures, 272, 160), "sb_quad");

    stats.items = usual | bits(QcItem::Sigil1, QcItem::Sigil3, QcItem::Sigil4);
    pictures = bar.Layout(stats, 0.0f, {});
    EXPECT_EQ(find_at(pictures, 288, 160), "sb_sigil1");
    EXPECT_EQ(find_at(pictures, 296, 160), "");
    EXPECT_EQ(find_at(pictures, 304, 160), "sb_sigil3");
    EXPECT_EQ(find_at(pictures, 312, 160), "sb_sigil4");
  }

  TEST(StatusBarTest, DrawsTheBackgroundsSeeThroughAndWhatIsOnThemWhole)
  {
    const StatusBar bar;
    const PlayerStats stats = make_fresh_player();

    for (const HudPicture &picture : bar.Layout(stats, 0.0f, {.size = StatusBarSize::Full, .shows_scores = true}))
    {
      const bool is_background = picture.name == "sbar" || picture.name == "ibar" || picture.name == "scorebar";
      EXPECT_EQ(picture.opacity, is_background ? StatusBar::background_opacity : 1.0f) << picture.name;
    }
    EXPECT_LT(StatusBar::background_opacity, 1.0f);
  }

  TEST(StatusBarTest, ShowsTheBarsTheSizeAsksFor)
  {
    const StatusBar bar;
    const PlayerStats stats = make_fresh_player();

    const auto both = bar.Layout(stats, 0.0f, {.size = StatusBarSize::Full});
    EXPECT_THAT(both, Contains(MakeBackground("ibar", 0, 152)));
    EXPECT_THAT(both, Contains(MakeBackground("sbar", 0, 176)));

    const auto one = bar.Layout(stats, 0.0f, {.size = StatusBarSize::BarOnly});
    EXPECT_THAT(one, Not(Contains(MakeBackground("ibar", 0, 152))));
    EXPECT_EQ(one.front(), MakeBackground("sbar", 0, 176));

    EXPECT_THAT(bar.Layout(stats, 0.0f, {.size = StatusBarSize::None}), IsEmpty());

    EXPECT_EQ(StatusBarOptions::SizeOfViewSize(100.0f), StatusBarSize::Full);
    EXPECT_EQ(StatusBarOptions::SizeOfViewSize(110.0f), StatusBarSize::BarOnly);
    EXPECT_EQ(StatusBarOptions::SizeOfViewSize(120.0f), StatusBarSize::None);
  }

  TEST(StatusBarTest, ShowsTheScoreboardInTheMiddleWhenAskedAndKeepsTheBar)
  {
    const StatusBar bar;
    PlayerStats stats = make_fresh_player();
    stats.level_name = "the Slipgate Complex";
    stats.time = 61.0f;

    // over the view, in the middle of the screen, after the bar as it is
    const auto scores = Scoreboard::Layout(stats, quake::HudAnchor::Center, Scoreboard::middle_top);
    const auto plain = bar.Layout(stats, 61.0f, bar_only);
    auto expected = plain;
    expected.insert(expected.end(), scores.begin(), scores.end());
    EXPECT_THAT(
      bar.Layout(stats, 61.0f, {.size = StatusBarSize::BarOnly, .shows_scores = true}), ElementsAreArray(expected));
    EXPECT_THAT(plain, Contains(MakeBackground("sbar", 0, 176)));
    EXPECT_EQ(scores.front(), MakeBackground("scorebar", 0, 88, quake::HudAnchor::Center));

    // without a bar the board is all there is
    EXPECT_THAT(
      bar.Layout(stats, 61.0f, {.size = StatusBarSize::None, .shows_scores = true}), ElementsAreArray(scores));

    // with what is carried above the bar
    const auto full = bar.Layout(stats, 61.0f, {.size = StatusBarSize::Full, .shows_scores = true});
    EXPECT_EQ(full.front(), MakeBackground("ibar", 0, 152));
    EXPECT_THAT(full, Contains(MakeBackground("sbar", 0, 176)));
    EXPECT_THAT(full, Contains(MakeBackground("scorebar", 0, 88, quake::HudAnchor::Center)));
  }

  TEST(StatusBarTest, GivesItsPlaceToTheScoreboardForADeadPlayer)
  {
    const StatusBar bar;
    PlayerStats stats = make_fresh_player();
    stats.level_name = "the Slipgate Complex";
    stats.time = 61.0f;

    // at the lower edge, in place of the bar, as in the original
    stats.health = 0;
    EXPECT_THAT(bar.Layout(stats, 61.0f, bar_only), ElementsAreArray(Scoreboard::Layout(stats)));
    stats.health = -40;
    EXPECT_THAT(bar.Layout(stats, 61.0f, bar_only), ElementsAreArray(Scoreboard::Layout(stats)));

    // and asking for it changes nothing then
    EXPECT_THAT(
      bar.Layout(stats, 61.0f, {.size = StatusBarSize::BarOnly, .shows_scores = true}),
      ElementsAreArray(Scoreboard::Layout(stats)));
  }
}
