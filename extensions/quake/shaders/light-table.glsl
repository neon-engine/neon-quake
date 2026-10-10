// The table of the light of the game: a picture the game sets again when
// the light changes, and the second texture of every face of the level
// that has light.
//
// Its first row is what each of the 64 styles of light is worth now, see
// styles.glsl: the worth of style `n` in 256ths, the low byte in red and
// the high byte in green of pixel `n`. The rows below are the masks of the
// faces of the level, one pixel a face, 64 to a row: which lights of the
// moment reach the face, a bit for each, the low byte in red up to the
// high byte in alpha, see light_of_the_moment() in quake.glsl and
// BspLightVisibility.
//
// A corner says which face it is of, and its fourth style, as one number:
// the number of the face plus one, times 256, plus the style. 0 for the
// face is a corner of no face of the level that is shown: of a model, an
// item, a door, which every light reaches.

layout (set = 0, binding = 3) uniform texture2D light_table;
layout (set = 0, binding = 6) uniform sampler light_table_sampler;

const int LIGHT_TABLE_WIDTH = 64;
const int LIGHT_STYLES = 64;

// What a style is worth now: 1 for a light as the level has it.
float worth_of_style(int style)
{
    // a style no light of the game has is always worth 1
    if (style >= LIGHT_STYLES) { return 1.0; }

    vec4 kept = texelFetch(sampler2D(light_table, light_table_sampler), ivec2(style, 0), 0);
    return (kept.r * 255.0 + kept.g * 255.0 * 256.0) / 256.0;
}

// The fourth style of the face of a corner, from the number of the corner.
int fourth_style_of(int code)
{
    return code & 255;
}

// Which lights of the moment reach the face of a corner, a bit for each,
// from the number of the corner: all of them for a corner of no face.
uint lights_of(int code)
{
    int face = code / 256;
    if (face == 0) { return 0xFFFFFFFFu; }

    face -= 1;
    ivec2 at = ivec2(face % LIGHT_TABLE_WIDTH, 1 + face / LIGHT_TABLE_WIDTH);
    uvec4 bytes = uvec4(texelFetch(sampler2D(light_table, light_table_sampler), at, 0) * 255.0 + 0.5);
    return bytes.r | (bytes.g << 8) | (bytes.b << 16) | (bytes.a << 24);
}
