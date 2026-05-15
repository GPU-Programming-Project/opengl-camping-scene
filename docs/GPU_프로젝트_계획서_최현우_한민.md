# GPU 프로그래밍 프로젝트 계획서

**OpenGL / Assimp 기반 3D 캠핑 Scene 구현**

**팀원:** 최현우, 한 민
**프로젝트 형태:** 2인 팀 프로젝트

## 1. 프로젝트 주제

Skybox, Point Light, Alpha Blending, Framebuffer Post-processing 등을 활용한 3D 캠핑 Scene 구현

LearnOpenGL을 기반으로 구현하며, 모델 로딩에는 Assimp 라이브러리를 사용합니다.

## 2. Scene 구성

**배경:** 숲속 캠핑장

**주요 오브젝트:**
- 텐트, 모닥불, 고기 꼬치, 통나무, 나무, 풀, 바위

**조명 구성:**
- 모닥불 → Point Light
- 낮/밤 배경 → Skybox + Directional Light
- 랜턴/손전등 → Spotlight (밤 Scene)

**상호작용:**
- 키 입력으로 낮/밤 전환
- Q/E 키로 카메라 상하 이동
- 스페이스바로 모닥불 켜고 끄기

## 3. 적용 그래픽스 기법

- **Lighting:** Ambient / Diffuse / Specular / Directional Light / Point Light / Spotlight
- **Advanced Lighting:** PBR 기반 고급 조명 효과
- **Model Loading:** Assimp를 이용한 외부 3D 모델 로딩
- **Depth Test:** 오브젝트 간 겹침 처리
- **Stencil Test:** 화면 정중앙 오브젝트 자동 선택 및 외곽선 표현
- **Alpha Blending:** 나뭇잎, 풀, 불꽃, 연기 효과
- **Instanced Rendering:** 반딧불이 다수 개체를 GPU 단일 Draw Call로 고성능 렌더링
- **Post-processing:** 밤 Scene Bloom 효과 + 안개(Fog) 효과 (Framebuffer 활용)
- **Skybox:** 낮/밤 배경 하늘 구성 (밤 하늘 별자리 포함)
- **Anti-aliasing:** MSAA 적용으로 계단 현상 제거
- **Gamma Correction:** 모니터 출력 색 보정

## 3-1. Anti-aliasing 기법 선택 근거

Anti-aliasing 기법은 SSAA, MSAA, FXAA, SMAA, TAA 등 다양한 방식이 존재한다.

| 기법 | 방식 | 탈락 이유 |
|---|---|---|
| SSAA | 고해상도 렌더링 후 다운샘플 | GPU 비용이 너무 큼 |
| FXAA | 엣지 감지 후 블러 필터 | 텍스처가 뭉개지는 부작용 |
| SMAA | 형태학적 엣지 재구성 | 별도 셰이더 구현 복잡도 높음 |
| TAA | 여러 프레임 시간 누적 | 고스팅 아티팩트, 구현 복잡 |
| **MSAA** ✓ | 서브픽셀 다중 샘플링 | OpenGL 하드웨어 네이티브 지원, 구현 단순, 품질/성능 균형 최적 |

MSAA는 `glfwWindowHint(GLFW_SAMPLES, 4)` + `glEnable(GL_MULTISAMPLE)` 만으로 적용 가능하며,
Post-processing FBO와의 연동 시에도 Multisampled FBO → 일반 FBO blit 순서로 명확히 처리 가능하여 선택함.

- 구현 사진
    - 전
      <img width="986" height="892" alt="image" src="https://github.com/user-attachments/assets/6bec9f2e-3661-48d2-9111-691461fa6fc5" />
    - 후
      <img width="1172" height="1014" alt="image" src="https://github.com/user-attachments/assets/5a8a4add-e487-48e8-8b76-fe91ed8d617f" />

## 4. 반딧불이 Instanced Rendering 구현 계획

### 4-1. 개요

반딧불이는 밤 Scene에서 수십~수백 개의 동일한 발광 Quad(사각형 빌보드)를 화면에 뿌려야 합니다.
각 개체를 개별 Draw Call로 처리하면 CPU→GPU 통신 오버헤드가 개체 수만큼 선형 증가합니다.
**Instanced Rendering**을 사용하면 하나의 `glDrawArraysInstanced()` 호출로 수백 개를 한 번에 처리할 수 있어 성능이 대폭 향상됩니다.

### 4-2. 구현 방법

**Instance Data 준비 (CPU)**

각 반딧불이의 위치(vec3), 크기(float), 깜빡임 위상(float)을 배열로 구성한 뒤, 별도의 Instance VBO에 업로드합니다.

