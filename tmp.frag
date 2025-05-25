precision mediump float;
varying vec2 v_texcoord;
uniform sampler2D tex;
uniform vec2 fullSize;
uniform mediump float time;

// https://www.shadertoy.com/view/4lSGRw
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

vec2 computeCubicDistortionUV(vec2 uv, float k, float kcube) {

  vec2 t = uv - .5;
  float r2 = t.x * t.x + t.y * t.y;
  r2 = r2 * r2;
  float f = 0.;

  if (kcube == 0.0) {
    f = 1. + r2 * k;
  } else {
    f = 1. + r2 * (k + kcube * sqrt(r2));
  }

  vec2 nUv = f * t + .5;

  return nUv;
}

void main() {
  vec2 uv = v_texcoord;

  vec4 pixColor = texture2D(tex, uv);
  // should both be negative for outwards distortion
  float k = -1.0;
  float kcube = -.5;

  float offset = .1 * sin(time * .5);

  float red = texture2D(tex, computeCubicDistortionUV(uv, k + offset, kcube)).r;
  float green = texture2D(tex, computeCubicDistortionUV(uv, k, kcube)).g;
  float blue =
      texture2D(tex, computeCubicDistortionUV(uv, k - offset, kcube)).b;

  gl_FragColor = vec4(red, green, blue, 1.);
}
