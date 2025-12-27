#include "../cloudWorld/include/bot.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

glm::mat4 MyBot::getNodeTransform(const tinygltf::Node& node) {
	glm::mat4 transform(1.0f);

	if (node.matrix.size() == 16) {
		transform = glm::make_mat4(node.matrix.data());
	} else {
		if (node.translation.size() == 3) {
			transform = glm::translate(transform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
		}
		if (node.rotation.size() == 4) {
			glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
			transform *= glm::mat4_cast(q);
		}
		if (node.scale.size() == 3) {
			transform = glm::scale(transform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
		}
	}
	return transform;
}

void MyBot::computeLocalNodeTransform(const tinygltf::Model& model,
	int nodeIndex,
	std::vector<glm::mat4> &localTransforms)
{
	// ---------------------------------------
	// TODO: your code here
	// ---------------------------------------
	// Get the node from the model
	const tinygltf::Node& node = model.nodes[nodeIndex];

	// Compute and store the local transform for this node
	localTransforms[nodeIndex] = getNodeTransform(node);

	// Most intuitive way I thought to process the children is via recursion
	for (int childIndex : node.children) {
		computeLocalNodeTransform(model, childIndex, localTransforms);
	}
}

void MyBot::computeGlobalNodeTransform(const tinygltf::Model& model,
	const std::vector<glm::mat4> &localTransforms,
	int nodeIndex, const glm::mat4& parentTransform,
	std::vector<glm::mat4> &globalTransforms)
{
	// ----------------------------------------
	// TODO: your code here
	// ----------------------------------------
	// Calculate global transform: Parent * Local
	globalTransforms[nodeIndex] = parentTransform * localTransforms[nodeIndex];

	// Same as local, recursively process the children
	for (int childIndex : model.nodes[nodeIndex].children) {
		computeGlobalNodeTransform(model, localTransforms, childIndex,
								   globalTransforms[nodeIndex], globalTransforms);
	}
}

std::vector<MyBot::SkinObject> MyBot::prepareSkinning(const tinygltf::Model &model) {
	std::vector<SkinObject> skinObjects;

	// In our Blender exporter, the default number of joints that may influence a vertex is set to 4, just for convenient implementation in shaders.

	for (size_t i = 0; i < model.skins.size(); i++) {
		SkinObject skinObject;

		const tinygltf::Skin &skin = model.skins[i];

		// Read inverseBindMatrices
		const tinygltf::Accessor &accessor = model.accessors[skin.inverseBindMatrices];
		assert(accessor.type == TINYGLTF_TYPE_MAT4);
		const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
		const float *ptr = reinterpret_cast<const float *>(
            buffer.data.data() + accessor.byteOffset + bufferView.byteOffset);

		skinObject.inverseBindMatrices.resize(accessor.count);
		for (size_t j = 0; j < accessor.count; j++) {
			float m[16];
			memcpy(m, ptr + j * 16, 16 * sizeof(float));
			skinObject.inverseBindMatrices[j] = glm::make_mat4(m);
		}

		assert(skin.joints.size() == accessor.count);

		skinObject.globalJointTransforms.resize(skin.joints.size());
		skinObject.jointMatrices.resize(skin.joints.size());

		// ----------------------------------------------
		// TODO: your code here to compute joint matrices
		// ----------------------------------------------

		// calculate local transforms for each node
		int root = skin.joints[0];
		std::vector<glm::mat4> localNodeTransforms(model.nodes.size());
		computeLocalNodeTransform(model, root, localNodeTransforms);

		// Having done the locals, now calculate the global for all nodes
		std::vector<glm::mat4> globalNodeTransforms(model.nodes.size());
		computeGlobalNodeTransform(model, localNodeTransforms, root, glm::mat4(1.0f), globalNodeTransforms);

		// compute joint matrices
		for (size_t j = 0; j < skin.joints.size(); ++j) {
			int nodeIdx = skin.joints[j];
			// The global transform of the joint node
			skinObject.globalJointTransforms[j] = globalNodeTransforms[nodeIdx];

			// GlobalTransform * InverseBindMatrix is the matrix sent to the shader
			skinObject.jointMatrices[j] = skinObject.globalJointTransforms[j] * skinObject.inverseBindMatrices[j];
		}
		// ----------------------------------------------

		skinObjects.push_back(skinObject);
	}
	return skinObjects;
}

int MyBot::findKeyframeIndex(const std::vector<float>& times, float animationTime)
{
	int left = 0;
	int right = times.size() - 1;

	while (left <= right) {
		int mid = (left + right) / 2;

		if (mid + 1 < times.size() && times[mid] <= animationTime && animationTime < times[mid + 1]) {
			return mid;
		}
		else if (times[mid] > animationTime) {
			right = mid - 1;
		}
		else { // animationTime >= times[mid + 1]
			left = mid + 1;
		}
	}

	// Target not found
	return times.size() - 2;
}

std::vector<MyBot::AnimationObject> MyBot::prepareAnimation(const tinygltf::Model &model)
{
	std::vector<AnimationObject> animationObjects;
	for (const auto &anim : model.animations) {
		AnimationObject animationObject;

		for (const auto &sampler : anim.samplers) {
			SamplerObject samplerObject;

			const tinygltf::Accessor &inputAccessor = model.accessors[sampler.input];
			const tinygltf::BufferView &inputBufferView = model.bufferViews[inputAccessor.bufferView];
			const tinygltf::Buffer &inputBuffer = model.buffers[inputBufferView.buffer];

			assert(inputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
			assert(inputAccessor.type == TINYGLTF_TYPE_SCALAR);

			// Input (time) values
			samplerObject.input.resize(inputAccessor.count);

			const unsigned char *inputPtr = &inputBuffer.data[inputBufferView.byteOffset + inputAccessor.byteOffset];
			const float *inputBuf = reinterpret_cast<const float*>(inputPtr);

			// Read input (time) values
			int stride = inputAccessor.ByteStride(inputBufferView);
			for (size_t i = 0; i < inputAccessor.count; ++i) {
				samplerObject.input[i] = *reinterpret_cast<const float*>(inputPtr + i * stride);
			}

			const tinygltf::Accessor &outputAccessor = model.accessors[sampler.output];
			const tinygltf::BufferView &outputBufferView = model.bufferViews[outputAccessor.bufferView];
			const tinygltf::Buffer &outputBuffer = model.buffers[outputBufferView.buffer];

			assert(outputAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

			const unsigned char *outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];
			const float *outputBuf = reinterpret_cast<const float*>(outputPtr);

			int outputStride = outputAccessor.ByteStride(outputBufferView);

			// Output values
			samplerObject.output.resize(outputAccessor.count);

			for (size_t i = 0; i < outputAccessor.count; ++i) {

				if (outputAccessor.type == TINYGLTF_TYPE_VEC3) {
					memcpy(&samplerObject.output[i], outputPtr + i * 3 * sizeof(float), 3 * sizeof(float));
				} else if (outputAccessor.type == TINYGLTF_TYPE_VEC4) {
					memcpy(&samplerObject.output[i], outputPtr + i * 4 * sizeof(float), 4 * sizeof(float));
				} else {
					std::cout << "Unsupport accessor type ..." << std::endl;
				}

			}

			animationObject.samplers.push_back(samplerObject);
		}

		animationObjects.push_back(animationObject);
	}
	return animationObjects;
}

void MyBot::updateAnimation(
	const tinygltf::Model &model,
	const tinygltf::Animation &anim,
	const AnimationObject &animationObject,
	float time,
	std::vector<glm::mat4> &nodeTransforms)
{
	// There are many channels so we have to accumulate the transforms
	for (const auto &channel : anim.channels) {

		int targetNodeIndex = channel.target_node;
		const auto &sampler = anim.samplers[channel.sampler];

		// Access output (value) data for the channel
		const tinygltf::Accessor &outputAccessor = model.accessors[sampler.output];
		const tinygltf::BufferView &outputBufferView = model.bufferViews[outputAccessor.bufferView];
		const tinygltf::Buffer &outputBuffer = model.buffers[outputBufferView.buffer];

		// Calculate current animation time (wrap if necessary)
		const std::vector<float> &times = animationObject.samplers[channel.sampler].input;
		float animationTime = fmod(time, times.back());

		// ----------------------------------------------------------
		// TODO: Find a keyframe for getting animation data
		// ----------------------------------------------------------
		int keyframeIndex = findKeyframeIndex(times, animationTime);

		const unsigned char *outputPtr = &outputBuffer.data[outputBufferView.byteOffset + outputAccessor.byteOffset];
		const float *outputBuf = reinterpret_cast<const float*>(outputPtr);

		// -----------------------------------------------------------
		// TODO: Add interpolation for smooth interpolation
		// -----------------------------------------------------------

		float time0 = times[keyframeIndex];
		float time1 = times[keyframeIndex + 1];
		float alpha = (animationTime - time0) / (time1 - time0);
		alpha = glm::clamp(alpha, 0.0f, 1.0f);

		if (channel.target_path == "translation") {
			// translation vectors from keyframes
			glm::vec3 translation0, translation1;
			memcpy(&translation0, outputPtr + keyframeIndex * 3 * sizeof(float), 3 * sizeof(float));
			memcpy(&translation1, outputPtr + (keyframeIndex + 1) * 3 * sizeof(float), 3 * sizeof(float));

			// linear interpolation between keyframes
			glm::vec3 translation = glm::mix(translation0, translation1, alpha);
			nodeTransforms[targetNodeIndex] *= glm::translate(glm::mat4(1.0f), translation);
		} else if (channel.target_path == "rotation") {
			// quaternions from the keyframes
			glm::quat rotation0, rotation1;
			memcpy(&rotation0, outputPtr + keyframeIndex * 4 * sizeof(float), 4 * sizeof(float));
			memcpy(&rotation1, outputPtr + (keyframeIndex + 1) * 4 * sizeof(float), 4 * sizeof(float));

			// slerp - spherical linear interpolation (smooth rotation)
			glm::quat rotation = glm::slerp(rotation0, rotation1, alpha);
			nodeTransforms[targetNodeIndex] *= glm::mat4_cast(rotation);
		} else if (channel.target_path == "scale") {
			// similar process as translation, get scale vectors from keyframes and do the same type of interpolation
			glm::vec3 scale0, scale1;
			memcpy(&scale0, outputPtr + keyframeIndex * 3 * sizeof(float), 3 * sizeof(float));
			memcpy(&scale1, outputPtr + (keyframeIndex + 1) * 3 * sizeof(float), 3 * sizeof(float));

			// same as translation, linear interpolation
			glm::vec3 scale = glm::mix(scale0, scale1, alpha);
			nodeTransforms[targetNodeIndex] *= glm::scale(glm::mat4(1.0f), scale);
		}
	}
}

void MyBot::updateSkinning(const std::vector<glm::mat4> &nodeTransforms) {

	// -------------------------------------------------
	// TODO: Recompute joint matrices
	// -------------------------------------------------
	const tinygltf::Skin &skin = model.skins[0];
	SkinObject &skinObject = skinObjects[0];

	// update skinning: recompute transforms and update matrices
	// recompute global transforms using the newest animated nodeTransforms
	// again, assuming joint[0] is the root
	int root = skin.joints[0];
	std::vector<glm::mat4> globalNodeTransforms(model.nodes.size());

	computeGlobalNodeTransform(model, nodeTransforms, root, glm::mat4(1.0f), globalNodeTransforms);

	// Update the joint matrices
	for (size_t j = 0; j < skin.joints.size(); ++j) {
		int nodeIdx = skin.joints[j];
		skinObject.jointMatrices[j] = globalNodeTransforms[nodeIdx] * skinObject.inverseBindMatrices[j];
	}
}

void MyBot::update(float time) {

	// -------------------------------------------------
	// TODO: your code here
	// -------------------------------------------------
	if (model.animations.size() > 0) {
		const tinygltf::Animation &animation = model.animations[0];
		const AnimationObject &animationObject = animationObjects[0];
		const tinygltf::Skin &skin = model.skins[0];

		// base transform will be the identity matrix
		std::vector<glm::mat4> nodeTransforms(model.nodes.size());
		for (size_t i = 0; i < nodeTransforms.size(); ++i) {
			nodeTransforms[i] = glm::mat4(1.0f);
		}
		// animation channels
		updateAnimation(model, animation, animationObject, time, nodeTransforms);
		// update skinning with newest transformation nodes
		updateSkinning(nodeTransforms);
	}
}

bool MyBot::loadModel(tinygltf::Model &model, const char *filename) {
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
	if (!warn.empty()) {
		std::cout << "WARN: " << warn << std::endl;
	}

	if (!err.empty()) {
		std::cout << "ERR: " << err << std::endl;
	}

	if (!res)
		std::cout << "Failed to load glTF: " << filename << std::endl;
	else
		std::cout << "Loaded glTF: " << filename << std::endl;

	return res;
}

void MyBot::initialize() {
	// Modify your path if needed
	if (!loadModel(model, "../cloudWorld/assets/models/bot/bot.gltf")) {
		return;
	}

	// Prepare buffers for rendering
	primitiveObjects = bindModel(model);

	// Prepare joint matrices
	skinObjects = prepareSkinning(model);

	// Prepare animation data
	animationObjects = prepareAnimation(model);

	// Create and compile our GLSL program from the shaders
	std::cout << "Debug: Loading shader..." << std::endl;
	programID = LoadShadersFromFile("../cloudWorld/render/bot.vert", "../cloudWorld/render/bot.frag");
	std::cout << "Debug: programID = " << programID << std::endl;
	if (programID == 0)
	{
		std::cerr << "Failed to load shaders." << std::endl;
	}

	// Get a handle for GLSL variables
	mvpMatrixID = glGetUniformLocation(programID, "MVP");
	lightPositionID = glGetUniformLocation(programID, "lightPosition");
	lightIntensityID = glGetUniformLocation(programID, "lightIntensity");
	// Retrieve the uniform ID for the joint matrix array
	// I need the ID for the skinning To-Do in render() -> "Set animation data for linear blend skinning in shader"
	jointMatricesID = glGetUniformLocation(programID, "jointMatrices");
	modelMatrixID = glGetUniformLocation(programID, "M");
	std::cout << "Debug: jointMatricesID = " << jointMatricesID << std::endl;
}

void MyBot::bindMesh(std::vector<PrimitiveObject> &primitiveObjects,
			tinygltf::Model &model, tinygltf::Mesh &mesh) {

	std::map<int, GLuint> vbos;
	for (size_t i = 0; i < model.bufferViews.size(); ++i) {
		const tinygltf::BufferView &bufferView = model.bufferViews[i];

		int target = bufferView.target;

		if (bufferView.target == 0) {
			// The bufferView with target == 0 in our model refers to
			// the skinning weights, for 25 joints, each 4x4 matrix (16 floats), totaling to 400 floats or 1600 bytes.
			// So it is considered safe to skip the warning.
			//std::cout << "WARN: bufferView.target is zero" << std::endl;
			//continue;
			target = GL_ARRAY_BUFFER;
		}

		const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
		GLuint vbo;
		glGenBuffers(1, &vbo);
		glBindBuffer(target, vbo);
		glBufferData(target, bufferView.byteLength,
					&buffer.data.at(0) + bufferView.byteOffset, GL_STATIC_DRAW);

		vbos[i] = vbo;
	}

	// Each mesh can contain several primitives (or parts), each we need to
	// bind to an OpenGL vertex array object
	for (size_t i = 0; i < mesh.primitives.size(); ++i) {

		tinygltf::Primitive primitive = mesh.primitives[i];
		tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

		GLuint vao;
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		for (auto &attrib : primitive.attributes) {
			tinygltf::Accessor accessor = model.accessors[attrib.second];
			int byteStride =
				accessor.ByteStride(model.bufferViews[accessor.bufferView]);
			glBindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);

			int size = 1;
			if (accessor.type != TINYGLTF_TYPE_SCALAR) {
				size = accessor.type;
			}

			int vaa = -1;
																	// Debug
			if (attrib.first.compare("POSITION") == 0) vaa = 0;   // vec3 - float
			if (attrib.first.compare("NORMAL") == 0) vaa = 1;		// vec3 - float
			if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2; // vec2 - float
			if (attrib.first.compare("JOINTS_0") == 0) vaa = 3;   // uvec4 - needs to be int
			if (attrib.first.compare("WEIGHTS_0") == 0) vaa = 4;  // vec4 - float
			if (vaa > -1) {
				// I was always getting weird placements of the joints, and it ultimately led to the attributes being
				// wrongly cast to floats or ints when it was meant to be the opposite
				// After debugging, JOINTS_0 is an integer attribute and everything else float
				glEnableVertexAttribArray(vaa);
				if (attrib.first.compare("JOINTS_0") == 0) {
					// JOINTS_0 contains integer indices - must be glVertexAttribIPointer
					glVertexAttribIPointer(
						vaa,
						size,
						accessor.componentType,   // GL_UNSIGNED_BYTE or GL_UNSIGNED_SHORT
						byteStride,
						BUFFER_OFFSET(accessor.byteOffset)
					);
				} else {
					// everything else - float attributes
					glVertexAttribPointer(
						vaa,
						size,
						accessor.componentType,   // GL_FLOAT
						accessor.normalized ? GL_TRUE : GL_FALSE,
						byteStride,
						BUFFER_OFFSET(accessor.byteOffset)
					);
				}
			}
			else {
				std::cout << "vaa missing: " << attrib.first << std::endl;
			}
		}

		// Record VAO for later use
		PrimitiveObject primitiveObject;
		primitiveObject.vao = vao;
		primitiveObject.vbos = vbos;
		primitiveObjects.push_back(primitiveObject);

		glBindVertexArray(0);
	}
}

void MyBot::bindModelNodes(std::vector<PrimitiveObject> &primitiveObjects,
					tinygltf::Model &model,
					tinygltf::Node &node) {
	// Bind buffers for the current mesh at the node
	if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
		bindMesh(primitiveObjects, model, model.meshes[node.mesh]);
	}

	// Recursive into children nodes
	for (size_t i = 0; i < node.children.size(); i++) {
		assert((node.children[i] >= 0) && (node.children[i] < model.nodes.size()));
		bindModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
	}
}

std::vector<MyBot::PrimitiveObject> MyBot::bindModel(tinygltf::Model &model) {
	std::vector<PrimitiveObject> primitiveObjects;
	const tinygltf::Scene &scene = model.scenes[model.defaultScene];
	for (size_t i = 0; i < scene.nodes.size(); ++i) {
		assert((scene.nodes[i] >= 0) && (scene.nodes[i] < model.nodes.size()));
		bindModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
	}

	return primitiveObjects;
}

void MyBot::drawMesh(const std::vector<PrimitiveObject> &primitiveObjects, tinygltf::Model &model, tinygltf::Mesh &mesh) {
	for (size_t i = 0; i < mesh.primitives.size(); ++i)
	{
		GLuint vao = primitiveObjects[i].vao;
		std::map<int, GLuint> vbos = primitiveObjects[i].vbos;

		glBindVertexArray(vao);

		tinygltf::Primitive primitive = mesh.primitives[i];
		tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));

		glDrawElements(primitive.mode, indexAccessor.count,
					indexAccessor.componentType,
					BUFFER_OFFSET(indexAccessor.byteOffset));

		glBindVertexArray(0);
	}
}

