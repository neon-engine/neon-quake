#version 450
#extension GL_GOOGLE_include_directive : require

// The sky. Its texture is two pictures side by side: on the right what is
// far, on the left the clouds in front of it, see-through where they are
// black. Both are laid on a dome over whoever looks, flattened so that it
// is far at the horizon, and both drift, the clouds twice as fast. A wall
// of sky shows the dome behind it, wherever the wall stands.

#include "fragment.glsl"

vec4 layer(vec2 at, float left)
{
    // one half of the texture, pixel by pixel, going round at its edges
    vec2 size = vec2(textureSize(sampler2D(diffuse_texture, diffuse_sampler), 0));
    vec2 half_size = vec2(size.x * 0.5, size.y);
    vec2 inside = (floor(fract(at) * half_size) + 0.5) / half_size;
    return textureLod(sampler2D(diffuse_texture, diffuse_sampler), vec2(left + inside.x * 0.5, inside.y), 0.0);
}

void main()
{
    // from the eyes to the wall, in the axes of the game: z is up there, and
    // the engine has it as y
    vec3 seen = world_position - scene.view_position.xyz;
    vec3 way = vec3(seen.x, -seen.z, seen.y * 3.0);
    way = normalize(way) * (6.0 * 63.0 / 128.0);

    float time = game_time();
    vec4 far = layer(way.xy + time * 8.0 / 128.0, 0.5);
    vec4 clouds = layer(way.xy + time * 16.0 / 128.0, 0.0);

    // black in the clouds is where they are not
    float there = step(0.004, dot(clouds.rgb, vec3(1.0)));
    frag_color = vec4(mix(far.rgb, clouds.rgb, there), 1.0);
}
