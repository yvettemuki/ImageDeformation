#include <iostream>
#include <cassert>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "gl_core_3_3.h"
#include <GL/freeglut.h>
#include "util.hpp"
#include "mesh.hpp"
#include "stb_image.h"
using namespace std;
using namespace glm;

// Vertex format
struct vert {
	glm::vec3 pos;		// 3D Position
	glm::vec2 tc;		// Texture coordinate
};

// Global state
GLint width, height;			     // Window size
GLint texWidthA, texHeightA;		 // TextureA size
GLint texWidthB, texHeightB;		 // TextureB size
GLint texWidthSaved, texHeightSaved; // TextureSaved size
GLint texWidth, texHeight;           // Grid size
unsigned char* texDataA;			 // TextureA pixel data
unsigned char* texDataB;			 // TextureB pixel data
unsigned char* texDataSaved;		 // TextureSaved pixel data
GLuint textureA;			// TextureA object
GLuint textureB;			// TextureB object
GLuint textureSaved;		// TextureSaved object
GLuint shader;			// Shader program
GLuint uniXform;		// Shader location of xform mtx
GLuint vao;				// Vertex array object
GLuint vbuf;			// Vertex buffer
GLuint ibuf;			// Index buffer
GLsizei vcount;			// Number of vertices
vector<vert> verts;		// Vertex data
vector<vert> vertices;  // Grid data
vector<GLuint> ids;		// Index data
vector<GLuint> indexs;		// Grid Index data (draw lines)
// Program state
bool clicking;
std::mt19937 rng;
std::uniform_real_distribution<float> noise;
int texType;
int deformType;
float widthRatio;
float heightRatio;
float rotAngle;

// Constants
const int MENU_EXIT = 0;	// Exit application
const int TEXTURE_A = 2;    // Texture A
const int TEXTURE_B = 3;    // Texture B
const int TEXTURE_SAVED = 4; // Texture saved to disk
const int DEFORM_SQUASH_A = 5; // Squash A
const int DEFORM_SQUASH_B = 6; // Squash B
const int DEFORM_SWIRL = 7;   // Swril 
const int SAVE_IMAGE_FILE = 10; // Save image file 
const int BMP_HEADER_LENGTH = 54; // BMP header length

// Initialization functions
void initState();
void initGLUT(int* argc, char** argv);
void initOpenGL();
void initTexture();

// Callback functions
void display();
void reshape(GLint width, GLint height);
void keyRelease(unsigned char key, int x, int y);
void keyDown(unsigned char key, int x, int y);
void mouseBtn(int button, int state, int x, int y);
void mouseMove(int x, int y);
void idle();
void menu(int cmd);
void cleanup();

// Other functions
void updateGeometry();		// Call after changing vertex data


int main(int argc, char** argv) {
	try {
		// Initialize
		initState();
		initGLUT(&argc, argv);
		initOpenGL();
		initTexture();

	} catch (const exception& e) {
		// Handle any errors
		cerr << "Fatal error: " << e.what() << endl;
		cleanup();
		return -1;
	}

	// Execute main loop
	glutMainLoop();

	return 0;
}

void initState() {
	// Initialize global state
	width = 0;
	height = 0;
	texWidthA = 0;
	texHeightA = 0;
	texWidthB = 0;
	texHeightB = 0;
	texWidth = 128;
	texHeight = 128;
	texDataA = NULL;
	texDataB = NULL;
	textureA = 0;
	textureB = 0;
	shader = 0;
	uniXform = 0;
	vao = 0;
	vbuf = 0;
	ibuf = 0;
	vcount = 0;
	clicking = false;
	texType = TEXTURE_A;
	deformType = DEFORM_SQUASH_A;
	widthRatio = 1.f;
	heightRatio = 1.f;
	rotAngle = 0.f;

	// Force images to load vertically flipped
	// OpenGL expects pixel data to start at the lower-left corner
	stbi_set_flip_vertically_on_load(1);

	// Initialize random number generator
	std::random_device rd;
	rng = std::mt19937(rd());
	noise = std::uniform_real_distribution<float>(-0.01, 0.01);
}

