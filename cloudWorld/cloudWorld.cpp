#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <render/shader.h>
#include <stb/stb_image.h>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include "include/bot.h"

static GLFWwindow* window = nullptr;

// Camera settings and MVP matrices
glm::mat4 viewMatrix;
glm::mat4 projectionMatrix;
glm::mat4 modelMatrix;
glm::float32 FoV   = 100.0f;
glm::float32 zNear = 0.1f;
glm::float32 zFar  = 3500.0f;
static glm::vec3 eye_center(0.0f, 0.0f, 10.0f);
static glm::vec3 lookat(0.0f, 0.0f, 0.0f);
static glm::vec3 up(0.0f, 1.0f, 0.0f);

// For camera rotation where yax control horizontal and pitch controls vertical rotation
static float yaw = 0.0f;    // left/right
static float pitch = 0.0f;  // up/down
static float speed = 10.0f;
// Default movement is slow but adequate for an analyzing exploration
// however I added a speedbost pressing left shift to increase exploration movement for all directions
static float speedBoost = 2.5f;

//Lighting
static glm::vec3 lightDirection = glm::normalize(glm::vec3(1.0f, -0.5f, -0.3f));  // Direction from light
static glm::vec3 lightColor = glm::vec3(1.0f, 0.95f, 0.9f);  // white sunlight
static glm::vec3 envColor = glm::vec3(0.4f, 0.55f, 0.65f);   // blue environment light

// Shadow mapping
static int shadowMapWidth = 4096;   // Shadow map resolution
static int shadowMapHeight = 4096;
static GLuint shadowFBO = 0;        // Shadow framebuffer object
static GLuint shadowDepthTexture = 0;  // Depth texture for shadows

// Shadow map projection parameters
static float depthFoV = 60.0f;      // Field of view for shadow camera
static float depthNear = 1.0f;      // Near plane
static float depthFar = 3500.0f;    // Far plane

// Skybox
GLuint skyboxVAO;
GLuint skyboxVertexBuffer;
GLuint skyboxIndexBuffer;
GLuint skyboxUVBuffer;
GLuint skyboxProgramID;
GLuint skyboxMatrixID;
GLuint skyboxTextureID;

static const GLfloat skyboxVertices[] = {
	-1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1,
	 1,-1,-1, -1,-1,-1, -1, 1,-1,  1, 1,-1,
	-1,-1,-1, -1,-1, 1, -1, 1, 1, -1, 1,-1,
	 1,-1, 1,  1,-1,-1,  1, 1,-1,  1, 1, 1,
	-1, 1, 1,  1, 1, 1,  1, 1,-1, -1, 1,-1,
	-1,-1,-1,  1,-1,-1,  1,-1, 1, -1,-1, 1
};

GLfloat skyboxUVs[48];

void adjustSkyboxUV() {
	const float W = 1.0f / 4.0f;
	const float H = 1.0f / 3.0f;
	const float eps = 0.001f;

	auto rect = [&](int col, int row, int idx) {
		float u0 = col * W + eps;
		float v0 = row * H + eps;
		float u1 = (col + 1) * W - eps;
		float v1 = (row + 1) * H - eps;

		skyboxUVs[idx+0] = u1; skyboxUVs[idx+1] = v1;
		skyboxUVs[idx+2] = u0; skyboxUVs[idx+3] = v1;
		skyboxUVs[idx+4] = u0; skyboxUVs[idx+5] = v0;
		skyboxUVs[idx+6] = u1; skyboxUVs[idx+7] = v0;
	};

	rect(1,1,  0);   // front
	rect(3,1,  8);   // back
	rect(2,1, 16);   // left
	rect(0,1, 24);   // right
	rect(1,0, 32);   // top
	rect(1,2, 40);   // bottom
}

static const GLuint skyboxIndices[] = {
	0, 1, 2,  0, 2, 3,
	4, 5, 6,  4, 6, 7,
	8, 9,10,  8,10,11,
   12,13,14, 12,14,15,
   16,17,18, 16,18,19,
   20,21,22, 20,22,23
};

