#version 330 core
in vec3 TexCoords;

out vec4 FragColor;

uniform samplerCube skybox;

void main()
{
    FragColor = texture(skybox, TexCoords) * 2.0; // 반짝이는 느낌을 추가하기
}
