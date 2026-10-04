#include "view-tint.hpp"

#include <gtest/gtest.h>

#include "qc-item.hpp"

namespace
{
  using quake::BspContents;
  using quake::QcItem;
  using quake::ViewTint;

  TEST(ViewTintTest, IsNothingWhenNothingHappened)
  {
    EXPECT_FLOAT_EQ(ViewTint().Mix().amount, 0.0f);
  }

  TEST(ViewTintTest, IsRedAfterAHitTheBodyTookAndFadesAway)
  {
    ViewTint tint;
    tint.Hurt(20.0f, 0.0f);

    // 10 counted, three parts of a hundred each
    ViewTint::Colour colour = tint.Mix();
    EXPECT_FLOAT_EQ(colour.red, 255.0f);
    EXPECT_FLOAT_EQ(colour.green, 0.0f);
    EXPECT_NEAR(colour.amount, 0.3f, 0.0001f);

    // 150 parts of a hundred a second
    tint.Advance(0.1f);
    EXPECT_NEAR(tint.Mix().amount, 0.15f, 0.0001f);
    tint.Advance(1.0f);
    EXPECT_FLOAT_EQ(tint.Mix().amount, 0.0f);
  }

  TEST(ViewTintTest, CountsNoHitForLessThanTenAndNeverShowsMoreThanAll)
  {
    ViewTint weak;
    weak.Hurt(1.0f, 0.0f);
    EXPECT_NEAR(weak.Mix().amount, 0.3f, 0.0001f);

    ViewTint strong;
    strong.Hurt(500.0f, 0.0f);
    EXPECT_FLOAT_EQ(strong.Mix().amount, 1.0f);
  }

  TEST(ViewTintTest, IsBrownishWhenTheArmourTookMostOfAHit)
  {
    ViewTint tint;
    tint.Hurt(5.0f, 20.0f);
    EXPECT_FLOAT_EQ(tint.Mix().red, 200.0f);
    EXPECT_FLOAT_EQ(tint.Mix().green, 100.0f);
  }

  TEST(ViewTintTest, FlashesGoldForAPickupForHalfASecond)
  {
    ViewTint tint;
    tint.PickUp();
    EXPECT_FLOAT_EQ(tint.Mix().red, 215.0f);
    EXPECT_NEAR(tint.Mix().amount, 0.5f, 0.0001f);

    tint.Advance(0.5f);
    EXPECT_NEAR(tint.Mix().amount, 0.0f, 0.0001f);
  }

  TEST(ViewTintTest, TakesTheColourOfWhatTheEyesAreInUntilTheyAreOut)
  {
    ViewTint tint;
    tint.SetContents(BspContents::Water);
    EXPECT_FLOAT_EQ(tint.Mix().red, 130.0f);
    EXPECT_NEAR(tint.Mix().amount, 128.0f / 255.0f, 0.0001f);

    tint.Advance(10.0f);
    EXPECT_NEAR(tint.Mix().amount, 128.0f / 255.0f, 0.0001f);

    tint.SetContents(BspContents::Empty);
    EXPECT_FLOAT_EQ(tint.Mix().amount, 0.0f);
  }

  TEST(ViewTintTest, IsBlueWithTheQuad)
  {
    ViewTint tint;
    tint.SetItems(static_cast<std::uint32_t>(QcItem::Quad));
    EXPECT_FLOAT_EQ(tint.Mix().blue, 255.0f);
    EXPECT_NEAR(tint.Mix().amount, 30.0f / 255.0f, 0.0001f);
  }

  TEST(ViewTintTest, MixesWaterAndAHitOneOverTheOther)
  {
    ViewTint tint;
    tint.SetContents(BspContents::Water);
    tint.Hurt(20.0f, 0.0f);

    const ViewTint::Colour colour = tint.Mix();
    const float water = 128.0f / 255.0f;
    EXPECT_NEAR(colour.amount, water + 0.3f * (1.0f - water), 0.0001f);
    // more red than water alone has
    EXPECT_GT(colour.red, 130.0f);
  }
}