GLuint LoadTexture(const char* image_path) {
	GLuint textureID;
	glGenTextures(1, &textureID);

	int width, height, nrChannels;
	unsigned char* data = stbi_load(image_path, &width, &height, &nrChannels, 0);
	if (!data) {
		printf("Failed to load texture: %s\n", image_path);
		return 0;
	}

	GLenum format = GL_RGB;
	if (nrChannels == 1) format = GL_RED;
	else if (nrChannels == 3) format = GL_RGB;
	else if (nrChannels == 4) format = GL_RGBA;

	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		format,
		width,
		height,
		0,
		format,
		GL_UNSIGNED_BYTE,
		data
	);
	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_image_free(data);
	return textureID;
}

void initSkybox() {
	adjustSkyboxUV();

	glGenVertexArrays(1, &skyboxVAO);
	glBindVertexArray(skyboxVAO);

	glGenBuffers(1, &skyboxVertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

	glGenBuffers(1, &skyboxUVBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxUVBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxUVs), skyboxUVs, GL_STATIC_DRAW);

	glGenBuffers(1, &skyboxIndexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyboxIndexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skyboxIndices), skyboxIndices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVertexBuffer);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxUVBuffer);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glBindVertexArray(0);

	skyboxProgramID = LoadShadersFromFile(
		"../cloudWorld/render/skybox.vert",
		"../cloudWorld/render/skybox.frag"
	);

	skyboxMatrixID = glGetUniformLocation(skyboxProgramID, "MVP");
	skyboxTextureID = LoadTexture("../cloudWorld/assets/skybox/NebulaAtlas.png");
}

void drawSkybox(const glm::mat4& Perspective, const glm::mat4& View) {
	glDepthMask(GL_FALSE);
	glUseProgram(skyboxProgramID);

	glm::mat4 ViewNoTranslation = glm::mat4(glm::mat3(View));
	glm::mat4 skyboxMVP = Perspective * ViewNoTranslation * glm::mat4(1.0f);

	glUniformMatrix4fv(skyboxMatrixID, 1, GL_FALSE, &skyboxMVP[0][0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, skyboxTextureID);

	glBindVertexArray(skyboxVAO);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

	glUseProgram(0);
	glDepthMask(GL_TRUE);
}

//For infinity camera movement
static const float WORLD_SIZE = 200.0f;

static glm::vec3 forwardDir() {
	return glm::normalize(glm::vec3(
		cos(pitch) * sin(yaw),
		sin(pitch),
	   -cos(pitch) * cos(yaw)
	));
}

static glm::vec3 rightDir() {
	return glm::normalize(glm::cross(forwardDir(), up));
}

static float wrapFloat(float value, float period) {
	float half = period * 0.5f;
	value = fmod(value + half, period);
	if (value < 0.0f) value += period;
	return value - half;
}

static void wrapPosition(glm::vec3& p, float size) {
	p.x = wrapFloat(p.x, size);
	p.y = wrapFloat(p.y, size);
	p.z = wrapFloat(p.z, size);
}

static void initShadowFBO() {
	// Generate framebuffer
	glGenFramebuffers(1, &shadowFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);

	// Create depth texture
	glGenTextures(1, &shadowDepthTexture);
	glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);

	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_DEPTH_COMPONENT,
		shadowMapWidth,
		shadowMapHeight,
		0,
		GL_DEPTH_COMPONENT,
		GL_FLOAT,
		NULL
	);

	// Texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	// Attach depth texture to FBO
	glFramebufferTexture2D(
		GL_FRAMEBUFFER,
		GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_2D,
		shadowDepthTexture,
		0
	);

	// No color buffer, only depth
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	// Check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "Shadow framebuffer is not complete!" << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;
};

GLuint sphereVAO;
GLuint sphereVBO;
GLuint sphereEBO;
GLsizei sphereIndexCount = 0;

GLuint planetProgramID;
GLuint planetMatrixID;
GLuint planetModelID;
//Planet creation and positions
static const int NUM_PLANETS = 20;
static const float PLANET_FIELD_RADIUS = 220.0f;
static const float MIN_PLANET_DISTANCE = 80.0f;
//Textures
static const int NUM_PLANET_TEXTURES = 20;
GLuint planetTextures[NUM_PLANET_TEXTURES];
GLuint planetTextureSampler;

