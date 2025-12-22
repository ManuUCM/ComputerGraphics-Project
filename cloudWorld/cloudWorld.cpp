#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <render/shader.h>

#include <iostream>
#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>

static GLFWwindow *window;
static int windowWidth = 1024;
static int windowHeight = 768;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);
static void cursor_callback(GLFWwindow *window, double xpos, double ypos);

// OpenGL camera view parameters
static glm::vec3 eye_center(-278.0f, 273.0f, 800.0f);
static glm::vec3 lookat(-278.0f, 273.0f, 0.0f);
static glm::vec3 up(0.0f, 1.0f, 0.0f);
static float FoV = 45.0f;
static float zNear = 600.0f;
static float zFar = 1500.0f; 

// Lighting control 
const glm::vec3 wave500(0.0f, 255.0f, 146.0f);
const glm::vec3 wave600(255.0f, 190.0f, 0.0f);
const glm::vec3 wave700(205.0f, 0.0f, 0.0f);
// Added an additional * 10.0f since it was very dark
static glm::vec3 lightIntensity = 5.0f * (8.0f * wave500 + 15.6f * wave600 + 18.4f * wave700) * 10.0f;
//static glm::vec3 lightPosition(-275.0f, 500.0f, -275.0f);
// Referring to cornellbox specifications, otherwise the light appeared below the ceiling
static glm::vec3 lightPosition = glm::vec3(-278.0f, 548.5f, -279.5f);

// Shadow mapping
static glm::vec3 lightUp(0, 0, 1);
static int shadowMapWidth = 0;
static int shadowMapHeight = 0;
// Step 0: shadow FBO
static GLuint shadowFBO = 0;
static GLuint shadowDepthTexture = 0;

// TODO: set these parameters 
static float depthFoV = 90.f;
static float depthNear = 100.f;
static float depthFar = 800.f;

// Helper flag and function to save depth maps for debugging
static bool saveDepth = false;

// This function retrieves and stores the depth map of the default frame buffer 
// or a particular frame buffer (indicated by FBO ID) to a PNG image.
static void saveDepthTexture(GLuint fbo, std::string filename) {
    int width = shadowMapWidth;
    int height = shadowMapHeight;
	if (shadowMapWidth == 0 || shadowMapHeight == 0) {
		width = windowWidth;
		height = windowHeight;
	}
    int channels = 3; 
    
    std::vector<float> depth(width * height);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glReadBuffer(GL_DEPTH_COMPONENT);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::vector<unsigned char> img(width * height * 3);
    for (int i = 0; i < width * height; ++i) img[3*i] = img[3*i+1] = img[3*i+2] = depth[i] * 255;

    stbi_write_png(filename.c_str(), width, height, channels, img.data(), width * channels);
}

struct CornellBox {

	// Refer to original Cornell Box data 
	// from https://www.graphics.cornell.edu/online/box/data.html

	GLfloat vertex_buffer_data[60] = {
		// Floor
		-552.8, 0.0, 0.0,
		0.0, 0.0,   0.0,
		0.0, 0.0, -559.2,
		-549.6, 0.0, -559.2,

		// Ceiling
		-556.0, 548.8, 0.0,
		-556.0, 548.8, -559.2,
		0.0, 548.8, -559.2,
		0.0, 548.8,   0.0,

		// Left wall 
		-552.8,   0.0,   0.0,
		-549.6,   0.0, -559.2,
		-556.0, 548.8, -559.2,
		-556.0, 548.8,   0.0,

		// Right wall 
		0.0,   0.0, -559.2,
		0.0,   0.0,   0.0,
		0.0, 548.8,   0.0,
		0.0, 548.8, -559.2,

		// Back wall 
		-549.6,   0.0, -559.2,
		0.0,   0.0, -559.2,
		0.0, 548.8, -559.2,
		-556.0, 548.8, -559.2
	};