void initGLUT(int* argc, char** argv) {
	// Set window and context settings
	width = 800; height = 600;
	glutInit(argc, argv);
	glutInitWindowSize(width, height);
	glutInitContextVersion(3, 3);
	glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE);
	// Create the window
	glutCreateWindow("FreeGlut Window");

	// Create a menu
	glutCreateMenu(menu);
	glutAddMenuEntry("TextureA", TEXTURE_A);
	glutAddMenuEntry("TextureB", TEXTURE_B);
	glutAddMenuEntry("SquashA", DEFORM_SQUASH_A);
	glutAddMenuEntry("SquashB", DEFORM_SQUASH_B);
	glutAddMenuEntry("Swirl", DEFORM_SWIRL);
	glutAddMenuEntry("Save to image file", SAVE_IMAGE_FILE);
	glutAddMenuEntry("Reload saved image file", TEXTURE_SAVED);
	glutAddMenuEntry("Exit", MENU_EXIT);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

	// GLUT callbacks
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutKeyboardUpFunc(keyRelease);
	glutKeyboardFunc(keyDown);
	glutMouseFunc(mouseBtn);
	glutMotionFunc(mouseMove);
	glutIdleFunc(idle);
	glutCloseFunc(cleanup);
}

void initOpenGL() {
	// Set clear color and depth
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	// Enable depth testing
	glEnable(GL_DEPTH_TEST);
	// Allow unpacking non-aligned pixel data
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Compile and link shader program
	vector<GLuint> shaders;
	shaders.push_back(compileShader(GL_VERTEX_SHADER, "sh_v.glsl"));
	shaders.push_back(compileShader(GL_FRAGMENT_SHADER, "sh_f.glsl"));
	shader = linkProgram(shaders);
	// Release shader sources
	for (auto s = shaders.begin(); s != shaders.end(); ++s)
		glDeleteShader(*s);
	shaders.clear();
	// Locate uniforms
	uniXform = glGetUniformLocation(shader, "xform");
	GLuint uniTex = glGetUniformLocation(shader, "tex");

	// Bind texture image unit
	glUseProgram(shader);
	glUniform1i(uniTex, 0);
	glUseProgram(0);

	assert(glGetError() == GL_NO_ERROR);
}

void genGrid(int width, int height) {
	if (vertices.size() > 0) {
		vertices.clear();
	}
	if (indexs.size() > 0) {
		indexs.clear();
	}

	float du = 0, dv = 0;
	float offsetU = width / 2, offsetV = height / 2;
	if (width % 2 == 0) du = 0.5;
	if (height % 2 == 0) dv = 0.5;

	for (int v = 0; v < height; v++) {
		for (int u = 0; u < width; u++) {
			// Create vertex to draw grid
			int idx = u + v * width;
			vert v0 = {
				glm::vec3((float)(u - offsetU + du), (float)(v - offsetV + dv), 0.f),
				glm::vec2((float)u / (width - 1), (float)v / (height - 1))
			};
			vertices.push_back(v0);

			// Create element idx to draw grid
			if (u != width - 1 && v != height - 1) {
				// When not to the grid edge index
				// Horizontal line index
				indexs.push_back(idx);
				indexs.push_back(idx + 1);
				indexs.push_back(idx + width + 1);
				// Vertical line index
				indexs.push_back(idx);
				indexs.push_back(idx + width);
				indexs.push_back(idx + width + 1);
			}
		}
	}

	vcount = indexs.size();
	/*for (int i = 0; i < vertices.size(); i ++) {
		cout << vertices[i].pos.x << ", " << vertices[i].pos.y << endl;
		cout << vertices[i].tc.x << ", " << vertices[i].tc.y << endl;
	}*/
}