struct Planet {
	glm::vec3 position;
	float radius;
	int textureIndex;
	glm::mat4 modelMatrix;
	glm::vec3 rotationAxis;
	float rotationSpeed;
	float rotationAngle;
};

std::vector<Planet> planets;

// Fog settings
static bool fogEnabled = true;
static glm::vec3 fogColor(0.02f, 0.02f, 0.08f);  // Dark blue, matching space theme
static float fogDensity = 0.005f;  // Adjust for desired fog thickness

glm::vec3 wrapPlanetPosition(const glm::vec3& planetPos) {
	glm::vec3 p = planetPos;
	p.x = wrapFloat(p.x - eye_center.x, WORLD_SIZE) + eye_center.x;
	p.y = wrapFloat(p.y - eye_center.y, WORLD_SIZE) + eye_center.y;
	p.z = wrapFloat(p.z - eye_center.z, WORLD_SIZE) + eye_center.z;
	return p;
}

glm::vec3 randomInSphere(float radius) {
	glm::vec3 p;
	do {
		p = glm::vec3(
			(float(rand()) / RAND_MAX) * 2.0f - 1.0f,
			(float(rand()) / RAND_MAX) * 2.0f - 1.0f,
			(float(rand()) / RAND_MAX) * 2.0f - 1.0f
		);
	} while (glm::length(p) > 1.0f);

	return p * radius;
}

void createSphere(int stacks, int slices) {
	// Sphere formula (inspired in quiz and lighting lecture)
	// x= sin(ϕ) * cos(θ)
	// y= cos(ϕ)
	// z= sin(ϕ) * sin(θ)
	// where ϕ ∈ [0,π] (stacks) and θ ∈ [0,2π] (slices).
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (int i = 0; i <= stacks; ++i) {
        float v = float(i) / float(stacks);
        float phi = v * glm::pi<float>();

        for (int j = 0; j <= slices; ++j) {
            float u = float(j) / float(slices);
            float theta = u * glm::two_pi<float>();

            glm::vec3 p(
                sin(phi) * cos(theta),
                cos(phi),
                sin(phi) * sin(theta)
            );

            vertices.push_back({
                p,
                glm::normalize(p),
                glm::vec2(u, 1.0f - v)
            });
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            uint32_t a = i * (slices + 1) + j;
            uint32_t b = (i + 1) * (slices + 1) + j;
            uint32_t c = b + 1;
            uint32_t d = a + 1;

            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(d);

            indices.push_back(d);
            indices.push_back(b);
            indices.push_back(c);
        }
    }

    sphereIndexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &sphereVAO);
    glBindVertexArray(sphereVAO);

    glGenBuffers(1, &sphereVBO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(Vertex),
                 vertices.data(),
                 GL_STATIC_DRAW);

    glGenBuffers(1, &sphereEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indices.size() * sizeof(uint32_t),
                 indices.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, normal));

    glEnableVertexAttribArray(2); // uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
}

MyBot bot;
int humanoidPlanetIndex = -1;
float humanoidAngle = 0.0f;
float humanoidAngularSpeed = 0.5f;
float humanoidRunRadiusFactor = 1.10f;
static float botAnimTime = 0.0f;