	// TODO: set vertex normals properly
	GLfloat normal_buffer_data[60] = {
		// Floor 
		0.0, 1.0, 0.0,   
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,

		// Ceiling
		0.0, -1.0, 0.0,   
		0.0, -1.0, 0.0,
		0.0, -1.0, 0.0,
		0.0, -1.0, 0.0,

		// Left wall 
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,

		// Right wall 
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,

		// Back wall 
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0
	};

	GLfloat color_buffer_data[60] = {
		// Floor
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		// Ceiling
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		// Left wall
		1.0f, 0.0f, 0.0f, 
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,

		// Right wall
		0.0f, 1.0f, 0.0f, 
		0.0f, 1.0f, 0.0f, 
		0.0f, 1.0f, 0.0f, 
		0.0f, 1.0f, 0.0f, 

		// Back wall
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 
		1.0f, 1.0f, 1.0f, 
		1.0f, 1.0f, 1.0f  
	};

	GLuint index_buffer_data[30] = {
		0, 1, 2, 	
		0, 2, 3, 
		
		4, 5, 6, 
		4, 6, 7, 

		8, 9, 10, 
		8, 10, 11, 

		12, 13, 14, 
		12, 14, 15, 

		16, 17, 18, 
		16, 18, 19, 
	};

	// OpenGL buffers
	GLuint vertexArrayID; 
	GLuint vertexBufferID; 
	GLuint indexBufferID; 
	GLuint colorBufferID;
	GLuint normalBufferID;

	// Shader variable IDs
	GLuint mvpMatrixID;
	GLuint modelMatrixID;
	GLuint lightPositionID;
	GLuint lightIntensityID;
	GLuint programID;

	// Shading
	GLuint lightVPMatrixID;
	GLuint shadowMapID;

	void initialize() {
		// Create a vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create a vertex buffer object to store the vertex data		
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the color data
		glGenBuffers(1, &colorBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(color_buffer_data), color_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the vertex normals		
		glGenBuffers(1, &normalBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(normal_buffer_data), normal_buffer_data, GL_STATIC_DRAW);

		// Create an index buffer object to store the index data that defines triangle faces
		glGenBuffers(1, &indexBufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);

		// Create and compile our GLSL program from the shaders
		programID = LoadShadersFromFile("../cloudWorld/box.vert", "../cloudWorld/box.frag");
		if (programID == 0)
		{
			std::cerr << "Failed to load shaders." << std::endl;
		}

		// Get a handle for our "MVP" uniform
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		// Local object model
		modelMatrixID = glGetUniformLocation(programID, "Model");
		lightPositionID = glGetUniformLocation(programID, "lightPosition");
		lightIntensityID = glGetUniformLocation(programID, "lightIntensity");
		// TODO initialize lightVPMatrix and shadowMap step 3 shades
		lightVPMatrixID = glGetUniformLocation(programID, "LightVP");
		shadowMapID = glGetUniformLocation(programID, "shadowMap");
	}

	void render(glm::mat4 cameraMatrix, glm::mat4 lightVP, bool shadowPass) {
		glUseProgram(programID);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);

		// Set model-view-projection matrix
		glm::mat4 mvp = cameraMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		// Model matrix
		glm::mat4 Model = glm::mat4(1.0f);
		// Send MVP and Model to shaders
		glUniformMatrix4fv(modelMatrixID, 1, GL_FALSE, &Model[0][0]);

		if (!shadowPass) {
			// Set light data
			glUniform3fv(lightPositionID, 1, &lightPosition[0]);
			glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);

			// TODO Step 3 shades
			// Send light VP matrix
			glUniformMatrix4fv(lightVPMatrixID, 1, GL_FALSE, &lightVP[0][0]);

			// Bind shadow map texture
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
			glUniform1i(shadowMapID, 1);
		}
		else {
			glUniformMatrix4fv(lightVPMatrixID, 1, GL_FALSE, &lightVP[0][0]);
		}

		// Draw the box
		glDrawElements(
			GL_TRIANGLES,      // mode
			30,    			   // number of indices
			GL_UNSIGNED_INT,   // type
			(void*)0           // element array buffer offset
		);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
	}

	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &colorBufferID);
		glDeleteBuffers(1, &indexBufferID);
		glDeleteBuffers(1, &normalBufferID);
		glDeleteVertexArrays(1, &vertexArrayID);
		glDeleteProgram(programID);
	}
};