void createTexture(char* fileName, GLuint* textureId, GLint* texWidth, GLint* texHeight, unsigned char* texData) {
	// Load texture file
	int n_ch;
	texData = stbi_load(fileName, texWidth, texHeight, &n_ch, 3);
	if (!texData)
		throw std::runtime_error("failed to load image");
	
	// Create texture object
	glGenTextures(1, textureId);
	glBindTexture(GL_TEXTURE_2D, *textureId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, *texWidth, *texHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, texData);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void initTexture() {
	// Create a surface (quad) to draw the texture onto
	//verts = {
	//	{ glm::vec2(-1.0f, -1.0f), glm::vec2(0.0f, 0.0f) },
	//	{ glm::vec2( 1.0f, -1.0f), glm::vec2(1.0f, 0.0f) },
	//	{ glm::vec2( 1.0f,  1.0f), glm::vec2(1.0f, 1.0f) },
	//	{ glm::vec2(-1.0f,  1.0f), glm::vec2(0.0f, 1.0f) },
	//};
	//// Vertex indices for triangles
	//ids = {
	//	0, 1, 2,	// Triangle 1
	//	2, 3, 0		// Triangle 2
	//};
	//vcount = ids.size();
	 
	// Create Grid vertices
	genGrid(texWidth, texHeight);

	// Create vertex array object
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// Create vertex buffer
	glGenBuffers(1, &vbuf);
	glBindBuffer(GL_ARRAY_BUFFER, vbuf);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vert), vertices.data(), GL_DYNAMIC_DRAW);
	// Specify vertex attributes
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vert), 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vert), (GLvoid*)sizeof(glm::vec3));
	// Create index buffer
	glGenBuffers(1, &ibuf);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibuf);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexs.size() * sizeof(GLuint), indexs.data(), GL_DYNAMIC_DRAW);

	// Cleanup state
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// Create texture object A
	createTexture("textureA.png", &textureA, &texWidthA, &texHeightA, texDataA);

	// Create texture Object B
	createTexture("textureB.jpeg", &textureB, &texWidthB, &texHeightB, texDataB);

	assert(glGetError() == GL_NO_ERROR);
}

