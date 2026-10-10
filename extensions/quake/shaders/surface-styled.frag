#version 450
#extension GL_GOOGLE_include_directive : require

// A wall whose light changes: as surface.frag draws a wall, with the light
// worked out from its lightmaps and what their styles are worth now, see
// styles.glsl. The colour of its corners says which styles those are, and
// is no color; the fourth comes with the face, see light-table.glsl.

#include "fragment.glsl"
#include "styles.glsl"

void main()
{
    vec4 texel = read_pixels(diffuse_texture, diffuse_sampler, tex_coord, dFdx(tex_coord), dFdy(tex_coord));
    vec3 light = light_of_styles(object.lightmap, lightmap_coord, vertex_color, light_code);

    // the light of a moment on top, as surface.frag adds it
    vec3 more = light_of_the_moment(world_position, lights_of(light_code));
    if (more != vec3(0.0))
    {
        vec3 kept = pow(light / object.lightmap.x, vec3(1.0 / 2.2));
        light = pow(min(kept + more, vec3(1.0)), vec3(2.2)) * object.lightmap.x;
    }
    vec3 shown = texel.rgb * light;

    // what glows is shown as it is, whatever the light
    if (object.emissive.w > 0.5)
    {
        vec4 glow = read_pixels(glow_texture, glow_sampler, tex_coord, dFdx(tex_coord), dFdy(tex_coord));
        shown = shown * (1.0 - glow.a) + glow.rgb;
    }

    frag_color = vec4(in_fog(shown, world_position), object_alpha(object.material, texel.a));
}
