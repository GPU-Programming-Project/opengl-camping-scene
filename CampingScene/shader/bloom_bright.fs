#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D screenTexture;

void main()
{
    vec3 color = texture(screenTexture, TexCoords).rgb;

    // 밝기 계산 과정
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));

    //0.5 이상인 것들만 통과하도록 하기
    if (brightness > 0.5) // 꽃들과 삽, 달이 해당됨!
        FragColor = vec4(color, 1.0);
    else
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
