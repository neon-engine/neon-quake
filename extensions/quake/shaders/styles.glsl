// The light of a face whose light changes: one that flickers, pulses, or is
// switched by the game.
//
// Such a face has up to four lightmaps, each of a style, and what is shown
// is their sum, each times what its style is worth at the moment. The
// lightmaps are handed to the graphics card once, apart, in bands of one
// picture, a quarter of it each; what the styles are worth comes in a table
// of 64 pixels that is all the game sets again when a light ticks. So the
// sum is worked out here, as a face is drawn, and no lightmap is ever sent
// a second time.
//
// The table is the second texture of the material: the worth of style `n`
// in 256ths, the low byte in red and the high byte in green of pixel `n`.
// The styles of a face are the colour of its corners, each a number over
// 255, and 255 itself for no further lightmap.

layout (set = 0, binding = 3) uniform texture2D style_table;
layout (set = 0, binding = 6) uniform sampler style_sampler;

const int LIGHTMAPS_OF_A_FACE = 4;
const int LIGHT_STYLES = 64;

// What a style is worth now: 1 for a light as the level has it.
float worth_of_style(int style)
{
    // a style no light of the game has is always worth 1
    if (style >= LIGHT_STYLES) { return 1.0; }

    vec4 kept = texelFetch(sampler2D(style_table, style_sampler), ivec2(style, 0), 0);
    return (kept.r * 255.0 + kept.g * 255.0 * 256.0) / 256.0;
}

// From light to the number a lightmap keeps for it, and back: the level
// keeps bytes that a screen is given as they are.
vec3 as_kept(vec3 light)
{
    return mix(light * 12.92, 1.055 * pow(max(light, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055, step(0.0031308, light));
}

vec3 as_light(vec3 kept)
{
    return mix(kept / 12.92, pow((kept + 0.055) / 1.055, vec3(2.4)), step(0.04045, kept));
}

// The light of a face at a place of its first lightmap, in linear light
// with its strength multiplied in, as baked_light() gives that of a face
// whose light stays. The original sums the bytes of the lightmaps and keeps
// the sum to what a byte holds, so the sum is of those.
vec3 light_of_styles(vec4 lightmap, vec2 coord, vec4 styles)
{
    vec3 sum = vec3(0.0);
    for (int i = 0; i < LIGHTMAPS_OF_A_FACE; i++)
    {
        int style = int(styles[i] * 255.0 + 0.5);
        if (style >= 255) { break; }

        vec2 place = coord + vec2(0.0, float(i) / float(LIGHTMAPS_OF_A_FACE));
        vec3 kept = as_kept(texture(sampler2D(lightmap_texture, lightmap_sampler), place).rgb);
        sum += kept * worth_of_style(style);
    }
    return as_light(min(sum, vec3(1.0))) * lightmap.x;
}
