#version 330 core
layout(location = 0) in vec3 p;
layout(location = 1) in vec2 t;
out vec2 TexCoord;
uniform mat4 m, v, pr;
uniform float uvScale;
void main() {
    gl_Position = pr * v * m * vec4(p, 1.0);
    TexCoord = t * uvScale;
}