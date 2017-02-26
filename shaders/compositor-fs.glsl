in vec2 uv;
out vec4 FragColor;

void main() {
    FragColor = abs(vec4(sin(uv.x), cos(uv.y), tan(uv.x*uv.y), 1));
}
