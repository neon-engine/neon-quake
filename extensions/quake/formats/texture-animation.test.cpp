#include "texture-animation.hpp"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
  using quake::MipTexture;
  using quake::TextureAnimation;
  using ::testing::ElementsAre;
  using ::testing::IsEmpty;

  /// A list of textures of these names. An empty name is a place the level
  /// names and does not carry.
  std::vector<std::optional<MipTexture>> MakeTextures(const std::vector<std::string> &names)
  {
    std::vector<std::optional<MipTexture>> textures;
    for (const std::string &name : names)
    {
      if (name.empty())
      {
        textures.emplace_back();
        continue;
      }
      MipTexture texture;
      texture.name = name;
      textures.emplace_back(texture);
    }
    return textures;
  }

  TEST(TextureAnimationTest, ATextureThatIsNoPartOfARunIsShownAlone)
  {
    const auto textures = MakeTextures({"wall", "+0eye", ""});

    const TextureAnimation wall = TextureAnimation::Find(textures, 0);
    EXPECT_THAT(wall.frames, ElementsAre(0));
    EXPECT_THAT(wall.alternate, IsEmpty());
    EXPECT_FALSE(wall.Changes());

    // one picture of a run is no change either
    const TextureAnimation eye = TextureAnimation::Find(textures, 1);
    EXPECT_THAT(eye.frames, ElementsAre(1));
    EXPECT_FALSE(eye.Changes());

    // what is not there, and what is not in the list
    EXPECT_THAT(TextureAnimation::Find(textures, 2).frames, ElementsAre(2));
    EXPECT_THAT(TextureAnimation::Find(textures, 7).frames, ElementsAre(7));
    EXPECT_THAT(TextureAnimation::Find(textures, -1).frames, ElementsAre(-1));
  }

  TEST(TextureAnimationTest, FindsTheRunOfANameInItsOrder)
  {
    const auto textures = MakeTextures({"+2eye", "wall", "+0eye", "+1EYE", "+0other", "+1eyes"});

    const TextureAnimation found = TextureAnimation::Find(textures, 3);
    EXPECT_THAT(found.frames, ElementsAre(2, 3, 0));
    EXPECT_THAT(found.alternate, IsEmpty());
    EXPECT_TRUE(found.Changes());

    // from whichever of them it is asked
    EXPECT_THAT(TextureAnimation::Find(textures, 0).frames, ElementsAre(2, 3, 0));
  }

  TEST(TextureAnimationTest, FindsTheSecondRunOfAName)
  {
    const auto textures = MakeTextures({"+0button", "+abutton", "+1button", "+bbutton"});

    const TextureAnimation lit = TextureAnimation::Find(textures, 0);
    EXPECT_THAT(lit.frames, ElementsAre(0, 2));
    EXPECT_THAT(lit.alternate, ElementsAre(1, 3));

    // asked from the second run, the runs change places
    const TextureAnimation pressed = TextureAnimation::Find(textures, 3);
    EXPECT_THAT(pressed.frames, ElementsAre(1, 3));
    EXPECT_THAT(pressed.alternate, ElementsAre(0, 2));

    // a single picture with a second run still changes
    const auto button = MakeTextures({"+0slip", "+aslip"});
    EXPECT_TRUE(TextureAnimation::Find(button, 0).Changes());
  }

  TEST(TextureAnimationTest, ARunWithAGapIsShownWithoutIt)
  {
    const auto textures = MakeTextures({"+0lamp", "+3lamp", "+1lamp"});
    EXPECT_THAT(TextureAnimation::Find(textures, 0).frames, ElementsAre(0, 2, 1));
  }

  TEST(TextureAnimationTest, ShowsEachPictureForAFifthOfASecond)
  {
    EXPECT_EQ(TextureAnimation::ChooseFrame(3, 0.0), 0u);
    EXPECT_EQ(TextureAnimation::ChooseFrame(3, 0.19), 0u);
    EXPECT_EQ(TextureAnimation::ChooseFrame(3, 0.21), 1u);
    EXPECT_EQ(TextureAnimation::ChooseFrame(3, 0.41), 2u);
    EXPECT_EQ(TextureAnimation::ChooseFrame(3, 0.61), 0u);
    EXPECT_EQ(TextureAnimation::ChooseFrame(1, 100.0), 0u);
    EXPECT_EQ(TextureAnimation::ChooseFrame(0, 1.0), 0u);
    EXPECT_EQ(TextureAnimation::ChooseFrame(3, -1.0), 0u);
  }
}
