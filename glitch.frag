precision mediump float;
varying vec2 v_texcoord;
uniform sampler2D tex;

uniform mediump float time;
uniform mediump float loudness;


// const float display_framerate = 30.0;
const float display_framerate = 60.0;
// const vec2 display_resolution = vec2(1128.0, 752.0);
const vec2 display_resolution = vec2(2256.0, 1504.0);

vec2 curve(vec2 uv) {
  uv = (uv - 0.5) * 2.0;
  uv *= 1.1;
  uv.x *= 1.0 + pow((abs(uv.y) / 8.0), 2.0);
  uv.y *= 1.0 + pow((abs(uv.x) / 8.0), 2.0);
  uv = (uv / 2.0) + 0.5;
  uv = uv * 0.92 + 0.04;
  return uv;
}

float rand(vec2 uv, float t) {
  return fract(sin(dot(uv, vec2(1225.6548, 321.8942))) * 4251.4865 + t);
}

void main() {
  vec2 uv = v_texcoord;
  // comment out to disable barrel distortion.
  // if cursor is misaligned - enable software cursor rendering
  // by setting envvar: run "export WLR_NO_HARDWARE_CURSORS=1"
  // before launching Hyprland
  // uv = curve(uv);
  vec4 pixColor = texture2D(tex, uv);

  vec3 col = pixColor.rgb;
  col += loudness;


  gl_FragColor = vec4( col, 1.0 );
}
