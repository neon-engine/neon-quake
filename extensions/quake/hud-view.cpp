#include "hud-view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "formats/picture-reader.hpp"
#include "formats/picture.hpp"

namespace quake
{
  using neon::extension::World;

  const HudView::Known &HudView::Find(
    const World &world,
    const GameData &data,
    const std::string_view name,
    const std::int32_t first_row,
    const std::int32_t rows)
  {
    // a strip of a picture is known apart from the whole of it
    const bool is_strip = first_row > 0 || rows > 0;
    const std::string key = is_strip
                              ? std::string(name) + "#" + std::to_string(first_row) + "+" + std::to_string(rows)
                              : std::string(name);
    if (const auto known = _pictures.find(key); known != _pictures.end()) { return known->second; }

    Known &made = _pictures[key];

    if (!_was_read)
    {
      _was_read = true;
      std::string problem;
      _has_wad = _wad.Read(data.Find("gfx.wad"), problem);
      if (!_has_wad) { world.Warn("The pictures of the status bar cannot be read: " + problem); }
    }

    // a picture of the `gfx/` folder is a file of its own; any other is in
    // the archive of the status bar
    Picture picture;
    std::string problem;
    const bool is_file = name.starts_with("gfx/");
    const bool was_read = is_file
                            ? read_picture_file(name, data.Find(name), picture, problem)
                            : _has_wad && _wad.ReadPicture(name, picture, problem);
    if (!was_read)
    {
      world.Warn("The picture " + std::string(name) + " is not shown: " + problem);
      return made;
    }

    // the rows that are asked for alone, as many as the picture has
    if (is_strip)
    {
      const std::int32_t first = std::clamp(first_row, 0, picture.height);
      const std::int32_t count = rows > 0 ? std::min(rows, picture.height - first) : picture.height - first;
      if (count <= 0)
      {
        world.Warn("The picture " + std::string(name) + " has no row " + std::to_string(first_row));
        return made;
      }
      const auto from = picture.pixels.begin() + static_cast<std::ptrdiff_t>(first * picture.width);
      picture.pixels = std::vector<std::uint8_t>(from, from + static_cast<std::ptrdiff_t>(count * picture.width));
      picture.height = count;
    }

    // The letters are see-through where they have the first colour of the
    // palette, every other picture where it has the last.
    std::vector<std::uint8_t> pixels;
    if (name == HudPicture::characters_name)
    {
      std::vector<std::uint8_t> holes = picture.pixels;
      for (std::uint8_t &pixel : holes) { if (pixel == 0) { pixel = 255; } }
      pixels = data.GetPalette().ToRgba(holes, true);
    } else
    {
      pixels = data.GetPalette().ToRgba(picture.pixels, true);
    }

    made.path = world.SetImage(
      "hud/" + key,
      static_cast<std::uint32_t>(picture.width),
      static_cast<std::uint32_t>(picture.height),
      pixels);
    made.width = picture.width;
    made.height = picture.height;
    return made;
  }

  const std::string &HudView::FindLetter(const World &world, const GameData &data, const std::int32_t letter)
  {
    if (const auto known = _letters.find(letter); known != _letters.end()) { return known->second; }

    std::string &made = _letters[letter];

    // the sheet is an archive lump without a size of its own: 16 letters
    // in a row and 16 rows, each 8 pixels
    Picture sheet;
    std::string problem;
    if (!_was_read) { Find(world, data, HudPicture::characters_name); }
    if (!_has_wad || !_wad.ReadPicture(HudPicture::characters_name, sheet, problem)) { return made; }

    const std::int32_t size = HudPicture::character_size;
    const std::int32_t column = letter % 16;
    const std::int32_t row = letter / 16;
    if (sheet.width < 16 * size || sheet.height < 16 * size) { return made; }

    // a letter is see-through where it has the first colour of the palette
    std::vector<std::uint8_t> holes(static_cast<std::size_t>(size * size));
    for (std::int32_t y = 0; y < size; y++)
    {
      for (std::int32_t x = 0; x < size; x++)
      {
        const std::uint8_t pixel =
          sheet.pixels[static_cast<std::size_t>((row * size + y) * sheet.width + column * size + x)];
        holes[static_cast<std::size_t>(y * size + x)] = pixel == 0 ? 255 : pixel;
      }
    }
    made = world.SetImage(
      "hud/letter-" + std::to_string(letter),
      static_cast<std::uint32_t>(size),
      static_cast<std::uint32_t>(size),
      data.GetPalette().ToRgba(holes, true));
    return made;
  }

  bool HudView::Open(const World &world)
  {
    if (_is_open) { return _holder != 0; }

    _is_open = true;
    if (!world.ShowUi(file))
    {
      world.Warn("Nothing is shown over the world: " + std::string(file) + " cannot be shown");
      return false;
    }
    _tint = world.FindUi("quake-tint");
    _holder = world.FindUi("quake-pictures");
    return _holder != 0;
  }

  void HudView::ShowTint(const World &world, const float red, const float green, const float blue, const float amount)
  {
    if (!Open(world) || _tint == 0) { return; }

    char colour[64];
    std::snprintf(
      colour, sizeof(colour), "rgba(%d, %d, %d, %.3f)",
      static_cast<int>(std::clamp(red, 0.0f, 255.0f)),
      static_cast<int>(std::clamp(green, 0.0f, 255.0f)),
      static_cast<int>(std::clamp(blue, 0.0f, 255.0f)),
      static_cast<double>(std::clamp(amount, 0.0f, 1.0f)));
    if (_tint_colour == colour) { return; }

    _tint_colour = colour;
    world.SetUiStyle(_tint, "background-color", _tint_colour);
  }