void init() {
	glEnable(GL_DEPTH_TEST);

	// Projection matrix
	projectionMatrix = glm::perspective(
		glm::radians(FoV),
		4.0f / 3.0f,
		zNear,
		zFar
	);

	// Skybox
	initSkybox();

	// Initialize shadow mapping
	initShadowFBO();

	createSphere(64, 64);
	planets.clear();
	srand(42);

	for (int i = 0; i < NUM_PLANETS; ++i) {
		Planet p;
		bool valid = false;

		while (!valid) { // This time randomly placed planets need to respect minimal distances between other already created planets
			p.position = randomInSphere(PLANET_FIELD_RADIUS);
			glm::vec3 pWrapped = wrapPlanetPosition(p.position);
			float t = float(rand()) / RAND_MAX;   // [0,1]

			// like the universe, I decided to make small planets common, big ones rare
			float scaleType = float(rand()) / RAND_MAX;
			if (scaleType < 0.7f) {
				// Small planets/asteroids have very high change (70% chance)
				p.radius = 1.0f + t * 5.0f;  // 1-6 units
			} else if (scaleType < 0.95f) {
				// Medium planets (25% chance)
				p.radius = 8.0f + t * 8.0f;  // 8-16 units
			} else {
				// Gas giants (5% chance)
				p.radius = 20.0f + t * 15.0f;  // 20-35 units
			}
			p.textureIndex = rand() % NUM_PLANET_TEXTURES;
			// random rotation axis
			p.rotationAxis = glm::normalize(randomInSphere(1.0f));
			// random angular speed (slow, space-like)
			p.rotationSpeed = 0.1f + (float(rand()) / RAND_MAX) * 0.3f;
			// initial angle
			p.rotationAngle = (float(rand()) / RAND_MAX) * glm::two_pi<float>();
			valid = true;
			for (const Planet& other : planets) {
				glm::vec3 otherWrapped = wrapPlanetPosition(other.position);
				float minDist = p.radius + other.radius + MIN_PLANET_DISTANCE;
				if (glm::distance(pWrapped, otherWrapped) < minDist) {
					valid = false;
					break;
				}
			}
		}
		planets.push_back(p);
	}

	planetProgramID = LoadShadersFromFile(
	"../cloudWorld/render/box.vert",
	"../cloudWorld/render/box.frag"
	);
	// Total of 20 textures, could be too much but given the randomness sometimes it can give cool combinations
	planetTextures[0] = LoadTexture("../cloudWorld/assets/textures/aerialRock.jpg");
	planetTextures[1] = LoadTexture("../cloudWorld/assets/textures/wafflePiqueCotton.jpg");
	planetTextures[2] = LoadTexture("../cloudWorld/assets/textures/jerseyMelange.jpg");
	planetTextures[3] = LoadTexture("../cloudWorld/assets/textures/lichenRock.jpg");
	planetTextures[4] = LoadTexture("../cloudWorld/assets/textures/rockBoulderDry.jpg");
	planetTextures[5] = LoadTexture("../cloudWorld/assets/textures/quatrefoilJacquardFabric.jpg");
	planetTextures[6] = LoadTexture("../cloudWorld/assets/textures/snowField.jpg");
	planetTextures[7] = LoadTexture("../cloudWorld/assets/textures/mossyBrick.jpg");
	planetTextures[8] = LoadTexture("../cloudWorld/assets/textures/redSlateRoofTiles.jpg");
	planetTextures[9] = LoadTexture("../cloudWorld/assets/textures/aerialRocks04.jpg");
	planetTextures[10] = LoadTexture("../cloudWorld/assets/textures/brokenBrickWall.jpg");
	planetTextures[11] = LoadTexture("../cloudWorld/assets/textures/ginghamCheck.jpg");
	planetTextures[12] = LoadTexture("../cloudWorld/assets/textures/rockBump.jpg");
	planetTextures[13] = LoadTexture("../cloudWorld/assets/textures/rockPitted.jpg");
	planetTextures[14] = LoadTexture("../cloudWorld/assets/textures/cliffSide.jpg");
	planetTextures[15] = LoadTexture("../cloudWorld/assets/textures/terryCloth.jpg");
	planetTextures[16] = LoadTexture("../cloudWorld/assets/textures/velourVelvet.jpg");
	planetTextures[17] = LoadTexture("../cloudWorld/assets/textures/rock06.jpg");
	planetTextures[18] = LoadTexture("../cloudWorld/assets/textures/rockBoulderCracked.jpg");
	planetTextures[19] = LoadTexture("../cloudWorld/assets/textures/rocksGround05.jpg");

	planetTextureSampler = glGetUniformLocation(planetProgramID, "diffuseTexture");
	planetMatrixID = glGetUniformLocation(planetProgramID, "MVP");
	planetModelID  = glGetUniformLocation(planetProgramID, "M");

	// humanoid init
	bot.initialize();
	if (humanoidPlanetIndex < 0 && !planets.empty()) {
		humanoidPlanetIndex = rand() % planets.size();
	}
	humanoidAngle = 0.0f;
}

