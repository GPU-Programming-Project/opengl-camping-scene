#version 330 core

// --- 인스턴스별 데이터 (VBO, glVertexAttribDivisor(location, 1)) ---
layout (location = 0) in vec3  aBasePos; // 각 반딧불이의 기준 위치
layout (location = 1) in float aPhase;   // 애니메이션 위상 (0 ~ 2π, 개체마다 다름)
layout (location = 2) in float aSpeed;   // 이동 속도 배율 (개체마다 다름)

uniform mat4  view;
uniform mat4  projection;
uniform float time;

// fragment shader에서 깜빡임 계산에 재사용
out float vPhase;

void main()
{
    // sin/cos 조합으로 랜덤한 유영 궤도 생성
    // 축마다 다른 주파수·위상을 써서 반복 패턴이 눈에 띄지 않게 함
    vec3 pos = aBasePos;
    pos.x += sin(time * aSpeed        + aPhase)        * 0.8;
    pos.y += cos(time * aSpeed * 0.7  + aPhase * 1.3)  * 0.4;
    pos.z += sin(time * aSpeed * 0.5  + aPhase * 0.7)  * 0.8;

    gl_Position = projection * view * vec4(pos, 1.0);

    // 카메라에서 멀수록 점이 작아지도록 원근감 적용
    float dist   = length((view * vec4(pos, 1.0)).xyz);
    gl_PointSize = max(1.5, 5.0 / dist * 10.0);

    vPhase = aPhase;
}