  std::int32_t HudView::GetWidth(const World &world, const GameData &data, const std::string_view name)
  {
    return Find(world, data, name).width;
  }

  void HudView::Show(const World &world, const GameData &data, const std::vector<HudPicture> &pictures)
  {
    if (!Open(world)) { return; }

    int view_width = 0;
    int view_height = 0;
    if (!world.GetViewSize(view_width, view_height) || view_height <= 0) { return; }
    if (pictures == _shown && view_width == _view_width && view_height == _view_height) { return; }

    _shown = pictures;
    _view_width = view_width;
    _view_height = view_height;

    // how many pixels of the real screen a pixel of the pictures is, and
    // one of the lines at the top, which is a whole number of them
    const float scale = static_cast<float>(view_height) / screen_height;
    const float notice_scale = std::max(1.0f, std::round(static_cast<float>(view_height) / notice_height));

    // a place in pixels of the real screen, as a length of the file
    const auto to_length = [scale](const std::int32_t pixels)
    {
      char text[32];
      std::snprintf(text, sizeof(text), "%.4fpx", static_cast<double>(static_cast<float>(pixels) / scale));
      return std::string(text);
    };

    for (auto &[path, used] : _used) { used = 0; }
    std::int32_t order = 0;
    for (const HudPicture &command : pictures)
    {
      const bool is_letter = command.kind == HudPictureKind::Character;
      std::string path;
      float width = static_cast<float>(HudPicture::character_size);
      float height = width;
      if (is_letter)
      {
        path = FindLetter(world, data, command.character & 255);
      } else
      {
        const Known &known = Find(world, data, command.name, command.first_row, command.rows);
        path = known.path;
        width = static_cast<float>(known.width);
        height = static_cast<float>(known.height);
      }
      if (path.empty()) { continue; }

      // from the screen of the original to the real one, see HudPicture
      const auto x = static_cast<float>(command.x);
      const auto y = static_cast<float>(command.y);
      const auto across = static_cast<float>(view_width);
      const auto down = static_cast<float>(view_height);
      float by = scale;
      float left = (across - 320.0f * scale) * 0.5f + x * scale;
      float top = 0.0f;
      switch (command.anchor)
      {
        case HudAnchor::Bottom: top = down - (200.0f - y) * scale;
          break;
        case HudAnchor::Center: top = (down - 200.0f * scale) * 0.5f + y * scale;
          break;
        case HudAnchor::TopLeft: by = notice_scale;
          left = x * by;
          top = y * by;
          break;
        case HudAnchor::MiddleSmall: by = notice_scale;
          left = across * 0.5f + x * by;
          top = down * 0.5f + y * by;
          break;
      }

      // whole pixels, so that every pixel of a picture is as large as the
      // next one wherever it can be
      Slot wanted;
      wanted.left = static_cast<std::int32_t>(std::round(left));
      wanted.top = static_cast<std::int32_t>(std::round(top));
      wanted.width = static_cast<std::int32_t>(std::round(left + width * by)) - wanted.left;
      wanted.height = static_cast<std::int32_t>(std::round(top + height * by)) - wanted.top;
      wanted.order = order++;

      std::vector<Slot> &slots = _slots[path];
      std::size_t &used = _used[path];
      if (used == slots.size())
      {
        Slot made;
        made.element = world.CreateUi(
          "type: image\n"
          "src: " + path + "\n"
          "position: absolute\n"
          "image_rendering: pixelated\n",
          _holder);
        if (made.element == 0) { return; }
        made.is_visible = true;
        // a place no picture has, so that all of it is set below
        made.width = -1;
        made.height = -1;
        made.left = std::numeric_limits<std::int32_t>::min();
        made.top = std::numeric_limits<std::int32_t>::min();
        slots.push_back(made);
      }

      Slot &slot = slots[used];
      used++;
      if (slot.left != wanted.left)
      {
        slot.left = wanted.left;
        world.SetUiStyle(slot.element, "left", to_length(slot.left));
      }
      if (slot.top != wanted.top)
      {
        slot.top = wanted.top;
        world.SetUiStyle(slot.element, "top", to_length(slot.top));
      }
      if (slot.width != wanted.width)
      {
        slot.width = wanted.width;
        world.SetUiStyle(slot.element, "width", to_length(slot.width));
      }
      if (slot.height != wanted.height)
      {
        slot.height = wanted.height;
        world.SetUiStyle(slot.element, "height", to_length(slot.height));
      }
      if (slot.order != wanted.order)
      {
        slot.order = wanted.order;
        world.SetUiStyle(slot.element, "z-index", std::to_string(slot.order));
      }
      if (!slot.is_visible)
      {
        slot.is_visible = true;
        world.SetUiVisible(slot.element, true);
      }
    }

    // the images the list has no picture for
    for (auto &[path, slots] : _slots)
    {
      for (std::size_t rest = _used[path]; rest < slots.size(); rest++)
      {
        if (!slots[rest].is_visible) { continue; }

        slots[rest].is_visible = false;
        world.SetUiVisible(slots[rest].element, false);
      }
    }
  }
} // quake