void render() {
	// light's view-projection matrix calculation for shadow map
	glm::vec3 lightPos = eye_center + glm::normalize(-lightDirection) * 200.0f;
	glm::mat4 lightView = glm::lookAt(
		lightPos,
		lightPos + lightDirection,
		glm::vec3(0.0f, 1.0f, 0.0f)
	);

	glm::mat4 lightProjection = glm::perspective(
		glm::radians(90.0f),  // FOV
		1.0f,                  // Aspect ratio (square shadow map)
		1.0f,                  // Near
		1500.0f                // Far
	);

	glm::mat4 lightVP = lightProjection * lightView;

	// Update view matrix
	viewMatrix = glm::lookAt(
		eye_center,
		lookat,
		up
	);

	// Shadow pass
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
	glViewport(0, 0, shadowMapWidth, shadowMapHeight);

	// write to depth buffer, not color
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glClear(GL_DEPTH_BUFFER_BIT);

	// render planets for shadow map
	glUseProgram(planetProgramID);
	for (Planet& p : planets) {
		glm::vec3 wrappedPos = wrapPlanetPosition(p.position);

		p.modelMatrix =
			glm::translate(glm::mat4(1.0f), wrappedPos) *
			glm::rotate(glm::mat4(1.0f), p.rotationAngle, p.rotationAxis) *
			glm::scale(glm::mat4(1.0f), glm::vec3(p.radius));

		// Use lightVP instead of camera MVP
		glm::mat4 lightMVP = lightVP * p.modelMatrix;
		glUniformMatrix4fv(planetMatrixID, 1, GL_FALSE, glm::value_ptr(lightMVP));
		glUniformMatrix4fv(planetModelID, 1, GL_FALSE, glm::value_ptr(p.modelMatrix));

		// Pass LightVP to shader
		GLuint lightVPID = glGetUniformLocation(planetProgramID, "LightVP");
		glUniformMatrix4fv(lightVPID, 1, GL_FALSE, glm::value_ptr(lightVP));

		glBindVertexArray(sphereVAO);
		glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
	}
	glBindVertexArray(0);
	glUseProgram(0);

	// render bot shadow pass
	// comments on each line for humanoid rendering are done in the light rendering
	if (humanoidPlanetIndex >= 0 && humanoidPlanetIndex < planets.size()) {
		const Planet& hp = planets[humanoidPlanetIndex];
		glm::vec3 wrappedPlanetPos = wrapPlanetPosition(hp.position);

		float theta = humanoidAngle;
		float phi = glm::radians(25.0f);
		glm::vec3 localSurfacePos(
			sin(phi) * cos(theta),
			cos(phi),
			sin(phi) * sin(theta)
		);
		glm::vec3 surfaceOffset = localSurfacePos * (hp.radius * 0.8f);
		glm::vec3 humanoidWorldPos = wrappedPlanetPos + surfaceOffset;

		glm::vec3 up_vector = glm::normalize(localSurfacePos);
		glm::vec3 tangent = glm::normalize(glm::cross(glm::vec3(0, 1, 0), up_vector));
		if (glm::length(tangent) < 0.001f) {
			tangent = glm::vec3(1, 0, 0);
		}
		glm::vec3 forward = glm::normalize(glm::cross(up_vector, tangent));

		glm::mat4 rotationMatrix = glm::mat4(1.0f);
		rotationMatrix[0] = glm::vec4(tangent, 0.0f);
		rotationMatrix[1] = glm::vec4(up_vector, 0.0f);
		rotationMatrix[2] = glm::vec4(forward, 0.0f);

		float humanoidScale = hp.radius * 2.0f;
		glm::vec3 botPositionCorrection = glm::vec3(1, 0, 1);
		glm::vec3 correctedHumanoidPos = humanoidWorldPos + botPositionCorrection;

		glm::mat4 humanoidModelMatrix =
			glm::translate(glm::mat4(1.0f), correctedHumanoidPos) *
			rotationMatrix *
			glm::scale(glm::mat4(1.0f), glm::vec3(humanoidScale));

		bot.render(lightVP, humanoidModelMatrix, lightDirection, lightColor, envColor);
	}
	// end of shadow pass

	// Re-enable color writes
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	// Restore default framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	int windowWidth, windowHeight;
	glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
	glViewport(0, 0, windowWidth, windowHeight);
	// end shadow pass

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Skybox
	drawSkybox(projectionMatrix, viewMatrix);

	// Procedural planets
	glUseProgram(planetProgramID);
	// Set directional light
	glUniform3fv(glGetUniformLocation(planetProgramID, "lightDir"), 1, glm::value_ptr(lightDirection));
	glUniform3fv(glGetUniformLocation(planetProgramID, "lightColor"), 1, glm::value_ptr(lightColor));
	glUniform3fv(glGetUniformLocation(planetProgramID, "envColor"), 1, glm::value_ptr(envColor));

	// fog inclusion
	GLuint fogEnabledID = glGetUniformLocation(planetProgramID, "fogEnabled");
	GLuint fogColorID = glGetUniformLocation(planetProgramID, "fogColor");
	GLuint fogDensityID = glGetUniformLocation(planetProgramID, "fogDensity");
	GLuint cameraPosID = glGetUniformLocation(planetProgramID, "cameraPosition");

	glUniform1i(fogEnabledID, fogEnabled ? 1 : 0);
	glUniform3fv(fogColorID, 1, glm::value_ptr(fogColor));
	glUniform1f(fogDensityID, fogDensity);
	glUniform3fv(cameraPosID, 1, glm::value_ptr(eye_center));

	// planets rendering
	for (Planet& p : planets) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,
					  planetTextures[p.textureIndex]);
		glUniform1i(planetTextureSampler, 0);

		glm::vec3 wrappedPos = wrapPlanetPosition(p.position);

		p.modelMatrix =
			glm::translate(glm::mat4(1.0f), wrappedPos) *
				glm::rotate(glm::mat4(1.0f), p.rotationAngle, p.rotationAxis) *
				glm::scale(glm::mat4(1.0f), glm::vec3(p.radius));

		modelMatrix = p.modelMatrix;

		glm::mat4 MVP = projectionMatrix * viewMatrix * modelMatrix;

		glUniformMatrix4fv(planetMatrixID, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(planetModelID, 1, GL_FALSE, glm::value_ptr(p.modelMatrix));

		GLuint lightVPID = glGetUniformLocation(planetProgramID, "LightVP");
		glUniformMatrix4fv(lightVPID, 1, GL_FALSE, glm::value_ptr(lightVP));

		// Bind shadow map texture
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
		glUniform1i(glGetUniformLocation(planetProgramID, "shadowMap"), 1);

		glBindVertexArray(sphereVAO);
		glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
	}
	glBindVertexArray(0);
	glUseProgram(0);

	// Humanoid rendering
	if (humanoidPlanetIndex >= 0 && humanoidPlanetIndex < planets.size()) {
		const Planet& hp = planets[humanoidPlanetIndex];

		// Get wrapped planet position (same wrapping used for rendering planets)
		glm::vec3 wrappedPlanetPos = wrapPlanetPosition(hp.position);

		// calculate position on planet surface using spherical coordinates
		float theta = humanoidAngle;
		float phi = glm::radians(25.0f);  // Latitude angle on planet

		// position on unit sphere surface (shown in lectures and quiz)
		glm::vec3 localSurfacePos(
			sin(phi) * cos(theta),
			cos(phi),
			sin(phi) * sin(theta)
		);

		// Scale to planet surface and apply run radius factor
		glm::vec3 surfaceOffset = localSurfacePos * (hp.radius * 0.8f);
		// gives a little distance away off the planet so that it does not intersect with the surface and look odd

		// Final world position
		glm::vec3 humanoidWorldPos = wrappedPlanetPos + surfaceOffset;

		// orientation to stand upright on planet surface
		glm::vec3 up_vector = glm::normalize(localSurfacePos);
		glm::vec3 tangent = glm::normalize(glm::cross(glm::vec3(0, 1, 0), up_vector));

		// handle edge case when up_vector aligns with world Y axis
		if (glm::length(tangent) < 0.001f) {
			tangent = glm::vec3(1, 0, 0);
		}
		glm::vec3 forward = glm::normalize(glm::cross(up_vector, tangent));

		// given the orientation vectors, make the rotationMatrix for the bot to follow
		glm::mat4 rotationMatrix = glm::mat4(1.0f);
		rotationMatrix[0] = glm::vec4(tangent, 0.0f);
		rotationMatrix[1] = glm::vec4(up_vector, 0.0f);
		rotationMatrix[2] = glm::vec4(forward, 0.0f);

		// Scale humanoid proportionally to planet size
		float humanoidScale = hp.radius * 2.0f; // looks a little unrealistic but it's funny to see for the fantasy of the world

		// calculate a correction offset (trial and error procedure, could not reason any other approach after much debugging)
		glm::vec3 botPositionCorrection = glm::vec3(1, 0, 1);  // Start with zero

		// correction to the humanoid's world position to bring it towards the chosen planet
		glm::vec3 correctedHumanoidPos = humanoidWorldPos + botPositionCorrection;

		glm::mat4 humanoidModelMatrix =
			glm::translate(glm::mat4(1.0f), correctedHumanoidPos) *
			rotationMatrix *
			glm::scale(glm::mat4(1.0f), glm::vec3(humanoidScale));

		bot.cameraPosition = eye_center;  // Update camera position each frame
		bot.lightDirection = lightDirection;
		MyBot::lightColor = lightColor;
		MyBot::envColor = envColor;
		MyBot::lightVP = lightVP;
		MyBot::shadowDepthTexture = shadowDepthTexture;
		bot.render(projectionMatrix * viewMatrix, humanoidModelMatrix, lightDirection, lightColor, envColor);

		// Debugging sphere to help place the humanoid right at the planet
		// sphere being mapped with the box shaders was perfectly placed near the planet
		// so I used this markerSphere to approximate the distance to the bot that had an additional
		// given the joints offset

		// glm::mat4 markerModel =
		// 	glm::translate(glm::mat4(1.0f), humanoidWorldPos) *
		// 	glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));   // relatively small
		// glm::mat4 markerMVP = projectionMatrix * viewMatrix * markerModel;
		// glUseProgram(planetProgramID);
		// glUniformMatrix4fv(planetMatrixID, 1, GL_FALSE, &markerMVP[0][0]);
		// glUniformMatrix4fv(planetModelID,  1, GL_FALSE, &markerModel[0][0]);
		// glBindVertexArray(sphereVAO);
		// glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
		// glBindVertexArray(0);
		// glUseProgram(0);
	}
}

