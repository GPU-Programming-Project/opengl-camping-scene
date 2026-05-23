#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader_m.h"
#include "camera.h"
#include "model.h"

#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cmath>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadCubemap(std::vector<std::string> faces);
int pickCenterMesh(const std::vector<Mesh>& meshes, glm::vec3 rayOrigin, glm::vec3 rayDir);

const unsigned int SCR_WIDTH  = 1280;
const unsigned int SCR_HEIGHT = 720;

Camera camera(glm::vec3(0.0f, 10.0f, -25.0f), glm::vec3(0, 1, 0), 90.0f, -20.0f);
float lastX = SCR_WIDTH  / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse   = true;
bool gammaEnabled  = false;
bool spacePressed  = false;
bool flashlightOn  = false;
bool fKeyPressed   = false;
bool bloomEnabled  = false;
bool bKeyPressed   = false;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 4); // msaa 4배 적용 시키기

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Camping Scene", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE); // MSAA 활성화
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);  // stencil test option 설정
    glEnable(GL_PROGRAM_POINT_SIZE);             // 버텍스 셰이더에서 gl_PointSize 제어 허용

    Shader sceneShader   ("shader/scene.vs",       "shader/scene.fs");
    Shader skyboxShader  ("shader/skybox.vs",      "shader/skybox.fs");
    Shader brightShader  ("shader/postprocess.vs", "shader/bloom_bright.fs");
    Shader blurShader    ("shader/postprocess.vs", "shader/bloom_blur.fs");
    Shader combineShader ("shader/postprocess.vs", "shader/bloom_combine.fs");
    Shader fireflyShader ("shader/firefly.vs",     "shader/firefly.fs");

    // GLB 모델 로드
    Model campingModel("resources/models/low_poly_forest_campfire_updated.glb");

    const int FIREFLY_COUNT = 150;

    // 인스턴스별 데이터 구조체
    struct FireflyInstance {
        glm::vec3 basePos; // 기준 위치
        float     phase;   // 애니메이션 위상 (개체마다 다른 타이밍)
        float     speed;   // 이동 속도 배율
    };

    // 랜덤 데이터 생성 (캠프파이어 주변 반경 3~20, 높이 1~6)
    srand(42);
    auto rng = [](float lo, float hi) {
        return lo + (hi - lo) * (rand() / (float)RAND_MAX);
    };
    std::vector<FireflyInstance> fireflyData(FIREFLY_COUNT);
    for (auto& f : fireflyData) {
        float angle  = rng(0.0f, 6.28f);
        float radius = rng(3.0f, 20.0f);
        f.basePos = glm::vec3(std::cos(angle) * radius,
                              rng(1.0f, 6.0f),
                              std::sin(angle) * radius);
        f.phase = rng(0.0f, 6.28f);
        f.speed = rng(0.4f, 1.2f);
    }

    // VAO / VBO 생성
    unsigned int fireflyVAO, fireflyVBO;
    glGenVertexArrays(1, &fireflyVAO);
    glGenBuffers(1, &fireflyVBO);

    glBindVertexArray(fireflyVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fireflyVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 FIREFLY_COUNT * sizeof(FireflyInstance),
                 fireflyData.data(), GL_STATIC_DRAW);

    // location 0: basePos (vec3, offset=0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(FireflyInstance), (void*)0);
    glVertexAttribDivisor(0, 1); // 인스턴스마다 1칸씩 전진

    // location 1: phase (float, offset=12)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE,
                          sizeof(FireflyInstance),
                          (void*)offsetof(FireflyInstance, phase));
    glVertexAttribDivisor(1, 1);

    // location 2: speed (float, offset=16)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE,
                          sizeof(FireflyInstance),
                          (void*)offsetof(FireflyInstance, speed));
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);

    // 광원 위치 (캠프파이어 위쪽)
    glm::vec3 lightPos(0.0f, 3.0f, 0.0f);

    // Skybox 큐브 정점
    float skyboxVertices[] = {
        // positions
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    // Cubemap 텍스처 로드
    std::vector<std::string> faces = {
        "resources/skybox/right.png",
        "resources/skybox/left.png",
        "resources/skybox/top.png",
        "resources/skybox/bottom.png",
        "resources/skybox/front.png",
        "resources/skybox/back.png"
    };
    unsigned int cubemapTexture = loadCubemap(faces);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    // --- FBO 생성 (post-processing용) ---
    unsigned int fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    unsigned int texColorBuffer;
    glGenTextures(1, &texColorBuffer);
    glBindTexture(GL_TEXTURE_2D, texColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColorBuffer, 0);

    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- 화면 전체 quad VAO ---
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // FBO2: bloom용 밝기 추출 + blur 결과 저장
    unsigned int fboBloom;
    glGenFramebuffers(1, &fboBloom);
    glBindFramebuffer(GL_FRAMEBUFFER, fboBloom);
    unsigned int texBloomBuffer;
    glGenTextures(1, &texBloomBuffer);
    glBindTexture(GL_TEXTURE_2D, texBloomBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texBloomBuffer, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    combineShader.use();
    combineShader.setInt("sceneTexture", 0);
    combineShader.setInt("bloomTexture", 1);
    // -----------------
    // render loop
    // -----------------
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // stencil test
        // 카메라와 가장 가까운 오브젝트 지정
        int targetMeshIdx = pickCenterMesh(campingModel.meshes, camera.Position, camera.Front);

        // 1. 씬 렌더링 (bloom ON → FBO 경유, bloom OFF → 화면 직접 렌더링 + MSAA 적용)
        if (bloomEnabled)
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        else
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.05f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 200.0f
        );
        glm::mat4 view  = camera.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);

        sceneShader.use();
        sceneShader.setMat4("projection", projection);
        sceneShader.setMat4("view", view);
        sceneShader.setMat4("model", model);
        sceneShader.setVec3("lightPos",   lightPos);
        sceneShader.setVec3("lightColor", glm::vec3(1.0f, 0.85f, 0.5f));
        sceneShader.setVec3("viewPos",    camera.Position);
        sceneShader.setFloat("time",        (float)glfwGetTime());
        sceneShader.setBool("gammaEnabled", gammaEnabled);
        sceneShader.setBool("flashlightOn",          flashlightOn);
        sceneShader.setVec3("flashlightPos",         camera.Position);
        sceneShader.setVec3("flashlightDir",         camera.Front);
        sceneShader.setVec3("flashlightColor",       glm::vec3(1.0f, 1.0f, 0.9f));
        sceneShader.setFloat("flashlightCutOff",      glm::cos(glm::radians(12.5f)));
        sceneShader.setFloat("flashlightOuterCutOff", glm::cos(glm::radians(17.5f)));


        // 캠프파이어 메시의 origin을 lightPos로 사용
        for (auto& mesh : campingModel.meshes) {
            if (mesh.name == "Mball.018_0") {
                lightPos = mesh.origin;
                break;
            }
        }
        sceneShader.setVec3("lightPos", lightPos);

        // 패스 1: 불투명 메시
        sceneShader.setBool("isOutline", false);
        for (int i = 0; i < (int)campingModel.meshes.size(); i++) {
            auto& mesh = campingModel.meshes[i];
            if (mesh.name == "LanternGlass_0") continue;

            // stencil test
            // 윤곽선을 그릴 mesh는 stencil buffer에 기록
            if (i == targetMeshIdx) {
                glStencilFunc(GL_ALWAYS, 1, 0xFF);
                glStencilMask(0xFF);
            } else {  // 나머지 mesh는 기록하지 않음
                glStencilFunc(GL_ALWAYS, 0, 0xFF);
                glStencilMask(0x00);
            }

            sceneShader.setBool("isEmissive", mesh.name == "Mball.018_0");
            sceneShader.setFloat("meshAlpha", 1.0f);
            mesh.Draw(sceneShader);
        }
        glStencilMask(0xFF); 

        // 패스 2: 투명 메시 (LanternGlass_0)
        glDepthMask(GL_FALSE);
        for (auto& mesh : campingModel.meshes) {
            if (mesh.name != "LanternGlass_0") continue;
            sceneShader.setBool("isEmissive", false);
            sceneShader.setFloat("meshAlpha", 0.3f);
            mesh.Draw(sceneShader);
        }
        glDepthMask(GL_TRUE);

        // Skybox 렌더링 (씬 오브젝트 다 그린 후 맨 마지막)
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        glm::mat4 skyboxView = glm::mat4(glm::mat3(camera.GetViewMatrix())); // 이동 성분 제거
        skyboxShader.setMat4("view", skyboxView);
        skyboxShader.setMat4("projection", projection);
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        // stencil test
        // 아웃라인 패스
        if (targetMeshIdx != -1) {
            glStencilFunc(GL_NOTEQUAL, 1, 0xFF);  // buffer에 1로 기록된 부분은 그리지 않는다. 
            glStencilMask(0x00);  // 이 단계에서는 stencil buffer를 수정하지 않는다.
            glDisable(GL_DEPTH_TEST);  // 다른 사물을 통과하여 윤곽선이 보일 수 있도록 depth test 끔

            sceneShader.use();
            sceneShader.setMat4("projection", projection);
            sceneShader.setMat4("view",       view);
            sceneShader.setMat4("model",      model);
            sceneShader.setBool ("isOutline",   true);  // vertex 팽창
            sceneShader.setFloat("outlineSize", 0.03f);  // 팽창 정도 조절
            campingModel.meshes[targetMeshIdx].Draw(sceneShader);  // mesh 그리기
            sceneShader.setBool("isOutline", false);

            glStencilMask(0xFF);
            glStencilFunc(GL_ALWAYS, 0, 0xFF);  // stencil 조건 원상 복구
            glEnable(GL_DEPTH_TEST);  // 깊이 검사 진행
        }

        // 반딧불이 드로우 (instancing)
        fireflyShader.use();
        fireflyShader.setMat4 ("projection", projection);
        fireflyShader.setMat4 ("view",       view);
        fireflyShader.setFloat("time",       (float)glfwGetTime());
        glBindVertexArray(fireflyVAO);
        glDrawArraysInstanced(GL_POINTS, 0, 1, FIREFLY_COUNT);
        // GL_POINTS       : 점 하나짜리 기본 도형
        // 세 번째 인자 1  : 버텍스 1개 (위치는 셰이더 내 인스턴스 데이터로 결정)
        // FIREFLY_COUNT   : 인스턴스 수 → 이 횟수만큼 버텍스 셰이더 실행
        glBindVertexArray(0);

        // 2. bloom ON일 때만 Pass 2, 3 실행 (bloom OFF면 이미 화면에 직접 그려짐)
        if (!bloomEnabled) { glfwSwapBuffers(window); glfwPollEvents(); continue; }

        // 밝은 픽셀 뽑아서 / Gaussian blur에 적용 / FBO2에 저장하기
        glBindFramebuffer(GL_FRAMEBUFFER, fboBloom);
        glDisable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT);
        // 2-1. 밝기 임계값 뽑기
        brightShader.use();
        glBindVertexArray(quadVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texColorBuffer);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // 2-2. 추출된 밝은 부분에 Gaussian blur 적용
        blurShader.use();
        glBindTexture(GL_TEXTURE_2D, texBloomBuffer);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 3. 원본 씬 + bloom 합성하여 화면 출력하기
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        combineShader.use();
        combineShader.setBool("bloomEnabled", bloomEnabled);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texColorBuffer);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texBloomBuffer);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
	// 위아래로 이동하는 기능 추가 하기 (feat/camera-movement-up-down)
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        camera.ProcessKeyboard(UP_MOVE, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN_MOVE, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if (!spacePressed) {
            gammaEnabled = !gammaEnabled;
            spacePressed = true;
        }
    } else {
        spacePressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (!fKeyPressed) {
            flashlightOn = !flashlightOn;
            fKeyPressed  = true;
        }
    } else {
        fKeyPressed = false;
    }

    // 키보드 B 인식에 따른 bloom 효과 토글!
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
        if (!bKeyPressed) {
            bloomEnabled = !bloomEnabled;
            bKeyPressed  = true;
        }
    } else {
        bKeyPressed = false;
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) {
        lastX = xpos; lastY = ypos;
        firstMouse = false;
    }
    camera.ProcessMouseMovement(xpos - lastX, lastY - ypos);
    lastX = xpos;
    lastY = ypos;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// 선택된 mesh의 번호를 return하는 함수
