#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

static GLFWwindow* window = nullptr;

// Static Camera
static glm::vec3 eye_center(0.0f, 0.0f, 10.0f);
static glm::vec3 lookat(0.0f, 0.0f, 0.0f);
static glm::vec3 up(0.0f, 1.0f, 0.0f);
static float yaw = 0.0f;    // left/right
static float pitch = 0.0f;  // up/down
static float speed = 10.0f;

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

static void key_callback(GLFWwindow* w, int key, int, int action, int) {
	if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
		glfwSetWindowShouldClose(w, 1);
}

int main() {
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

	double lastTime = glfwGetTime();

	// Render Loop
	while (!glfwWindowShouldClose(window)) {
		double now = glfwGetTime();
		float dt = float(now - lastTime);
		lastTime = now;

		glfwPollEvents();

		// Rotation
		if (glfwGetKey(window, GLFW_KEY_LEFT)  == GLFW_PRESS) yaw   -= 1.5f * dt;
		if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) yaw   += 1.5f * dt;
		if (glfwGetKey(window, GLFW_KEY_UP)    == GLFW_PRESS) pitch += 1.0f * dt;
		if (glfwGetKey(window, GLFW_KEY_DOWN)  == GLFW_PRESS) pitch -= 1.0f * dt;
		pitch = glm::clamp(pitch, -1.3f, 1.3f);

		// Translation
		glm::vec3 fwd = forwardDir();
		glm::vec3 rgt = rightDir();

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) eye_center += fwd * speed * dt;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) eye_center -= fwd * speed * dt;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) eye_center -= rgt * speed * dt;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) eye_center += rgt * speed * dt;
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) eye_center += up  * speed * dt;
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) eye_center -= up  * speed * dt;

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

		static double printTimer = 0.0;
		printTimer += dt;

		if (printTimer > 0.25) { // imprime cada 0.25s
			std::cout << "eye = ("
					  << eye_center.x << ", "
					  << eye_center.y << ", "
					  << eye_center.z << ") "
					  << " yaw=" << yaw
					  << " pitch=" << pitch
					  << std::endl;
			printTimer = 0.0;
		}


		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}