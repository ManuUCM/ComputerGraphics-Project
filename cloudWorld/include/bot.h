#ifndef bot_h
#define bot_h
#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

// GLTF model loader
#include <tiny_gltf.h>

#include <render/shader.h>

#include <vector>
#include <iostream>
#include <iomanip>
#define _USE_MATH_DEFINES
#include <math.h>
#include <cassert>

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

// Lighting
static glm::vec3 lightIntensity(5e6f, 5e6f, 5e6f);
static glm::vec3 lightPosition(-275.0f, 500.0f, 800.0f);

// Animation
static bool playAnimation = true;
static float playbackSpeed = 2.0f;

struct MyBot {
    // Shader variable IDs
    GLuint mvpMatrixID;
    GLuint jointMatricesID;
    GLuint lightPositionID;
    GLuint lightIntensityID;
    GLuint programID;
    GLuint modelMatrixID;

    tinygltf::Model model;

    // New attributes
    // Model dimensions used for positioning
    glm::vec3 modelCenter;
    float modelScale;
    glm::vec3 skeletonOffset;  // Manual offset to center skeleton

    // The bot should also be affected by the fog of the planets so I apply it as well
    bool fogEnabled;
    glm::vec3 fogColor;
    float fogDensity;
    glm::vec3 cameraPosition;

    // Directional and hemi-environment lighting like the planets
    static glm::vec3 lightDirection;
    static glm::vec3 lightColor;
    static glm::vec3 envColor;

    // Shadow mapping
    static glm::mat4 lightVP;
    static GLuint shadowDepthTexture;

    // Each VAO corresponds to each mesh primitive in the GLTF model
    struct PrimitiveObject {
        GLuint vao;
        std::map<int, GLuint> vbos;
    };
    std::vector<PrimitiveObject> primitiveObjects;

    // Skinning
    struct SkinObject {
        // Transforms the geometry into the space of the respective joint
        std::vector<glm::mat4> inverseBindMatrices;

        // Transforms the geometry following the movement of the joints
        std::vector<glm::mat4> globalJointTransforms;

        // Combined transforms
        std::vector<glm::mat4> jointMatrices;
    };
    std::vector<SkinObject> skinObjects;

    // Animation
    struct SamplerObject {
        std::vector<float> input;
        std::vector<glm::vec4> output;
        int interpolation;
    };
    struct ChannelObject {
        int sampler;
        std::string targetPath;
        int targetNode;
    };
    struct AnimationObject {
        std::vector<SamplerObject> samplers; // Animation data
    };
    std::vector<AnimationObject> animationObjects;

    glm::mat4 getNodeTransform(const tinygltf::Node& node);

    void computeLocalNodeTransform(
        const tinygltf::Model& model,
        int nodeIndex,
        std::vector<glm::mat4>& localTransforms
    );

    void computeGlobalNodeTransform(
        const tinygltf::Model& model,
        const std::vector<glm::mat4>& localTransforms,
        int nodeIndex,
        const glm::mat4& parentTransform,
        std::vector<glm::mat4>& globalTransforms
    );

    std::vector<SkinObject> prepareSkinning(const tinygltf::Model& model);

    int findKeyframeIndex(const std::vector<float>& times, float animationTime);

    std::vector<AnimationObject> prepareAnimation(const tinygltf::Model& model);

    void updateAnimation(
        const tinygltf::Model& model,
        const tinygltf::Animation& anim,
        const AnimationObject& animationObject,
        float time,
        std::vector<glm::mat4>& nodeTransforms
    );

    void updateSkinning(const std::vector<glm::mat4>& nodeTransforms);

    void update(float time);

    bool loadModel(tinygltf::Model& model, const char* filename);

    void initialize();

    void bindMesh(
        std::vector<PrimitiveObject>& primitiveObjects,
        tinygltf::Model& model,
        tinygltf::Mesh& mesh
    );

    void bindModelNodes(
        std::vector<PrimitiveObject>& primitiveObjects,
        tinygltf::Model& model,
        tinygltf::Node& node
    );

    std::vector<PrimitiveObject> bindModel(tinygltf::Model& model);

    void drawMesh(
        const std::vector<PrimitiveObject>& primitiveObjects,
        tinygltf::Model& model,
        tinygltf::Mesh& mesh
    );

    void drawModelNodes(
        const std::vector<PrimitiveObject>& primitiveObjects,
        tinygltf::Model& model,
        tinygltf::Node& node
    );

    void drawModel(
        const std::vector<PrimitiveObject>& primitiveObjects,
        tinygltf::Model& model
    );

    void render(glm::mat4 cameraMatrix, const glm::mat4& M, const glm::vec3& lightDir, const glm::vec3& lightCol,
            const glm::vec3& envCol);

    void cleanup();
};
#endif