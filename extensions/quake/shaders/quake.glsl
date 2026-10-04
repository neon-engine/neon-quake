// What the shaders of the game share.
//
// The time. The engine tells every shader how long the world has run.
float game_time()
{
    return scene.time.x;
}

// How bright the player set the game: the power every colour is taken to,
// which the game sets as the third of its numbers, from 1 for as the
// pictures are down to 0.5 for the brightest. The original does this to the
// colours a screen is given, and a power of them is the same power of the
// light they stand for.
vec3 as_bright_as_set(vec3 shown)
{
    float power = scene.numbers[2].x;
    return power <= 0.0 || power == 1.0 ? shown : pow(max(shown, vec3(0.0)), vec3(power));
}

// The fog of a level over what is drawn at a place of the world. It is as
// the ports of today have it: what is seen fades into the colour of the fog
// with the square of how far it is, counted in the units of the game, 32 to
// a metre. What comes out is as bright as the player set the game. The game sets how thick the fog is as the first of its numbers
// for the shaders, and its colour as the second, see GameCode::Run. A level
// without fog has none of it.
vec3 in_fog(vec3 shown, vec3 at)
{
    float density = scene.numbers[0].x;
    if (density <= 0.0) { return as_bright_as_set(shown); }

    float far = length(at - scene.view_position.xyz) * 32.0 * density / 64.0;
    return as_bright_as_set(mix(scene.numbers[1].rgb, shown, clamp(exp(-far * far), 0.0, 1.0)));
}

// The sky is not at any distance. It takes half the colour of the fog, as
// the ports have it.
vec3 sky_in_fog(vec3 shown)
{
    return as_bright_as_set(scene.numbers[0].x <= 0.0 ? shown : mix(shown, scene.numbers[1].rgb, 0.5));
}

// The light of what flashes and burns for a moment: an explosion, the shot
// of a gun, a rocket on its way. The game makes a point light of the engine
// for each, with its colour as `diffuse` and, as `constant`, how far it
// reaches in metres. As in the original the light falls off evenly to
// nothing at that distance, and a wall 64 units inside of it is lit as
// bright as a texture is. What comes out is added to the light of a level
// as the level keeps it: 1 for twice as bright as the texture.
vec3 light_of_the_moment(vec3 at)
{
    vec3 sum = vec3(0.0);
    int count = min(scene.light_counts.x, MAX_POINT_LIGHTS);
    for (int i = 0; i < count; i++)
    {
        float left = scene.point_lights[i].attenuation.x - distance(at, scene.point_lights[i].position.xyz);
        if (left > 0.0) { sum += scene.point_lights[i].diffuse.rgb * left; }
    }

    // 32 units to a metre, and 127.5 units of reach for as bright again as
    // the texture
    return sum * (32.0 / 127.5);
}

// A texture read pixel by pixel, as the game shows its pictures: the pixel
// the place lies in, and not what lies between it and its neighbours. Far
// away, where a pixel of the screen covers many of the picture, the smaller
// copies of the picture are read, so that it does not glitter.
vec4 read_pixels(texture2D picture, sampler through, vec2 at, vec2 across, vec2 down)
{
    vec2 size = vec2(textureSize(sampler2D(picture, through), 0));
    vec2 middle = (floor(at * size) + 0.5) / size;
    return textureGrad(sampler2D(picture, through), middle, across, down);
}
