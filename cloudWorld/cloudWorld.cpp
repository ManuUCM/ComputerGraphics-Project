#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

static GLFWwindow* window = nullptr;

int main() {
	// ---------- Init GLFW ----------
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

	// ---------- Init GLAD ----------
	if (!gladLoadGL(glfwGetProcAddress)) {
		std::cerr << "Failed to init GLAD\n";
		return -1;
	}

	glEnable(GL_DEPTH_TEST);

	// ---------- Static Camera ----------
	glm::vec3 eye_center(0.0f, 0.0f, 10.0f);
	glm::vec3 lookat(0.0f, 0.0f, 0.0f);
	glm::vec3 up(0.0f, 1.0f, 0.0f);

	// ---------- Render Loop ----------
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		int w, h;
		glfwGetFramebufferSize(window, &w, &h);
		glViewport(0, 0, w, h);

		// Matrices (computed but not used yet)
		glm::mat4 P = glm::perspective(glm::radians(60.0f),
									   (float)w / (float)h,
									   0.1f, 1000.0f);
		glm::mat4 V = glm::lookAt(eye_center, lookat, up);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}