struct shortBlock {
	GLfloat vertex_buffer_data[60] = {
		//With x and z inverted

		-130.0, 165.0,  -65.0,
		-82.0, 165.0, -225.0,
		-240.0, 165.0, -272.0,
		-290.0, 165.0, -114.0,

		-290.0,   0.0, -114.0,
		-290.0, 165.0, -114.0,
		-240.0, 165.0, -272.0,
		-240.0,   0.0, -272.0,

		-130.0,   0.0,  -65.0,
		-130.0, 165.0,  -65.0,
		-290.0, 165.0, -114.0,
		-290.0,   0.0, -114.0,

		 -82.0,   0.0, -225.0,
		 -82.0, 165.0, -225.0,
		-130.0, 165.0,  -65.0,
		-130.0,   0.0,  -65.0,

		-240.0,   0.0, -272.0,
		-240.0, 165.0, -272.0,
		 -82.0, 165.0, -225.0,
		 -82.0,   0.0, -225.0
	};

	GLfloat normal_buffer_data[60] = {
		// Top
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		// Right
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		// Front
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		// Left
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		// Back
		0.0, 0.0, -1.0,
		0.0, 0.0, -1.0,
		0.0, 0.0, -1.0,
		0.0, 0.0, -1.0
	};

	GLfloat color_buffer_data[60] = {
		//all white from specs
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f
	};

	GLuint index_buffer_data[30] = {
		0, 1, 2,
		0, 2, 3,

		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		16, 17, 18,
		16, 18, 19,
	};

	GLuint vertexArrayID;
	GLuint vertexBufferID;
	GLuint indexBufferID;
	GLuint colorBufferID;
	GLuint normalBufferID;

	GLuint mvpMatrixID;
	GLuint modelMatrixID;
	GLuint lightPositionID;
	GLuint lightIntensityID;
	GLuint programID;

	// step 2 shades
	GLuint lightVPMatrixID;
	GLuint shadowMapID;

	 void initialize(GLuint program) {

	 	// Avoid loading shaders once again
        programID = program;
	 	lightVPMatrixID = glGetUniformLocation(programID, "LightVP");
	 	shadowMapID = glGetUniformLocation(programID, "shadowMap");

        glGenVertexArrays(1, &vertexArrayID);
        glBindVertexArray(vertexArrayID);

        glGenBuffers(1,&vertexBufferID);
        glBindBuffer(GL_ARRAY_BUFFER,vertexBufferID);
        glBufferData(GL_ARRAY_BUFFER,sizeof(vertex_buffer_data),vertex_buffer_data,GL_STATIC_DRAW);

        glGenBuffers(1,&colorBufferID);
        glBindBuffer(GL_ARRAY_BUFFER,colorBufferID);
        glBufferData(GL_ARRAY_BUFFER,sizeof(color_buffer_data),color_buffer_data,GL_STATIC_DRAW);

        glGenBuffers(1,&normalBufferID);
        glBindBuffer(GL_ARRAY_BUFFER,normalBufferID);
        glBufferData(GL_ARRAY_BUFFER,sizeof(normal_buffer_data),normal_buffer_data,GL_STATIC_DRAW);

        glGenBuffers(1,&indexBufferID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indexBufferID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(index_buffer_data),index_buffer_data,GL_STATIC_DRAW);

        mvpMatrixID = glGetUniformLocation(programID,"MVP");
        modelMatrixID = glGetUniformLocation(programID,"Model");
        lightPositionID = glGetUniformLocation(programID,"lightPosition");
        lightIntensityID = glGetUniformLocation(programID,"lightIntensity");
    }

