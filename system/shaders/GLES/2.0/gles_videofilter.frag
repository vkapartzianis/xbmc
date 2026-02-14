#version 100

precision mediump float;

uniform sampler2D img;
varying vec2 cord;

void main()
{
  // gl_FragColor = texture2D(img, cord);

  vec3 rgb = texture2D(img, cord).rgb;
  rgb = (rgb - vec3(16.0 / 255.0)) * (255.0 / 219.0);
  rgb = clamp(rgb, 0.0, 1.0);
  gl_FragColor = vec4(rgb, 1.0);
}
