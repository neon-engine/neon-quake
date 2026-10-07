#include "basedir-file.hpp"

#include <gtest/gtest.h>

namespace
{
  using quake::BasedirFile;

  TEST(BasedirFileTest, ReadsWhatItWrites)
  {
    EXPECT_EQ(BasedirFile::Read(BasedirFile::Write("librequake")), "librequake");
  }

  TEST(BasedirFileTest, ReadsANameWrittenByHand)
  {
    EXPECT_EQ(BasedirFile::Read("version: 1\nbasedir:   steam   \n"), "steam");
    EXPECT_EQ(BasedirFile::Read("version: 1\r\nbasedir: steam\r\n"), "steam");
    EXPECT_EQ(BasedirFile::Read("basedir: \"my copy\" # the one from the shelf\n"), "my copy");
    EXPECT_EQ(BasedirFile::Read("basedir: 'steam'"), "steam");
  }

  TEST(BasedirFileTest, ReadsNothingWithoutAName)
  {
    EXPECT_EQ(BasedirFile::Read(""), "");
    EXPECT_EQ(BasedirFile::Read("version: 1\n"), "");
    EXPECT_EQ(BasedirFile::Read("basedir:\n"), "");
    EXPECT_EQ(BasedirFile::Read("# basedir: steam\n"), "");
    EXPECT_EQ(BasedirFile::Read("other:\n  basedir: steam\n"), "") << "only a name at the top counts";
  }
} // namespace
