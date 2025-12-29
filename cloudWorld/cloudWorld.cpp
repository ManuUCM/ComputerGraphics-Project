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

// Static Camera
glm::mat4 viewMatrix;
glm::mat4 projectionMatrix;
glm::mat4 modelMatrix;
glm::float32 FoV   = 100.0f;
glm::float32 zNear = 0.1f;
glm::float32 zFar  = 3500.0f;
static glm::vec3 eye_center(0.0f, 0.0f, 10.0f);
static glm::vec3 lookat(0.0f, 0.0f, 0.0f);
static glm::vec3 up(0.0f, 1.0f, 0.0f);
static float yaw = 0.0f;    // left/right
static float pitch = 0.0f;  // up/down
static float speed = 10.0f;
static float speedBoost = 2.5f;

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
static const int NUM_PLANETS = 14;
static const float PLANET_FIELD_RADIUS = 220.0f;
static const float MIN_PLANET_DISTANCE = 80.0f;
//Textures
static const int NUM_PLANET_TEXTURES = 17;
GLuint planetTextures[NUM_PLANET_TEXTURES];
GLuint planetTextureSampler;
// Better spawn and despawn for procedural planets
static const float SPAWN_RADIUS   = 260.0f;
static const float DESPAWN_RADIUS = 300.0f;
static const int   MAX_PLANETS    = 14;
glm::vec3 lastSpawnCenter;

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
	// Sphere formula (inspired in quiz and lighting lecture
	// x= sin(ϕ) * cos(θ)
	// y= cos(ϕ)
	// z= sin(ϕ) * sin(θ)
	// where ϕ ∈ [0,π] (stacks) and 𝜃 ∈ [0,2𝜋] θ ∈ [0,2π] (slices).
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
			p.radius = 2.5f + t * t * 10.0f;       // small planets common, big ones rare
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
	planetTextures[0] = LoadTexture("../cloudWorld/assets/textures/aerialRock.jpg");
	planetTextures[1] = LoadTexture("../cloudWorld/assets/textures/aerialMud.jpg");
	planetTextures[2] = LoadTexture("../cloudWorld/assets/textures/jerseyMelange.jpg");
	planetTextures[3] = LoadTexture("../cloudWorld/assets/textures/lichenRock.jpg");
	planetTextures[4] = LoadTexture("../cloudWorld/assets/textures/rockBoulderDry.jpg");
	planetTextures[5] = LoadTexture("../cloudWorld/assets/textures/slateFloor.jpg");
	planetTextures[6] = LoadTexture("../cloudWorld/assets/textures/snowField.jpg");
	planetTextures[7] = LoadTexture("../cloudWorld/assets/textures/barkWillow.jpg");
	planetTextures[8] = LoadTexture("../cloudWorld/assets/textures/barkWillow2.jpg");
	planetTextures[9] = LoadTexture("../cloudWorld/assets/textures/crepeSatin.jpg");
	planetTextures[10] = LoadTexture("../cloudWorld/assets/textures/gangesRiverPebbles.jpg");
	planetTextures[11] = LoadTexture("../cloudWorld/assets/textures/gravel.jpg");
	planetTextures[12] = LoadTexture("../cloudWorld/assets/textures/rockBump.jpg");
	planetTextures[13] = LoadTexture("../cloudWorld/assets/textures/rockPitted.jpg");
	planetTextures[14] = LoadTexture("../cloudWorld/assets/textures/roughLinen.jpg");
	planetTextures[15] = LoadTexture("../cloudWorld/assets/textures/terryCloth.jpg");
	planetTextures[16] = LoadTexture("../cloudWorld/assets/textures/velourVelvet.jpg");

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
	// Update view matrix
	viewMatrix = glm::lookAt(
		eye_center,
		lookat,
		up
	);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Skybox
	drawSkybox(projectionMatrix, viewMatrix);

	// Procedural planets
	glUseProgram(planetProgramID);
	// Environment lighting
	glUniform3f(glGetUniformLocation(planetProgramID, "envColor"),
			0.4f, 0.55f, 0.65f);

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

		// Calculate position on planet surface using spherical coordinates
		float theta = humanoidAngle;
		float phi = glm::radians(25.0f);  // Latitude angle on planet

		// Position on unit sphere surface
		glm::vec3 localSurfacePos(
			sin(phi) * cos(theta),
			cos(phi),
			sin(phi) * sin(theta)
		);

		// Scale to planet surface and apply run radius factor
		glm::vec3 surfaceOffset = localSurfacePos * (hp.radius * 0.8f);

		// Final world position
		glm::vec3 humanoidWorldPos = wrappedPlanetPos + surfaceOffset;

		// Calculate orientation to stand upright on planet surface
		glm::vec3 up_vector = glm::normalize(localSurfacePos);
		glm::vec3 tangent = glm::normalize(glm::cross(glm::vec3(0, 1, 0), up_vector));

		// Handle edge case when up_vector aligns with world Y axis
		if (glm::length(tangent) < 0.001f) {
			tangent = glm::vec3(1, 0, 0);
		}
		glm::vec3 forward = glm::normalize(glm::cross(up_vector, tangent));

		// Construct rotation matrix from orientation vectors
		glm::mat4 rotationMatrix = glm::mat4(1.0f);
		rotationMatrix[0] = glm::vec4(tangent, 0.0f);
		rotationMatrix[1] = glm::vec4(up_vector, 0.0f);
		rotationMatrix[2] = glm::vec4(forward, 0.0f);

		// Scale humanoid proportionally to planet size
		float humanoidScale = hp.radius * 2.0f;

		// Calculate a correction offset (we'll determine these empirically)
		glm::vec3 botPositionCorrection = glm::vec3(1, 0, 1);  // Start with zero

		// Apply correction to the humanoid's world position
		glm::vec3 correctedHumanoidPos = humanoidWorldPos + botPositionCorrection;

		glm::mat4 humanoidModelMatrix =
			glm::translate(glm::mat4(1.0f), correctedHumanoidPos) *
			rotationMatrix *
			glm::scale(glm::mat4(1.0f), glm::vec3(humanoidScale));

		bot.render(projectionMatrix * viewMatrix, humanoidModelMatrix);

		// glm::mat4 markerModel =
		// 	glm::translate(glm::mat4(1.0f), humanoidWorldPos) *
		// 	glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));   // small marker
		//
		// std::cout << "modelCenter from bot: [" << bot.modelCenter.x << ", "
		//   << bot.modelCenter.y << ", " << bot.modelCenter.z << "]" << std::endl;
		// std::cout << "modelScale from bot: " << bot.modelScale << std::endl;
		//
		// glm::mat4 markerMVP = projectionMatrix * viewMatrix * markerModel;
		//
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
		glfwSetWindowShouldClose(w, 1);
}

int main() {
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

	glPointSize(10.0f);

	glEnable(GL_DEPTH_TEST);

	init();

	double lastTime = glfwGetTime();

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

		glfwSwapBuffers(window);
	}

	cleanup();
	glfwTerminate();
	return 0;
}