#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "mesh.h"
#include "shader_m.h"

#include <string>
#include <vector>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

// convert Assimp matrix to glm
static glm::mat4 aiMat4ToGlm(const aiMatrix4x4& m)
{
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
}

unsigned int TextureFromFile(const char* path, const std::string& directory);
unsigned int TextureFromEmbedded(const aiTexture* tex);

class Model {
public:
    std::vector<Texture> textures_loaded;
    std::vector<Mesh>    meshes;
    std::string          directory;

    Model(const std::string& path) { loadModel(path); }

    void Draw(Shader& shader) {
        for (auto& mesh : meshes)
            mesh.Draw(shader);
    }

private:
    const aiScene* scene_ptr = nullptr;

    void loadModel(const std::string& path)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path,
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_FlipUVs |
            aiProcess_CalcTangentSpace);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cerr << "Assimp error: " << importer.GetErrorString() << std::endl;
            return;
        }
        scene_ptr = scene;
        directory = path.substr(0, path.find_last_of("/\\"));
        processNode(scene->mRootNode, scene, glm::mat4(1.0f));
    }

    // glb 파일의 계층 구조를 재귀적으로 순회하면서 mesh를 수집하는 함수
    void processNode(aiNode* node, const aiScene* scene, glm::mat4 parentTransform)
    {
        // accumulate this node's transform
        glm::mat4 nodeTransform = parentTransform * aiMat4ToGlm(node->mTransformation);

        // 노드의 이름을 읽은 다음, nodeName에 저장한 다음, processMesh를 호출할 때 같이 넘긴다.
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            std::string nodeName = node->mName.C_Str();
            meshes.push_back(processMesh(scene->mMeshes[node->mMeshes[i]], scene, nodeTransform, nodeName));
            std::cout << "Node: " << node->mName.C_Str() << std::endl; // 프로젝트를 실행할 때 노드의 이름을 출력
    }   
        for (unsigned int i = 0; i < node->mNumChildren; i++)
            processNode(node->mChildren[i], scene, nodeTransform);
    }

    // 실제 mesh 데이터를 가공해서 mesh 객체를 만드는 함수. 이름을 받을 수 있게 수정
    Mesh processMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 nodeTransform, const std::string& nodeName)
    {
        std::vector<Vertex>       vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture>      textures;
        glm::vec4 baseColor(1.0f);
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(nodeTransform)));

        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex v;
            // bake node transform into vertex position
            glm::vec4 pos = nodeTransform * glm::vec4(
                mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f);
            v.Position  = glm::vec3(pos);
            v.Normal    = mesh->HasNormals()
                        ? glm::normalize(normalMatrix * glm::vec3(
                              mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z))
                        : glm::vec3(0.0f, 1.0f, 0.0f);
            v.TexCoords = mesh->mTextureCoords[0]
                        ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                        : glm::vec2(0.0f);
            v.Color     = mesh->mColors[0]
                        ? glm::vec4(mesh->mColors[0][i].r, mesh->mColors[0][i].g,
                                    mesh->mColors[0][i].b, mesh->mColors[0][i].a)
                        : glm::vec4(1.0f);
            vertices.push_back(v);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];

            // glTF base color factor (solid color per mesh)
            aiColor4D color;
            if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
                baseColor = glm::vec4(color.r, color.g, color.b, color.a);

            auto diff = loadMaterialTextures(mat, aiTextureType_DIFFUSE, "texture_diffuse");
            textures.insert(textures.end(), diff.begin(), diff.end());
            // also try BASE_COLOR slot (glTF PBR)
            auto base = loadMaterialTextures(mat, aiTextureType_BASE_COLOR, "texture_diffuse");
            if (diff.empty())
                textures.insert(textures.end(), base.begin(), base.end());

            auto spec = loadMaterialTextures(mat, aiTextureType_SPECULAR, "texture_specular");
            textures.insert(textures.end(), spec.begin(), spec.end());
        }

        return Mesh(vertices, indices, textures, baseColor, nodeName);
    }

    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName)
    {
        std::vector<Texture> textures;
        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
            aiString str;
            mat->GetTexture(type, i, &str);

            bool skip = false;
            for (auto& loaded : textures_loaded) {
                if (loaded.path == str.C_Str()) { textures.push_back(loaded); skip = true; break; }
            }
            if (!skip) {
                Texture tex;
                tex.type = typeName;
                tex.path = str.C_Str();

                // GLB embedded texture: path is '*0', '*1', etc.
                if (str.C_Str()[0] == '*') {
                    int index = std::atoi(str.C_Str() + 1);
                    if (scene_ptr && index < (int)scene_ptr->mNumTextures)
                        tex.id = TextureFromEmbedded(scene_ptr->mTextures[index]);
                    else
                        tex.id = 0;
                } else {
                    tex.id = TextureFromFile(str.C_Str(), directory);
                }

                textures.push_back(tex);
                textures_loaded.push_back(tex);
            }
        }
        return textures;
    }
};

unsigned int TextureFromFile(const char* path, const std::string& directory)
{
    std::string filename = directory + '/' + std::string(path);

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format = (nrComponents == 1) ? GL_RED :
                        (nrComponents == 3) ? GL_RGB : GL_RGBA;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        std::cerr << "Texture load failed: " << filename << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

unsigned int TextureFromEmbedded(const aiTexture* tex)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = nullptr;
    bool usedStbi = false;

    if (tex->mHeight == 0) {
        // Compressed format (PNG, JPEG) - decode from memory
        data = stbi_load_from_memory(
            reinterpret_cast<unsigned char*>(tex->pcData),
            tex->mWidth, &width, &height, &nrComponents, 0);
        usedStbi = true;
    } else {
        // Uncompressed raw ARGB8888
        width        = tex->mWidth;
        height       = tex->mHeight;
        nrComponents = 4;
        data = reinterpret_cast<unsigned char*>(tex->pcData);
    }

    if (data) {
        GLenum format = (nrComponents == 1) ? GL_RED :
                        (nrComponents == 3) ? GL_RGB : GL_RGBA;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (usedStbi)
            stbi_image_free(data);
    } else {
        std::cerr << "Embedded texture load failed" << std::endl;
    }

    return textureID;
}
