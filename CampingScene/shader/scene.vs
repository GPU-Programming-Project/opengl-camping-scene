#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aColor;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 VertColor;

uniform mat4  model;
uniform mat4  view;
uniform mat4  projection;
uniform bool  isOutline;
uniform float outlineSize;

void main()
{
    // outline이 true라면 vertex를 normal 방향으로 밀어내 확대한다.
    vec3 pos = isOutline ? aPos + aNormal * outlineSize : aPos;
    FragPos    = vec3(model * vec4(pos, 1.0));
    Normal     = mat3(transpose(inverse(model))) * aNormal;
    TexCoord   = aTexCoord;
    VertColor  = aColor;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