```cpp
struct FireflyInstance {
    glm::vec3 position;
    float scale;
    float phase; // 깜빡임 시간 오프셋
};

std::vector<FireflyInstance> instances(NUM_FIREFLIES);
// 랜덤 위치/위상 초기화
for (auto& f : instances) {
    f.position = glm::vec3(randRange(-10,10), randRange(0.5,3), randRange(-10,10));
    f.scale    = 0.05f;
    f.phase    = randRange(0, 2*M_PI);
}

GLuint instanceVBO;
glBindVertexArray(fireflyVAO);
glGenBuffers(1, &instanceVBO);
glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(FireflyInstance),
             instances.data(), GL_DYNAMIC_DRAW);

// layout(location=2): position
glEnableVertexAttribArray(2);
glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(FireflyInstance),
                      (void*)offsetof(FireflyInstance, position));
glVertexAttribDivisor(2, 1); // 인스턴스당 1번 갱신

// layout(location=3): scale
glEnableVertexAttribArray(3);
glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(FireflyInstance),
                      (void*)offsetof(FireflyInstance, scale));
glVertexAttribDivisor(3, 1);

// layout(location=4): phase
glEnableVertexAttribArray(4);
glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(FireflyInstance),
                      (void*)offsetof(FireflyInstance, phase));
glVertexAttribDivisor(4, 1);
```

**Vertex Shader**

```glsl
#version 330 core
layout(location = 0) in vec2 aPos;       // Quad 정점 (빌보드)
layout(location = 2) in vec3 iPosition;  // 인스턴스 위치
layout(location = 3) in float iScale;    // 인스턴스 크기
layout(location = 4) in float iPhase;    // 깜빡임 위상

uniform mat4 view;
uniform mat4 projection;
uniform float time;

out float vBrightness;

void main() {
    // 빌보드: view 행렬의 right/up 벡터를 이용해 카메라를 항상 향하게
    vec3 camRight = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camUp    = vec3(view[0][1], view[1][1], view[2][1]);
    vec3 worldPos = iPosition
                  + camRight * aPos.x * iScale
                  + camUp    * aPos.y * iScale;

    gl_Position = projection * view * vec4(worldPos, 1.0);

    // sin 함수로 부드러운 깜빡임 (0.2 ~ 1.0 범위)
    vBrightness = 0.6 + 0.4 * sin(time * 2.0 + iPhase);
}
```

**Fragment Shader**

```glsl
#version 330 core
in float vBrightness;
out vec4 FragColor;

void main() {
    // 연한 노란빛 발광 색상
    vec3 color = vec3(1.0, 0.95, 0.4) * vBrightness;
    FragColor = vec4(color, vBrightness * 0.9); // Alpha Blending과 함께 사용
}
```

**Draw Call**

```cpp
// 밤 Scene 렌더링 시
fireflyShader.use();
fireflyShader.setFloat("time", (float)glfwGetTime());
// Alpha Blending 활성화
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

glBindVertexArray(fireflyVAO);
glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, NUM_FIREFLIES); // 단 1번의 Draw Call
```

### 4-3. 핵심 포인트 요약

| 항목 | 내용 |
| --- | --- |
| glVertexAttribDivisor(n, 1) | 해당 attribute를 인스턴스마다 1번씩 읽도록 설정 |
| glDrawArraysInstanced | 1번의 Draw Call로 N개 인스턴스 렌더링 |
| Billboard 처리 | View 행렬의 right/up 벡터 활용, 항상 카메라를 향함 |
| 깜빡임 애니메이션 | 버텍스 셰이더에서 sin(time + phase)로 처리 |
| Alpha Blending 연동 | Fragment의 알파값과 GL_BLEND로 반투명 발광 효과 |

## 5. 주차별 계획 및 역할 분담

| 주차 | 작업 내용 | 역할 분담 |
| --- | --- | --- |
| 10주차 | 계획서 작성 | 공통 |
| 11주차 | 모델 및 Asset 수집, 기본 Scene 구성 (지형, 오브젝트 배치), Assimp를 이용한 모델 로딩 구현 | 최현우: 모델 수집 및 Assimp 연동 / 한 민: 기본 Scene 레이아웃 구성 |
| 12주차 | 조명 효과 적용 (Ambient, Diffuse, Specular, Directional Light, Point Light, Spotlight), Skybox 구현 (낮/밤 전환), Alpha Blending 적용 (풀, 나뭇잎, 불꽃, 연기), **반딧불이 Instanced Rendering 구현**, Stencil Test로 오브젝트 외곽선 표현, Framebuffer Post-processing (Bloom 효과) | 최현우: 조명 시스템, Skybox, 낮/밤 전환, **반딧불이 Instanced Rendering** / 한 민: Alpha Blending, Stencil, Post-processing |
| 13주차 | 최종 통합 및 버그 수정, 발표 준비 및 발표 | 공통 |