void MyBot::drawModelNodes(const std::vector<PrimitiveObject>& primitiveObjects, tinygltf::Model &model, tinygltf::Node &node) {
	// Draw the mesh at the node, and recursively do so for children nodes
	if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
		drawMesh(primitiveObjects, model, model.meshes[node.mesh]);
	}
	for (size_t i = 0; i < node.children.size(); i++) {
		drawModelNodes(primitiveObjects, model, model.nodes[node.children[i]]);
	}
}
void MyBot::drawModel(const std::vector<PrimitiveObject>& primitiveObjects, tinygltf::Model &model) {
	// Draw all nodes
	const tinygltf::Scene &scene = model.scenes[model.defaultScene];
	for (size_t i = 0; i < scene.nodes.size(); ++i) {
		drawModelNodes(primitiveObjects, model, model.nodes[scene.nodes[i]]);
	}
}

void MyBot::render(glm::mat4 cameraMatrix, const glm::mat4& M) {
	glUseProgram(programID);

	// Set camera
	glm::mat4 mvp = cameraMatrix;
	glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);
	glUniformMatrix4fv(modelMatrixID, 1, GL_FALSE, glm::value_ptr(M));

	// -----------------------------------------------------------------
	// TODO: Set animation data for linear blend skinning in shader
	// -----------------------------------------------------------------
	if (!skinObjects.empty()) {
		// First skin
		const std::vector<glm::mat4> &jointMatrices = skinObjects[0].jointMatrices;
		glUniformMatrix4fv(jointMatricesID, jointMatrices.size(), GL_FALSE, glm::value_ptr(jointMatrices[0]));
	}
	// -----------------------------------------------------------------
	// Set light data
	glUniform3fv(lightPositionID, 1, &lightPosition[0]);
	glUniform3fv(lightIntensityID, 1, &lightIntensity[0]);

	// Draw the GLTF model
	drawModel(primitiveObjects, model);
}

void MyBot::cleanup() {
	glDeleteProgram(programID);
}