#version 320 es

precision mediump float;
in vec2 v_texcoord;
out vec4 fragColor;
uniform sampler2D tex;
uniform vec2 v_fullSize;
uniform mediump float time;

// https://www.shadertoy.com/view/sdK3z1

uniform mediump float loudness;
uniform mediump float subwoofer;
uniform mediump float subtone;
uniform mediump float kickdrum;
uniform mediump float lowBass;
uniform mediump float bassBody;
uniform mediump float midBass;
uniform mediump float warmth;
uniform mediump float lowMids;
uniform mediump float midsMody;
uniform mediump float upperMids;
uniform mediump float attack;
uniform mediump float highs;

// const float loudness = 0.6;
// const float time = 1.0;
// const float display_framerate = 30.0;
const float display_framerate = 60.0;
const vec2 display_resolution = vec2(2256.0, 1504.0);

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec2 computeCubicDistortionUV(vec2 uv, float k, float kcube) {
  vec2 t = uv - .5;
  float r2 = t.x * t.x + t.y * t.y;
  r2 = r2 * 0.2;
  float f = 0.;

  if (kcube == 0.0) {
    f = 1. + r2 * k;
  } else {
    f = 1. + r2 * (k + kcube * sqrt(r2));
  }

  vec2 nUv = f * t + .5;

  return nUv;
}

vec3 applyCubicDistortion(sampler2D tex, vec2 uv, float distortion,
                          float extra) {
  // https://www.shadertoy.com/view/4lSGRw

  float offset = .1 * sin(time * .5);

  float red =
      texture(tex,
              computeCubicDistortionUV(uv, distortion * 0.5 + offset, extra))
          .r;
  float green = texture(tex, computeCubicDistortionUV(uv, distortion, extra)).g;
  float blue =
      texture(tex, computeCubicDistortionUV(uv, distortion - offset, extra)).b;

  return vec3(red, green, blue);
}

vec3 getBloom(sampler2D tex, vec2 uv, float size, float brightness) {
  // BLOOM
  const float blur_directions =
      24.0;                       // default is 12.0 but 24.0+ will look bestest
  const float blur_quality = 4.0; // default is 3.0  but 4.0+  will look bestest
  // const float blur_size = 12.0;   // radius in pixels
  // const float blur_brightness = 6.5; // radius in pixels
  vec2 blur_radius = size / (display_resolution.xy * 0.5);

  // Blur calculations may be expensive: blur_directions * blur_quality amount
  // of iterations 12 * 3 = 36 iterations per pixel by default
  vec3 bloomColor = vec3(0.0);
  for (float d = 0.0; d < 6.283185307180;
       d += 6.283185307180 / blur_directions) {
    for (float i = 1.0 / blur_quality; i <= 1.0; i += 1.0 / blur_quality) {
      vec3 toAdd =
          texture(tex, uv + vec2(cos(d), sin(d)) * blur_radius * i).rgb;
      toAdd *= brightness * vec3(1.0, 0.55, 0.40);
      bloomColor += toAdd;
    }
  }

  bloomColor /= blur_quality * blur_directions;
  bloomColor = bloomColor * bloomColor * bloomColor;

  return bloomColor;
}

// New function to create pixelated texture coordinates
vec2 pixelate(vec2 coord, float pixelSize) {
  // https://www.shadertoy.com/view/Dtt3W2
  return floor(coord * pixelSize) / pixelSize;
}

vec2 pixelDistortionUV(vec2 uv, float strength) {

  // Pixelation factor (higher values for more pixelation)
  float pixelationFactor = sin(time * 0.5) * 50.0 + 50.0 +
                           hash(vec2(time, cos(time / 2.0) * 12.0)) * 80.05;
  ;

  // Use pixelated texture coordinates for displacement
  vec2 pixelatedCoord = pixelate(uv, pixelationFactor * strength);
  // pixelatedCoord.x +=

  pixelatedCoord.x += sin(pixelatedCoord.x * 10.0 + time * 80.0) * 0.01;

  return pixelatedCoord;
}

void main() {
  vec2 uv = v_texcoord;

  float timeMod = mod(time * 100.0, 32.0) / 110.0;

  bool isPixelating = true;
  if (isPixelating) {

    uv = pixelDistortionUV(uv, 1.);
  }

  vec4 pixColor = texture(tex, uv);

  vec3 color = pixColor.rgb;

  float cubicStrength = -1.0;
  float cubicStrengthExtra = -0.5;
  vec2 uvDistorted =
      computeCubicDistortionUV(uv, cubicStrength, cubicStrengthExtra);
  color = applyCubicDistortion(tex, uv, -1.0, -0.5);

  color = tanh(color + getBloom(tex, uvDistorted, 5.0, 1.5));

  fragColor = vec4(color, 1.);
}