// stencil test에 이용
// rayOrigin = ray의 시작점, rayDir = ray의 방향
int pickCenterMesh(const std::vector<Mesh>& meshes, glm::vec3 rayOrigin, glm::vec3 rayDir)
{
    int   bestIdx  = -1;
    float bestDist = 2.5f;  // 해당 거리 안에 들어와야 유효한 오브젝트로 인식

    for (int i = 0; i < (int)meshes.size(); i++) {  
        if (meshes[i].name == "LanternGlass_0") continue;  // 투명한 물체는 제외

        glm::vec3 toOrigin = meshes[i].origin - rayOrigin; // mesh에서 ray로 향하는 방향벡터

        float proj = glm::dot(toOrigin, rayDir);  // mesh -> ray로 향하는 벡터와 ray의 방향을 내적
        if (proj < 0.0f) continue;  // 0이 내적의 결과가 0보다 작으면 제외. 양수만 허용하는 이유는 카메라 뒤의 물체는 제외하기 위함

        glm::vec3 closest = rayOrigin + rayDir * proj;  // ray위의 가장 가까운 점 계산. proj가 0에 가까울수록 
        float dist = glm::length(closest - meshes[i].origin);  // 해당 점의 거리 계산

        if (dist < bestDist) {  // 현재 최단 거리보다 짧으면 갱신
            bestDist = dist;
            bestIdx  = i;
        }
    }
    return bestIdx;
}

unsigned int loadCubemap(std::vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 3);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cerr << "Cubemap texture failed to load: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}
