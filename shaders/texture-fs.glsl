layout(binding=0) uniform sampler2D image;
layout(binding=1) uniform sampler2D depth;
in vec2 uv;
out vec4 color;

void main() {
    color = texture(image, uv);
    gl_FragDepth = texture(depth, uv).r;
}
