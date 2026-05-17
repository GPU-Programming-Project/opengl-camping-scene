#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D sceneTexture;   // 원본 씬
uniform sampler2D bloomTexture;   // blur된 밝은 부분
uniform bool bloomEnabled;

void main()
{
    vec3 scene = texture(sceneTexture, TexCoords).rgb;

    if (bloomEnabled) {
        vec3 bloom = texture(bloomTexture, TexCoords).rgb;
        // 원본 씬 + bloom 빛 번짐 추가 적용
        FragColor = vec4(scene + bloom, 1.0);
    } else {
        FragColor = vec4(scene, 1.0);
    }
}
