#version 450
#extension GL_GOOGLE_include_directive : require

// What the eyes see in water, in slime, and in lava: the whole picture
// waves. Each way a pixel is pushed by a wave that runs along the other,
// two and a half waves over the picture and a three hundredth of it far, as
// the ports of today have it. The picture is drawn a little smaller than
// it is, by as much as the waves reach, so that no wave reads past an edge.
//
// It is an effect of the camera, on the light of its scene: the game names
// it while the eyes are in a liquid, see GameCode::ShowHud.

#include "effect.glsl"

void main()
{
    vec2 size = frame_size();
    float shape = size.x / size.y;

    const float waves = 3.14159265 * 5.0;
    vec2 reach = vec2(1.0 / 300.0, shape / 300.0);
    vec2 along = vec2(waves, waves * shape);

    vec2 pushed = frame_coord + sin(frame_coord.yx * along + scene.time.x) * reach;
    frag_color = read_frame(pushed * (1.0 - 2.0 * reach) + reach);
}