void cleanup() {
	//Cleanup for all models

	// Skybox
	glDeleteVertexArrays(1, &skyboxVAO);
	glDeleteBuffers(1, &skyboxVertexBuffer);
	glDeleteBuffers(1, &skyboxUVBuffer);
	glDeleteBuffers(1, &skyboxIndexBuffer);
	glDeleteProgram(skyboxProgramID);
	glDeleteTextures(1, &skyboxTextureID);

	//planets
	glDeleteVertexArrays(1, &sphereVAO);
	glDeleteBuffers(1, &sphereVBO);
	glDeleteBuffers(1, &sphereEBO);
	glDeleteProgram(planetProgramID);
	glDeleteTextures(NUM_PLANET_TEXTURES, planetTextures);

	//humanoid
	bot.cleanup();
}

static void key_callback(GLFWwindow* w, int key, int, int action, int) {
	if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
		// press esc to close window, say this for report (way better than clicking on the x button everytime)
		glfwSetWindowShouldClose(w, 1);
	if (key == GLFW_KEY_F && action == GLFW_PRESS) {
		// Toggle fog on/off
		fogEnabled = !fogEnabled;
		bot.fogEnabled = fogEnabled;
		std::cout << "Fog " << (fogEnabled ? "enabled" : "disabled") << std::endl;
	}

	if (key == GLFW_KEY_EQUAL && action == GLFW_PRESS) {
		// Increase fog density (see less of the planets far away)
		fogDensity += 0.005f;
		fogDensity = std::min(fogDensity, 0.2f);  // Cap at 0.2
		bot.fogDensity = fogDensity;           // Sync with bot
		std::cout << "Fog density: " << fogDensity << std::endl;
	}

	if (key == GLFW_KEY_MINUS && action == GLFW_PRESS) {
		// Decrease fog density (see more at a distance)
		fogDensity -= 0.005f;
		fogDensity = std::max(fogDensity, 0.0f);  // Can't go negative
		bot.fogDensity = fogDensity;
		std::cout << "Fog density: " << fogDensity << std::endl;
	}
}