    void render(glm::mat4 cameraMatrix, glm::mat4 lightVP, bool shadowPass) {

        glUseProgram(programID);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER,vertexBufferID);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);

        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER,colorBufferID);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,0,0);

        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER,normalBufferID);
        glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,0,0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indexBufferID);

        glm::mat4 Model = glm::mat4(1.0f);
        glm::mat4 mvp = cameraMatrix;

        glUniformMatrix4fv(mvpMatrixID,1,GL_FALSE,&mvp[0][0]);
        glUniformMatrix4fv(modelMatrixID,1,GL_FALSE,&Model[0][0]);

	 	if (!shadowPass) {
	 		glUniform3fv(lightPositionID,1,&lightPosition[0]);
	 		glUniform3fv(lightIntensityID,1,&lightIntensity[0]);
	 		// TODO step 2 add same shading as in the cornellbox
	 		glUniformMatrix4fv(lightVPMatrixID,1,GL_FALSE,&lightVP[0][0]);
	 		glActiveTexture(GL_TEXTURE1);
	 		glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
	 		glUniform1i(shadowMapID,1);
	 	}
	 	else {
	 		glUniformMatrix4fv(lightVPMatrixID, 1, GL_FALSE, &lightVP[0][0]);
	 	}

        glDrawElements(GL_TRIANGLES,30,GL_UNSIGNED_INT,(void*)0);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }

    void cleanup() {
        glDeleteBuffers(1,&vertexBufferID);
        glDeleteBuffers(1,&colorBufferID);
        glDeleteBuffers(1,&indexBufferID);
        glDeleteBuffers(1,&normalBufferID);
        glDeleteVertexArrays(1,&vertexArrayID);
    }
};

struct tallBlock {
	GLfloat vertex_buffer_data[60] = {
		-423.0, 330.0, -247.0,
		-265.0, 330.0, -296.0,
		-314.0, 330.0, -456.0,
		-472.0, 330.0, -406.0,

		-423.0,   0.0, -247.0,
		-423.0, 330.0, -247.0,
		-472.0, 330.0, -406.0,
		-472.0,   0.0, -406.0,

		-472.0,   0.0, -406.0,
		-472.0, 330.0, -406.0,
		-314.0, 330.0, -456.0,
		-314.0,   0.0, -456.0,

		-314.0,   0.0, -456.0,
		-314.0, 330.0, -456.0,
		-265.0, 330.0, -296.0,
		-265.0,   0.0, -296.0,

		-265.0,   0.0, -296.0,
		-265.0, 330.0, -296.0,
		-423.0, 330.0, -247.0,
		-423.0,   0.0, -247.0
	};

	GLfloat normal_buffer_data[60] = {
		// Top
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 0.0,

		// Right
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,
		1.0, 0.0, 0.0,

		// Back
		0.0, 0.0, -1.0,
		0.0, 0.0, -1.0,
		0.0, 0.0, -1.0,
		0.0, 0.0, -1.0,

		// Left
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,
		-1.0, 0.0, 0.0,

		// Front
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0,
		0.0, 0.0, 1.0
	};

	GLfloat color_buffer_data[60] = {
		//all white from specs
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,

		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f
	};

	GLuint index_buffer_data[30] = {
		0, 1, 2,
		0, 2, 3,

		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		16, 17, 18,
		16, 18, 19,
	};

	GLuint vertexArrayID;
    GLuint vertexBufferID;
    GLuint indexBufferID;
    GLuint colorBufferID;
    GLuint normalBufferID;

    GLuint mvpMatrixID;
    GLuint modelMatrixID;
    GLuint lightPositionID;
    GLuint lightIntensityID;
    GLuint programID;

	// step 2 shades
	GLuint lightVPMatrixID;
	GLuint shadowMapID;