void display() {
	try {
		// Clear the back buffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Get ready to draw
		glUseProgram(shader);

		// Fix aspect ratio
		glm::mat4 xform(1.0f);
		float winAspect = (float)width / (float)height;
		float texAspect = (float)texWidth / (float)texHeight;

		widthRatio = glm::min(1.0f, texAspect / winAspect);
		heightRatio = glm::min(1.0f, winAspect / texAspect);
		xform[0][0] = widthRatio;
		xform[1][1] = heightRatio;
		// Create scale matrix
		mat4 scal = scale(mat4(1.f), vec3(2.f/(float)texWidth, 2.f/(float)texHeight, 0.f));
		
		mat4 rot = rotate(mat4(1.f), radians(rotAngle), vec3(0.f, 1.f, 0.f));

		xform = rot * scal * xform;
		// Send transformation matrix to shader
		glUniformMatrix4fv(uniXform, 1, GL_FALSE, value_ptr(xform));

		// Draw the textured quad
		glBindVertexArray(vao);
		glActiveTexture(GL_TEXTURE0 + 0);
		switch (texType) {
		// 2: textureA
		case TEXTURE_A:
			glBindTexture(GL_TEXTURE_2D, textureA);
			break;

		// 3: textureB
		case TEXTURE_B:
			glBindTexture(GL_TEXTURE_2D, textureB);
			break;

		// 4: textureSaved
		case TEXTURE_SAVED:
			glBindTexture(GL_TEXTURE_2D, textureSaved);
			break;
		}
		glDrawElements(GL_TRIANGLES, vcount, GL_UNSIGNED_INT, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindVertexArray(0);

		// Revert context state
		glUseProgram(0);

		// Display the back buffer
		glutSwapBuffers();

	} catch (const exception& e) {
		cerr << "Fatal error: " << e.what() << endl;
		glutLeaveMainLoop();
	}
}

void reshape(GLint width, GLint height) {
	::width = width;
	::height = height;
	glViewport(0, 0, width, height);
}

void keyRelease(unsigned char key, int x, int y) {
	switch (key) {
	case 27:	// Escape key
		menu(MENU_EXIT);
		break;
	}
}

// Convert a position in screen space into texture space
glm::vec3 mouseToWorldCoord(int x, int y) {
	glm::vec3 mousePos(x, y, 1.0f);

	// Convert screen coordinates into clip space (NDC space)
	glm::mat3 screenToClip(1.0f);
	screenToClip[0][0] = 2.0f / width;
	screenToClip[1][1] = -2.0f / height;	// Flip y coordinate
	screenToClip[2][0] = -1.0f;
	screenToClip[2][1] = 1.0f;

	vec2 NDCPos2 = screenToClip * mousePos;
	//cout << NDCPos2.x << " == " << NDCPos2.y << endl;
	vec4 NDCPos4 = vec4(NDCPos2, 0.f, 0.f);

	// Invert the aspect ratio correction (from display())
	float winAspect = (float)width / (float)height;
	float texAspect = (float)texWidth / (float)texHeight;
	glm::mat4 invAspect(1.0f);
	invAspect[0][0] = glm::max(1.0f, winAspect / texAspect);
	invAspect[1][1] = glm::max(1.0f, texAspect / winAspect);
	
	// From NDC to clip space
	//mat4 proj = perspective(45.0f, winAspect, 0.1f, 100.0f);
	//mat4 view = translate(mat4(1.0f), vec3(0, 0, -28.f));
	//mat4 invView = inverse(view);
	//mat4 invProj = inverse(proj);

	// Get texture coordinate that was clicked on
	//vec3 clipPos = vec3(invView * invProj * NDCPos4);
	vec3 clipPos = invAspect * NDCPos4;
	vec3 worldPos = vec3(clipPos.x * texWidth * 0.5f, clipPos.y * texHeight * 0.5f, clipPos.z);
	//cout << worldPos.x << " ++ " << worldPos.y << endl;
	return worldPos;
}

void updateVertices(vec3 cj) {
	if (deformType == DEFORM_SQUASH_A) {
		float alpha = 0.6f;
		for (int i = 0; i < vertices.size(); i++) {
			vec3 vi = vertices[i].pos;
			if (distance(vi, cj) > 0.5f) {
				// Calculate distance
				float dist = alpha / (pow(vi.x - cj.x, 2) + pow(vi.y - cj.y, 2));
				vec3 dir = vi - cj;
				vertices[i].pos = vi + dist * dir;
			}
		}
	}

	if (deformType == DEFORM_SQUASH_B) {
		float alpha = 0.6f;
		for (int i = 0; i < vertices.size(); i++) {
			vec3 vi = vertices[i].pos;
			if (distance(vi, cj) > 0.5f) {
				// Calculate distance
				float dist = alpha / (pow(vi.x - cj.x, 2) + pow(vi.y - cj.y, 2));
				vec3 dir = cj - vi;
				vec3 updateVi = vi + dist * dir;
				if (distance(updateVi, cj) > 1.0f) {
					vertices[i].pos = updateVi;
				}
			}
		}
	}

	if (deformType == DEFORM_SWIRL) {
		float effectRadius = 10.f;
		float effectAngle = 2 * pi<float>();
		for (int i = 0; i < vertices.size(); i++) {
			vec3 vi = vertices[i].pos;
			vec2 toCenterXY = vi - cj;
			float len = length(toCenterXY * vec2(texWidth / texHeight, 1.f));
			//float z = -10.0f;
			float angle = atan(toCenterXY.y, toCenterXY.x) + effectAngle * smoothstep(effectRadius, 0.f, len);
			float radius = length(toCenterXY);
			vec3 updateVi = cj + vec3(radius * cos(angle), radius * sin(angle), 0.f);
			/*if (distance(vi, updateVi) < 0.1f) {
				z = 0.f;
			}*/
			vertices[i].pos = updateVi;
		}
	}
	
}

void save2ImageFile() {
	FILE* sampleFile;
	FILE* outputFile;
	GLubyte BMPHeader[BMP_HEADER_LENGTH];
	int w = (int)(width * widthRatio);
	int h = (int)(height * heightRatio);
	int dataLens = w * h * 3; // BGR

	//glutSwapBuffers();
	glReadBuffer(GL_FRONT);
	GLubyte* dataset = (GLubyte*)malloc(dataLens); 

	if (dataset == 0) {
		cout << "read pixels error" << endl;
		return;
	}
	sampleFile = fopen("sample.bmp", "rb");
	if (sampleFile == 0) {
		cout << "read sample bump file error" << endl;
		return;
	}
	outputFile = fopen("output.bmp", "wb");
	if (outputFile == 0) {
		cout << "write output file error" << endl;
		return;
	}

	// Read pixels
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	GLint offsetX = 0.5 * (width - w);
	GLint offsetY = 0.5 * (height - h);
	/*GLint offsetY = 0.5 * (height - texHeight);*/
	glReadPixels(offsetX, offsetY, w, h, GL_BGR, GL_UNSIGNED_BYTE, dataset);
	
	// Write the header file
	fread(BMPHeader, sizeof(BMPHeader), 1, sampleFile);
	fwrite(BMPHeader, sizeof(BMPHeader), 1, outputFile);
	fseek(outputFile, 0x0012, SEEK_SET);
	fwrite(&height, sizeof(height), 1, outputFile);
	fwrite(&height, sizeof(height), 1, outputFile);

	// Write pixel data
	fseek(outputFile, 0, SEEK_END);
	fwrite(dataset, dataLens, 1, outputFile);

	// Release memory and close the file
	fclose(sampleFile);
	fclose(outputFile);
	free(dataset);
	
}

void mouseBtn(int button, int state, int x, int y) {
	if (button == GLUT_LEFT && state == GLUT_DOWN) {
		vec3 worldPos = mouseToWorldCoord(x, y);
		float widthMax = 0.5 * (float)texWidth - 0.5;
		float heightMax = 0.5 * (float)texHeight - 0.5;
		if (worldPos.x > -widthMax && worldPos.x < widthMax && worldPos.y > -heightMax && worldPos.y < heightMax) {
			clicking = true;
			updateVertices(worldPos);
		}
	}
	else if (button == GLUT_LEFT && state == GLUT_UP) {
		clicking = false;
	}
}

void mouseMove(int x, int y) {}

void keyDown(unsigned char key, int x, int y) {
	if (key == 'A' || key == 'a') {
		rotAngle -= 10;
		if (rotAngle < -180) {
			rotAngle += 360;
		}
		glutPostRedisplay();
	}
	if (key == 'D' || key == 'd') {
		rotAngle += 10;
		if (rotAngle > 180) {
			rotAngle -= 360;
		}
		glutPostRedisplay();
	}
}

void idle() {
	if (clicking) {
		updateGeometry();
		glutPostRedisplay();
	}
}

void menu(int cmd) {
	switch (cmd) {
	case TEXTURE_A:
		texType = TEXTURE_A;
		genGrid(texWidth, texHeight);
		updateGeometry();
		glutPostRedisplay();
		break;

	case TEXTURE_B:
		texType = TEXTURE_B;
		genGrid(texWidth, texHeight);
		updateGeometry();
		glutPostRedisplay();
		break;

	case DEFORM_SQUASH_A:
		deformType = DEFORM_SQUASH_A;
		genGrid(texWidth, texHeight);
		updateGeometry();
		glutPostRedisplay();
		break;

	case DEFORM_SQUASH_B:
		deformType = DEFORM_SQUASH_B;
		genGrid(texWidth, texHeight);
		updateGeometry();
		glutPostRedisplay();
		break;

	case DEFORM_SWIRL:
		deformType = DEFORM_SWIRL;
		genGrid(texWidth, texHeight);
		updateGeometry();
		glutPostRedisplay();
		break;

	case SAVE_IMAGE_FILE:
		save2ImageFile();
		break;

	case TEXTURE_SAVED:
		texType = TEXTURE_SAVED;
		// Create save texture object
		createTexture("output.bmp", &textureSaved, &texWidthSaved, &texHeightSaved, texDataSaved);
		genGrid(texWidth, texHeight);
		updateGeometry();
		glutPostRedisplay();
		break;

	case MENU_EXIT:
		glutLeaveMainLoop();
		break;
	}
}

void cleanup() {
	// Release all resources
	if (textureA) { glDeleteTextures(1, &textureA); textureA = 0; }
	if (textureB) { glDeleteTextures(1, &textureB); textureB = 0; }
	if (shader) { glDeleteProgram(shader); shader = 0; }
	uniXform = 0;
	if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
	if (vbuf) { glDeleteBuffers(1, &vbuf); vbuf = 0; }
	if (ibuf) { glDeleteBuffers(1, &ibuf); ibuf = 0; }
	vcount = 0;
	if (texDataA) { stbi_image_free(texDataA); texDataA = NULL; }
	if (texDataB) { stbi_image_free(texDataB); texDataB = NULL; }
}


// Re-uploads geometry to OpenGL -- call after changing vertex or index data
void updateGeometry() {
	// Upload vertex array
	glBindBuffer(GL_ARRAY_BUFFER, vbuf);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vert), vertices.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Upload index array
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibuf);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexs.size() * sizeof(GLuint), indexs.data(), GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