int main() {
	// Randomizer
	std::srand(static_cast<unsigned int>(std::time(nullptr)) ^ uintptr_t(&main));

	// Init GLFW
	if (!glfwInit()) {
		std::cerr << "Failed to init GLFW\n";
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(1280, 720, "Space World", nullptr, nullptr);
	if (!window) {
		std::cerr << "Failed to create window\n";
		glfwTerminate();
		return -1;
	}

	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, key_callback);

	// Init GLAD
	if (!gladLoadGL(glfwGetProcAddress)) {
		std::cerr << "Failed to init GLAD\n";
		return -1;
	}

	glEnable(GL_DEPTH_TEST);

	init();

	double lastTime = glfwGetTime();

	// FPS tracking as in lab4
	float fTime = 0.0f;
	unsigned long frames = 0;

	// Render Loop
	while (!glfwWindowShouldClose(window)) {
		double now = glfwGetTime();
		float dt = float(now - lastTime);

		humanoidAngle += humanoidAngularSpeed * dt;
		botAnimTime += dt * playbackSpeed;
		bot.update(botAnimTime);
		for (Planet& p : planets) {
			p.rotationAngle += p.rotationSpeed * dt;
		}

		// Shift key to increase speed if exploration is too slow
		float currentSpeed = speed;
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
			currentSpeed *= speedBoost;

		lastTime = now;
		glfwPollEvents();

		// Rotation
		// Camera rotation with left, right, up and down key arrows
		if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) yaw   -= 1.5f * dt;
		if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) yaw   += 1.5f * dt;
		if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) pitch += 1.0f * dt;
		if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) pitch -= 1.0f * dt;
		pitch = glm::clamp(pitch, -1.3f, 1.3f);

		// Translation
		glm::vec3 fwd = forwardDir();
		glm::vec3 rgt = rightDir();

		// Positional movement with WASD for forward, backward, left and right
		// QE for up and down respectively
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) eye_center += fwd * currentSpeed * dt;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) eye_center -= fwd * currentSpeed * dt;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) eye_center -= rgt * currentSpeed * dt;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) eye_center += rgt * currentSpeed * dt;
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) eye_center += up  * currentSpeed * dt;
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) eye_center -= up  * currentSpeed * dt;

		// Limit exploration - infinity illusion
		wrapPosition(eye_center, WORLD_SIZE);

		int w, h;
		glfwGetFramebufferSize(window, &w, &h);
		glViewport(0, 0, w, h);

		// Matrices (computed but not used yet)
		glm::mat4 Perspective = glm::perspective(glm::radians(60.0f),
									   (float)w / (float)h,
									   0.1f, 1000.0f);
		glm::mat4 View = glm::lookAt(eye_center, lookat, up);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		lookat = eye_center + forwardDir();
		render();

		// FPS increment
		frames++;
		fTime += dt;
		if (fTime > 2.0f) {
			float fps = frames / fTime;
			frames = 0;
			fTime = 0;

			std::stringstream ss;
			ss << std::fixed << std::setprecision(2);
			ss << "CloudWorld | FPS: " << fps;
			glfwSetWindowTitle(window, ss.str().c_str());
		}

		glfwSwapBuffers(window);
	}

	cleanup();
	glfwTerminate();
	return 0;
}