    void initialize(GLuint program) {

    	// Avoid loading shaders once again
        programID = program;
    	lightVPMatrixID = glGetUniformLocation(programID, "LightVP");
    	shadowMapID = glGetUniformLocation(programID, "shadowMap");

        glGenVertexArrays(1,&vertexArrayID);
        glBindVertexArray(vertexArrayID);

        glGenBuffers(1,&vertexBufferID);
        glBindBuffer(GL_ARRAY_BUFFER,vertexBufferID);
        glBufferData(GL_ARRAY_BUFFER,sizeof(vertex_buffer_data),vertex_buffer_data,GL_STATIC_DRAW);

        glGenBuffers(1,&colorBufferID);
        glBindBuffer(GL_ARRAY_BUFFER,colorBufferID);
        glBufferData(GL_ARRAY_BUFFER,sizeof(color_buffer_data),color_buffer_data,GL_STATIC_DRAW);

        glGenBuffers(1,&normalBufferID);
        glBindBuffer(GL_ARRAY_BUFFER,normalBufferID);
        glBufferData(GL_ARRAY_BUFFER,sizeof(normal_buffer_data),normal_buffer_data,GL_STATIC_DRAW);

        glGenBuffers(1,&indexBufferID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indexBufferID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(index_buffer_data),index_buffer_data,GL_STATIC_DRAW);

        mvpMatrixID = glGetUniformLocation(programID,"MVP");
        modelMatrixID = glGetUniformLocation(programID,"Model");
        lightPositionID = glGetUniformLocation(programID,"lightPosition");
        lightIntensityID = glGetUniformLocation(programID,"lightIntensity");
    }

    void render(glm::mat4 cameraMatrix, glm::mat4 lightVP, bool shadowPass) {
	    glUseProgram(programID);

    	glEnableVertexAttribArray(0);
    	glBindBuffer(GL_ARRAY_BUFFER,vertexBufferID);
    	glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,0,0);

    	glEnableVertexAttribArray(1);
    	glBindBuffer(GL_ARRAY_BUFFER,colorBufferID);
    	glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,0,0);

    	glEnableVertexAttribArray(2);
    	glBindBuffer(GL_ARRAY_BUFFER,normalBufferID);
    	glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,0,0);

    	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indexBufferID);

    	glm::mat4 Model = glm::mat4(1.0f);
    	glm::mat4 mvp = cameraMatrix;

    	glUniformMatrix4fv(mvpMatrixID,1,GL_FALSE,&mvp[0][0]);
    	glUniformMatrix4fv(modelMatrixID,1,GL_FALSE,&Model[0][0]);

    	if (!shadowPass) {
    		glUniform3fv(lightPositionID,1,&lightPosition[0]);
    		glUniform3fv(lightIntensityID,1,&lightIntensity[0]);
    		// TODO step 2 add same shading as in the cornellbox
    		glUniformMatrix4fv(lightVPMatrixID,1,GL_FALSE,&lightVP[0][0]);
    		glActiveTexture(GL_TEXTURE1);
    		glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
    		glUniform1i(shadowMapID,1);
		}
    	else {
    		glUniformMatrix4fv(lightVPMatrixID, 1, GL_FALSE, &lightVP[0][0]);
    	}

        glDrawElements(GL_TRIANGLES,30,GL_UNSIGNED_INT,(void*)0);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
    }

    void cleanup() {
        glDeleteBuffers(1,&vertexBufferID);
        glDeleteBuffers(1,&colorBufferID);
        glDeleteBuffers(1,&indexBufferID);
        glDeleteBuffers(1,&normalBufferID);
        glDeleteVertexArrays(1,&vertexArrayID);
    }
};

