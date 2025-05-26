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

const float display_framerate = 60.0;
const vec2 display_resolution = vec2(2256.0, 1504.0);

#define PI 3.14159265

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float iHash(vec2 _v, vec2 _r) {
  float h00 = hash(vec2(floor(_v * _r + vec2(0.0, 0.0)) / _r));
  float h10 = hash(vec2(floor(_v * _r + vec2(1.0, 0.0)) / _r));
  float h01 = hash(vec2(floor(_v * _r + vec2(0.0, 1.0)) / _r));
  float h11 = hash(vec2(floor(_v * _r + vec2(1.0, 1.0)) / _r));
  vec2 ip = vec2(smoothstep(vec2(0.0, 0.0), vec2(1.0, 1.0), mod(_v * _r, 1.)));
  return (h00 * (1. - ip.x) + h10 * ip.x) * (1. - ip.y) +
         (h01 * (1. - ip.x) + h11 * ip.x) * ip.y;
}

float noise(vec2 _v) {
  float sum = 0.;
  for (int i = 1; i < 9; i++) {
    sum +=
        iHash(_v + vec2(i), vec2(2. * pow(2., float(i)))) / pow(2., float(i));
  }
  return sum;
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

  float offset = .1 * sin(time * .5) * distortion;

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
  float pixelationFactor = sin(time * 0.5) * 50.0 + 70.0 +
                           hash(vec2(time, cos(time / 2.0) * 12.0)) * 50.0;
  ;

  // Use pixelated texture coordinates for displacement
  vec2 pixelatedCoord = pixelate(uv, pixelationFactor * strength);
  // pixelatedCoord.x +=

  pixelatedCoord.x += sin(pixelatedCoord.x * 10.0 + time * 80.0) * 0.01;

  return pixelatedCoord;
}

// float timeMod = mod(time * 100.0, 32.0) / 110.0;

void main() {
  vec2 uv = v_texcoord;

  float bass = (subwoofer + subtone + kickdrum * 0.25) / 2.25;

  // rms: 0.27832186| subwoofer 0.9590597| subtone 0| kickdrum 1| lowBass 0|
  // bassBody 0| midBass 1| warmth 0.60537446| lowMids 0.4150363| midsMody
  // 0.31651106| upperMids 0.004805756| attack 0| highs 0
  bool isPixelating = loudness > 0.26 && subwoofer > 0.9 && kickdrum > 0.9 &&
                      midBass > 0.8 && warmth > 0.5 && lowMids > 0.5 &&
                      hash(vec2(time, sin(time))) > 0.3;
  if (isPixelating) {
    uv = pixelDistortionUV(uv, 1.);
  }

  // tape crease
  float tcPhase =
      clamp((sin(uv.y * 8.0 - time * (lowBass + bassBody + midBass)) - 0.96) *
                noise(vec2(time)),
            0.0, 0.01) *
      (10.0 * (midsMody) + (midBass + bass + lowBass) * 4.);
  float tcNoise = max(noise(vec2(uv.y * 100.0, time * 10.0)) - 0.5, 0.0);
  uv.x = uv.x - tcNoise * tcPhase;

  // switching noise
  float snPhase = smoothstep(0.03, 0.0, uv.y) * warmth * warmth * warmth;
  uv.y += snPhase * 0.3;
  uv.x +=
      snPhase *
      ((noise(vec2(uv.y * 100.0, time * 10.0 + warmth + lowBass)) - 0.5) * 0.2);

  vec4 pixColor = texture(tex, uv);

  vec3 color = pixColor.rgb;

  // Cubic distortion
  float cubicStrength = -1.0 * bass;
  float cubicStrengthExtra = -1.0 * lowBass;
  vec2 uvDistorted =
      computeCubicDistortionUV(uv, cubicStrength, cubicStrengthExtra);
  color = applyCubicDistortion(tex, uv, cubicStrength, cubicStrengthExtra);

  // BLOOM
  float bloomStrength = (midsMody + attack * 2.0 + upperMids) / 4.0 * 5.0;
  float bloomBrightness = bloomStrength / 2. * attack * 5.0 + 1.;
  color =
      tanh(color + getBloom(tex, uvDistorted, bloomStrength, bloomBrightness));

  // Extra vhs bloom
  for (float x = -4.0; x < 2.5; x += 1.0) {
    color += vec3(texture(tex, uv + vec2(x - 0.0, 0.0) * 7E-3).r,
                  texture(tex, uv + vec2(x - 2.0, 0.0) * 7E-3).g,
                  texture(tex, uv + vec2(x - 4.0, 0.0) * 7E-3).b) *
             0.1;
  }
  color *= 0.6;

  // color *= bass + 1.0;
  fragColor = vec4(color, 1.);
}
