#version 330 core

in  float vPhase;
out vec4  FragColor;

uniform float time;

void main()
{
    // 각 반딧불이마다 다른 타이밍으로 깜빡임
    float flicker = 0.5 + 0.5 * sin(time * 2.5 + vPhase * 3.7);

    FragColor = vec4(0.2, 1.0, 0.1, flicker);
}