static void initShadowFBO() {

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

	// Texture params
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

	// No color buffer only depth
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW." << std::endl;
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // For MacOS
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(windowWidth, windowHeight, "Cloud World", NULL, NULL);
	if (window == NULL)
	{
		std::cerr << "Failed to open a GLFW window." << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetKeyCallback(window, key_callback);

	glfwSetCursorPosCallback(window, cursor_callback);

	// Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
	int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0)
	{
		std::cerr << "Failed to initialize OpenGL context." << std::endl;
		return -1;
	}

	// Prepare shadow map size for shadow mapping. Usually this is the size of the window itself, but on some platforms like Mac this can be 2x the size of the window. Use glfwGetFramebufferSize to get the shadow map size properly. 
    glfwGetFramebufferSize(window, &shadowMapWidth, &shadowMapHeight);
	// Initialize FBO
	initShadowFBO();

	// Background
	glClearColor(0.2f, 0.2f, 0.25f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

    // Create the classical Cornell Box
	CornellBox b;
	b.initialize();

	shortBlock sb;
	sb.initialize(b.programID); // reuse cornellbox programID for both blocks

	tallBlock tb;
	tb.initialize(b.programID);

	// Camera setup
    glm::mat4 viewMatrix, projectionMatrix;
	projectionMatrix = glm::perspective(glm::radians(FoV), (float)windowWidth / windowHeight, zNear, zFar);

	do
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Shadow pass
		// render scene from lightPoint point of view
		// Bind shadow framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
		glViewport(0, 0, shadowMapWidth, shadowMapHeight);

		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		// Clear depth buffer only
		glClear(GL_DEPTH_BUFFER_BIT);

		// Light-space view matrix
		glm::mat4 lightView = glm::lookAt(
			lightPosition,
			lightPosition + glm::vec3(0.0f, -1.0f, 0.0f),   // looking straight down
			glm::vec3(0.0f, 0.0f, 1.0f)                    // up direction (Z)
		);

		// Light-space projection
		glm::mat4 lightProjection = glm::perspective(
			glm::radians(depthFoV),
			1.0f,
			depthNear,
			depthFar
		);

		glm::mat4 lightVP = lightProjection * lightView;

		// Render the Cornell box from the POV of the light
		//b.render(lightVP);
		b.render(lightVP, glm::mat4(1.0f), true);
		sb.render(lightVP, glm::mat4(1.0f), true);
		tb.render(lightVP, glm::mat4(1.0f), true);

		/*
		saveDepthTexture(shadowFBO, "shadow_debug.png");
		std::cout << "Saved shadow map" << std::endl;
		exit(0);
		*/

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		// Camera pass

		// Restore default framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, windowWidth, windowHeight);

		viewMatrix = glm::lookAt(eye_center, lookat, up);
		glm::mat4 vp = projectionMatrix * viewMatrix;

		b.render(vp, lightVP, false);
		sb.render(vp, lightVP, false);
		tb.render(vp, lightVP, false);


		if (saveDepth) {
            std::string filename = "depth_camera.png";
            saveDepthTexture(0, filename);
            std::cout << "Depth texture saved to " << filename << std::endl;
            saveDepth = false;
        }

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (!glfwWindowShouldClose(window));

	// Clean up
	b.cleanup();
	sb.cleanup();
	tb.cleanup();

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
	if (key == GLFW_KEY_R && action == GLFW_PRESS)
	{
		eye_center = glm::vec3(-278.0f, 273.0f, 800.0f);
		lightPosition = glm::vec3(-275.0f, 500.0f, -275.0f);
	}

	if (key == GLFW_KEY_UP && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		eye_center.y += 20.0f;
	}

	if (key == GLFW_KEY_DOWN && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		eye_center.y -= 20.0f;
	}

	if (key == GLFW_KEY_LEFT && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		eye_center.x -= 20.0f;
	}

	if (key == GLFW_KEY_RIGHT && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		eye_center.x += 20.0f;
	}

	if (key == GLFW_KEY_W && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		lightPosition.z -= 20.0f;
	}

	if (key == GLFW_KEY_S && (action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		lightPosition.z += 20.0f;
	}
	    
	if (key == GLFW_KEY_SPACE && (action == GLFW_REPEAT || action == GLFW_PRESS)) 
    {
        saveDepth = true;
    }

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GL_TRUE);
}

static void cursor_callback(GLFWwindow *window, double xpos, double ypos) {
	if (xpos < 0 || xpos >= windowWidth || ypos < 0 || ypos > windowHeight) 
		return;

	// Normalize to [0, 1] 
	float x = xpos / windowWidth;
	float y = ypos / windowHeight;

	// To [-1, 1] and flip y up 
	x = x * 2.0f - 1.0f;
	y = 1.0f - y * 2.0f;

	const float scale = 250.0f;
	lightPosition.x = x * scale - 278;
	lightPosition.y = y * scale + 400;
	//lightPosition.y = 548.0f;

	//std::cout << lightPosition.x << " " << lightPosition.y << " " << lightPosition.z << std::endl;
}
