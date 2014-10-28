#include "Angel.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Open Asset Importer header files (in ../../assimp--3.0.1270/include)
// This is a standard open source library for loading meshes, see gnatidread.h.
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

GLint windowWidth = 960, windowHeight = 640;

// gnatidread.cpp is the CITS3003 "Graphics 'n Animation Tool Interface & Data Reader" code
// This file contains parts of the code that you shouldn't need to modify (but you can).
#include "gnatidread.h"
#include "gnatidread2.h"

using namespace std;  // Import the C++ standard functions (e.g. min)

// IDs for the GLSL program and variables:
GLuint shaderProgram;  // The number identifying the GLSL shader program.
GLuint vPosition, vNormal, vTexCoord;  // IDs for input variables (from glGetAttribLocation)
GLuint vBoneIDs, vBoneWeights;
GLuint projectionU, modelViewU;  // IDs for uniform variables (from glGetUniformLocation)
GLuint boneTransformsU;

static float viewDist = 7.5;  // Distance from the camera to the centre of the scene.
static float camRotSidewaysDeg = 0.0;  // Rotates the camera sideways around the centre.
static float camRotUpAndOverDeg = 20.0;  // Rotates the camera up and over the centre.

mat4 projection;  // Projection matrix (set in the reshape function)
mat4 view;  // View matrix (set in the display function)

// These are used to set the window title:
char lab[] = "Project 1";
char *programName = NULL;  // Set in main.
int numDisplayCalls = 0;  // Used to calculate the number of frames per second.

// ---- [Meshes] ---------------------------------------------------------------
// Uses the type aiMesh from ../../assimp--3.0.1270/include/assimp/mesh.h
//     (numMeshes is defined in gnatidread.h)
aiMesh *meshes[numMeshes];  // For each mesh we have a pointer to the mesh to draw
GLuint vaoIDs[numMeshes];  // and a corresponding VAO ID from glGenVertexArrays.
const aiScene *scenes[numMeshes];

// ---- [Textures] -------------------------------------------------------------
//     (numTextures is defined in gnatidread.h)
texture *textures[numTextures];  // An array of texture pointers (see gnatidread.h)
GLuint textureIDs[numTextures];  // Stores the IDs returned by glGenTextures.

// ---- [Scene objects] --------------------------------------------------------

// For each object in a scene we store the following...
// Note: the following is exactly what the sample solution uses, you can do things differently if you want.
typedef struct {
	vec4 loc;
	float scale;
	float angles[3];  // Rotations around X, Y and Z axes.
	float diffuse, specular, ambient;  // Amount of each light component.
	float shine;
	vec3 rgb;
	float brightness;  // Multiplies all colours.
	int meshId;
	int texId;
	float texScale;
} SceneObject;

const int maxObjects = 1024;  // Scenes with more than 1024 objects seem unlikely.
const int numSaves = 30;
const int saveHeader = ('S' | 'A' << 8 | 'V' << 16 | 'E' << 24);

SceneObject sceneObjs[maxObjects];  // An array storing the objects currently in the scene.
int nObjects = 0;  // How many objects are currently in the scene.
int currObject = -1;  // The current object.
int toolObj = -1;  // The object currently being modified.

// ---- [Texture loading] ------------------------------------------------------

// Loads a texture by number, and binds it for later use.
void loadTextureIfNotAlreadyLoaded(int texNum) {
	if (textures[texNum] != NULL) return;  // Already loaded

	textures[texNum] = loadTextureNum(texNum);
	glActiveTexture(GL_TEXTURE0); CheckError();

	// Based on: http://www.opengl.org/wiki/Common_Mistakes
	glBindTexture(GL_TEXTURE_2D, textureIDs[texNum]); CheckError();

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textures[texNum]->width, textures[texNum]->height,
			0, GL_RGB, GL_UNSIGNED_BYTE, textures[texNum]->rgbData); CheckError();
	glGenerateMipmap(GL_TEXTURE_2D); CheckError();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); CheckError();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); CheckError();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); CheckError();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); CheckError();

	glBindTexture(GL_TEXTURE_2D, 0); CheckError();  // Back to default texture
}

// ---- [Mesh loading] ---------------------------------------------------------

// The following uses the Open Asset Importer library via loadMesh in
// gnatidread.h to load models in .x format, including vertex positions,
// normals and texture coordinates.
// You shouldn't need to modify this - it's called from drawMesh below.
void loadMeshIfNotAlreadyLoaded(int meshNum) {
	if (meshNum < 0 || meshNum >= numMeshes) {
		failInt("Error - no such model number:", meshNum);
	}

	if (meshes[meshNum] != NULL) return;  // Already loaded

	const aiScene *scene = loadScene(meshNum);
	scenes[meshNum] = scene;
	aiMesh *mesh = scene->mMeshes[0];
	meshes[meshNum] = mesh;

	glBindVertexArray(vaoIDs[meshNum]); CheckError();

	// Create and initialize a buffer object for positions and texture coordinates, initially empty.
	// mesh->mTextureCoords[0] has space for up to 3 dimensions, but we only need 2.
	GLuint buffer;
	glGenBuffers(1, &buffer); CheckError();
	glBindBuffer(GL_ARRAY_BUFFER, buffer); CheckError();
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * (3+3+3) * mesh->mNumVertices,
			NULL, GL_STATIC_DRAW); CheckError();

	// Next, we load the position and texCoord data in parts.
	int nVerts = mesh->mNumVertices;
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 3 * nVerts, mesh->mVertices); CheckError();
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * 3 * nVerts, sizeof(float) * 3 * nVerts, mesh->mTextureCoords[0]); CheckError();
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * 6 * nVerts, sizeof(float) * 3 * nVerts, mesh->mNormals); CheckError();

	// Load the element index data.
	GLuint elements[mesh->mNumFaces * 3];
	for (GLuint i = 0; i < mesh->mNumFaces; i++) {
		elements[i*3] = mesh->mFaces[i].mIndices[0];
		elements[i*3+1] = mesh->mFaces[i].mIndices[1];
		elements[i*3+2] = mesh->mFaces[i].mIndices[2];
	}

	GLuint elementBuffer;
	glGenBuffers(1, &elementBuffer); CheckError();
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer); CheckError();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * mesh->mNumFaces * 3, elements, GL_STATIC_DRAW); CheckError();

	// vPosition is actually 4D - the conversion sets the fourth dimension (i.e. w) to 1.0.
	glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0)); CheckError();
	glEnableVertexAttribArray(vPosition); CheckError();

	// vTexCoord is actually 2D - the third dimension is ignored (it's always 0.0).
	glVertexAttribPointer(vTexCoord, 3, GL_FLOAT, GL_FALSE, 0,
			BUFFER_OFFSET(sizeof(float) * 3 * nVerts)); CheckError();
	glEnableVertexAttribArray(vTexCoord); CheckError();
	glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 0,
			BUFFER_OFFSET(sizeof(float) * 6 * nVerts)); CheckError();
	glEnableVertexAttribArray(vNormal); CheckError();

	// Get boneIDs and boneWeights for each vertex from the imported mesh data.
	GLint boneIDs[nVerts][4];
	GLfloat boneWeights[nVerts][4];
	getBonesAffectingEachVertex(mesh, boneIDs, boneWeights);

	GLuint buffers[2];
	glGenBuffers(2, buffers); CheckError();  // Add two vertex buffer objects

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]); CheckError();
	glBufferData(GL_ARRAY_BUFFER, sizeof(int) * 4 * nVerts, boneIDs, GL_STATIC_DRAW); CheckError();
	glVertexAttribIPointer(vBoneIDs, 4, GL_INT, 0, BUFFER_OFFSET(0)); CheckError();
	glEnableVertexAttribArray(vBoneIDs); CheckError();

	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]); CheckError();
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * nVerts, boneWeights, GL_STATIC_DRAW); CheckError();
	glVertexAttribPointer(vBoneWeights, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0)); CheckError();
	glEnableVertexAttribArray(vBoneWeights); CheckError();
}

// -----------------------------------------------------------------------------

static void mouseClickOrScroll(int button, int state, int x, int y) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
		if (glutGetModifiers() != GLUT_ACTIVE_SHIFT) {
			activateTool(button);
		} else {
			activateTool(GLUT_MIDDLE_BUTTON);
		}
	} else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
		deactivateTool();
	} else if (button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN) {
		activateTool(button);
	} else if (button == GLUT_MIDDLE_BUTTON && state == GLUT_UP) {
		deactivateTool();
	} else if (button == 3) {
		// Scroll up
		viewDist = (viewDist < 0.0 ? viewDist : viewDist * 0.8) - 0.05;
	} else if (button == 4) {
		// Scroll down
		viewDist = (viewDist < 0.0 ? viewDist : viewDist * 1.25) + 0.05;
	}
}

static void mousePassiveMotion(int x, int y) {
	mouseX = x;
	mouseY = y;
}

mat2 camRotZ() {
	return rotZ(-camRotSidewaysDeg) * mat2(10.0, 0.0, 0.0, -10.0);
}

// Callback functions for doRotate below and later:
static void adjustCamSideViewDist(vec2 cv) {
	camRotSidewaysDeg += cv[0];
	viewDist += cv[1];
}

static void adjustCamSideUp(vec2 su) {
	camRotSidewaysDeg += su[0];
	camRotUpAndOverDeg += su[1];
}

static void adjustLocXZ(vec2 xz) {
	SceneObject *obj = &sceneObjs[toolObj];
	obj->loc[0] += xz[0];
	obj->loc[2] += xz[1];
}

static void adjustScaleY(vec2 sy) {
	SceneObject *obj = &sceneObjs[toolObj];
	obj->scale += sy[0];
	obj->loc[1] += sy[1];
}

// Set the mouse buttons to rotate the camera around the centre of the scene.
static void doRotate() {
	setToolCallbacks(adjustCamSideViewDist, mat2(400.0, 0.0, 0.0, -10.0),
			adjustCamSideUp, mat2(400.0, 0.0, 0.0, -90.0));
}

// Add an object to the scene.
static void addObject(int id) {
	if (nObjects == maxObjects) return;

	vec2 currPos = currMouseXYWorld(camRotSidewaysDeg);

	SceneObject *obj = &sceneObjs[nObjects];
	obj->loc[0] = currPos[0];
	obj->loc[1] = 0.0;
	obj->loc[2] = currPos[1];
	obj->loc[3] = 1.0;

	if (id != 0 && id != 55) {
		obj->scale = 0.005;
	}

	obj->rgb[0] = 0.7;
	obj->rgb[1] = 0.7;
	obj->rgb[2] = 0.7;
	obj->brightness = 1.0;

	obj->diffuse = 1.0;
	obj->specular = 0.5;
	obj->ambient = 0.7;
	obj->shine = 10.0;

	obj->angles[0] = 0.0;
	obj->angles[1] = 180.0;
	obj->angles[2] = 0.0;

	obj->meshId = id;
	obj->texId = 1 + (rand() % (numTextures - 1));
	obj->texScale = 2.0;

	toolObj = currObject = nObjects++;
	setToolCallbacks(adjustLocXZ, camRotZ(),
			adjustScaleY, mat2(0.05, 0.0, 0.0, 10.0));
	glutPostRedisplay();
}

// The init function.
void init(void) {
	srand(time(NULL));  // Initialize random seed (so the starting scene varies)
	aiInit();

	glGenVertexArrays(numMeshes, vaoIDs); CheckError();  // Allocate vertex array objects for meshes
	glGenTextures(numTextures, textureIDs); CheckError();  // Allocate texture objects

	// Load shaders and use the resulting shader program.
	shaderProgram = InitShader("vshader.glsl", "fshader.glsl");

	glUseProgram(shaderProgram); CheckError();

	// Initialize the vertex position attribute from the vertex shader.
	vPosition = glGetAttribLocation(shaderProgram, "vPosition"); CheckError();
	vNormal = glGetAttribLocation(shaderProgram, "vNormal"); CheckError();

	// Likewise, initialize the vertex texture coordinates attribute.
	vTexCoord = glGetAttribLocation(shaderProgram, "vTexCoord"); CheckError();

	vBoneIDs = glGetAttribLocation(shaderProgram, "boneIDs"); CheckError();
	vBoneWeights = glGetAttribLocation(shaderProgram, "boneWeights"); CheckError();

	projectionU = glGetUniformLocation(shaderProgram, "Projection"); CheckError();
	modelViewU = glGetUniformLocation(shaderProgram, "ModelView"); CheckError();
	boneTransformsU = glGetUniformLocation(shaderProgram, "boneTransforms"); CheckError();

	// Objects 0 and 1 are the ground and the first light.
	addObject(0);  // Square for the ground
	SceneObject *groundObj = &sceneObjs[0];
	groundObj->loc = vec4(0.0, 0.0, 0.0, 1.0);
	groundObj->scale = 10.0;
	groundObj->angles[0] = 90.0;  // Rotate it
	groundObj->texScale = 5.0; // Repeat the texture

	addObject(55);  // Sphere for the first light
	SceneObject *lightObj1 = &sceneObjs[1];
	lightObj1->loc = vec4(2.0, 1.0, 1.0, 1.0);
	lightObj1->scale = 0.1;
	lightObj1->texId = 0;  // Plain texture
	lightObj1->brightness = 0.2;  // The light's brightness is 5 times this (below)

	// [I] Add a second light to the scene.
	addObject(55);  // Sphere for the second light
	SceneObject *lightObj2 = &sceneObjs[2];
	lightObj2->loc = vec4(-2.0, 1.0, -1.0, 1.0);
	lightObj2->scale = 0.1;
	lightObj2->texId = 0;  // Plain texture
	lightObj2->brightness = 0.5;

	addObject(1 + (rand() % (numMeshes - 1)));  // A test mesh

	// We need to enable the depth test to discard fragments that
	// are behind previously drawn fragments for the same pixel.
	glEnable(GL_DEPTH_TEST);
	doRotate();  // Start in camera rotate mode
	glClearColor(0.0, 0.0, 0.0, 1.0);  // Black background
}

// -----------------------------------------------------------------------------

void drawMesh(SceneObject sceneObj) {
	loadTextureIfNotAlreadyLoaded(sceneObj.texId);
	loadMeshIfNotAlreadyLoaded(sceneObj.meshId);

	aiMesh *mesh = meshes[sceneObj.meshId];
	const aiScene *scene = scenes[sceneObj.meshId];

	float poseTime = 0.0f;
	if (sceneObj.meshId >= 56) {
		double framesPerSecond = 30.0;
		double elapsedTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0 * framesPerSecond;
		double animDuration = getAnimDuration(mesh, scene, 0);
		poseTime = fmod(elapsedTime, animDuration);
	}

	// Activate a texture.
	glActiveTexture(GL_TEXTURE0); CheckError();
	glBindTexture(GL_TEXTURE_2D, textureIDs[sceneObj.texId]); CheckError();

	// Texture 0 is the only texture type in this program, and is for the RGB colour of the
	// surface but there could be separate types, e.g. specularity and normals.
	glUniform1i(glGetUniformLocation(shaderProgram, "texture"), 0); CheckError();

	// Set the texture scale for the shaders.
	glUniform1f(glGetUniformLocation(shaderProgram, "texScale"), sceneObj.texScale); CheckError();

	// Set the projection matrix for the shaders.
	glUniformMatrix4fv(projectionU, 1, GL_TRUE, projection); CheckError();

	// [B] Set the model matrix.
	mat4 rot = RotateX(sceneObj.angles[0]) * RotateY(sceneObj.angles[1]) * RotateZ(sceneObj.angles[2]);
	mat4 model = Translate(sceneObj.loc) * rot * Scale(sceneObj.scale);

	// Set the model-view matrix for the shaders.
	glUniformMatrix4fv(modelViewU, 1, GL_TRUE, view * model); CheckError();

	// Activate the VAO for a mesh.
	glBindVertexArray(vaoIDs[sceneObj.meshId]); CheckError();

	int nBones = mesh->mNumBones;
	if (nBones == 0) nBones = 1;  // If no bones, just a single identity matrix is used

	// Get boneTransforms for the first (0th) animation at the given time (a float measured in frames).
	mat4 boneTransforms[nBones];
	calculateAnimPose(mesh, scene, 0, poseTime, boneTransforms);
	glUniformMatrix4fv(boneTransformsU, nBones, GL_TRUE, (const GLfloat*) boneTransforms);

	glDrawElements(GL_TRIANGLES, mesh->mNumFaces * 3, GL_UNSIGNED_INT, NULL); CheckError();
}

void display(void) {
	numDisplayCalls++;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); CheckError();

	// [A] Set the view matrix.
	mat4 rot = RotateX(camRotUpAndOverDeg) * RotateY(camRotSidewaysDeg);
	view = Translate(0.0, 0.0, -viewDist) * rot;

	SceneObject lightObj1 = sceneObjs[1];
	vec4 lightPosition1 = view * lightObj1.loc;
	SceneObject lightObj2 = sceneObjs[2];
	vec4 lightPosition2 = rot * lightObj2.loc;

	glUniform4fv(glGetUniformLocation(shaderProgram, "LightPosition1"), 1, lightPosition1); CheckError();
	glUniform4fv(glGetUniformLocation(shaderProgram, "LightPosition2"), 1, lightPosition2); CheckError();
	glUniform3fv(glGetUniformLocation(shaderProgram, "LightColor1"), 1, lightObj1.rgb); CheckError();
	glUniform3fv(glGetUniformLocation(shaderProgram, "LightColor2"), 1, lightObj2.rgb); CheckError();
	glUniform1f(glGetUniformLocation(shaderProgram, "LightBrightness1"), lightObj1.brightness); CheckError();
	glUniform1f(glGetUniformLocation(shaderProgram, "LightBrightness2"), lightObj2.brightness); CheckError();

	for (int i = 0; i < nObjects; i++) {
		SceneObject obj = sceneObjs[i];

		vec3 rgb = obj.rgb * obj.brightness * 2.0;
		glUniform3fv(glGetUniformLocation(shaderProgram, "AmbientProduct"), 1, obj.ambient * rgb); CheckError();
		glUniform3fv(glGetUniformLocation(shaderProgram, "DiffuseProduct"), 1, obj.diffuse * rgb); CheckError();
		glUniform3fv(glGetUniformLocation(shaderProgram, "SpecularProduct"), 1, obj.specular * rgb); CheckError();
		glUniform1f(glGetUniformLocation(shaderProgram, "Shininess"), obj.shine); CheckError();

		drawMesh(sceneObjs[i]);
	}

	glutSwapBuffers();
}

// ---- [Menus] ----------------------------------------------------------------

static void objectMenu(int id) {
	deactivateTool();
	addObject(id);
}

static void texMenu(int id) {
	deactivateTool();
	if (currObject >= 0) {
		sceneObjs[currObject].texId = id;
		glutPostRedisplay();
	}
}

static void groundMenu(int id) {
	deactivateTool();
	sceneObjs[0].texId = id;
	glutPostRedisplay();
}

static void adjustBrightnessY(vec2 by) {
	SceneObject *obj = &sceneObjs[toolObj];
	obj->brightness = max(0.0f, obj->brightness + by[0]);
	obj->loc[1] += by[1];
}

static void adjustRedGreen(vec2 rg) {
	SceneObject *obj = &sceneObjs[toolObj];
	obj->rgb[0] = max(0.0f, obj->rgb[0] + rg[0]);
	obj->rgb[1] = max(0.0f, obj->rgb[1] + rg[1]);
}

static void adjustBlueBrightness(vec2 bl_br) {
	SceneObject *obj = &sceneObjs[toolObj];
	obj->rgb[2] = max(0.0f, obj->rgb[2] + bl_br[0]);
	obj->brightness = max(0.0f, obj->brightness + bl_br[1]);
}

static void lightMenu(int id) {
	deactivateTool();
	if (id == 70) {
		toolObj = 1;
		setToolCallbacks(adjustLocXZ, camRotZ(),
				adjustBrightnessY, mat2(1.0, 0.0, 0.0, 10.0));
	} else if (id == 71) {
		toolObj = 1;
		setToolCallbacks(adjustRedGreen, mat2(1.0, 0.0, 0.0, 1.0),
				adjustBlueBrightness, mat2(1.0, 0.0, 0.0, 1.0));
	} else if (id == 80) {
		toolObj = 2;
		setToolCallbacks(adjustLocXZ, camRotZ(),
				adjustBrightnessY, mat2(1.0, 0.0, 0.0, 10.0));
	} else if (id == 81) {
		toolObj = 2;
		setToolCallbacks(adjustRedGreen, mat2(1.0, 0.0, 0.0, 1.0),
				adjustBlueBrightness, mat2(1.0, 0.0, 0.0, 1.0));
	} else {
		printf("Error in lightMenu\n");
		exit(EXIT_FAILURE);
	}
}

static int createArrayMenu(int size, const char menuEntries[][128], void(*menuFn)(int)) {
	int nSubMenus = (int) ceil(size / 10.0);
	int subMenus[nSubMenus];

	for (int i = 0; i < nSubMenus; i++) {
		subMenus[i] = glutCreateMenu(menuFn);
		for (int j = i*10+1; j <= min(i*10+10, size); j++) {
	 		glutAddMenuEntry(menuEntries[j-1], j);
	 	}
	}
	int menuId = glutCreateMenu(menuFn);

	for (int i = 0; i < nSubMenus; i++) {
		char num[6];
		sprintf(num, "%d-%d", i*10+1, min(i*10+10, size));
		glutAddSubMenu(num, subMenus[i]);
	}
	return menuId;
}

static void adjustAmbientDiffuse(vec2 ad) {
	SceneObject *obj = &sceneObjs[toolObj];
	obj->ambient = max(0.0f, obj->ambient + ad[0]);
	obj->diffuse = max(0.0f, obj->diffuse + ad[1]);
}

static void adjustSpecularShine(vec2 ss) {
	SceneObject *obj = &sceneObjs[toolObj];
	obj->specular = max(0.0f, obj->specular + ss[0]);
	obj->shine = max(0.0f, obj->shine + ss[1]);
}

static void materialMenu(int id) {
	deactivateTool();
	if (currObject >= 0) {
		if (id == 10) {
			toolObj = currObject;
			setToolCallbacks(adjustRedGreen, mat2(1.0, 0.0, 0.0, 1.0),
					adjustBlueBrightness, mat2(1.0, 0.0, 0.0, 1.0));
		} else if (id == 20) {
			// [C] Adjust ambient/diffuse/specular lighting and shininess.
			toolObj = currObject;
			setToolCallbacks(adjustAmbientDiffuse, mat2(1.0, 0.0, 0.0, 1.0),
					adjustSpecularShine, mat2(1.0, 0.0, 0.0, 10.0));
		} else {
			printf("Error in materialMenu\n");
			exit(EXIT_FAILURE);
		}
	}
}

static void adjustAngleYX(vec2 angle_yx) {
	SceneObject *obj = &sceneObjs[currObject];
	obj->angles[1] += angle_yx[0];
	obj->angles[0] += angle_yx[1];
}

static void adjustAngleZTexScale(vec2 az_ts) {
	SceneObject *obj = &sceneObjs[currObject];
	obj->angles[2] += az_ts[0];
	obj->texScale += az_ts[1];
}

static void duplicateObject(int id) {
	if (nObjects == maxObjects) return;

	sceneObjs[nObjects] = sceneObjs[id];
	toolObj = currObject = nObjects++;
	setToolCallbacks(adjustLocXZ, camRotZ(),
			adjustScaleY, mat2(0.05, 0.0, 0.0, 10.0));
	glutPostRedisplay();
}

static void deleteObject(int id) {
	if (nObjects == 3) return;

	nObjects--;
	currObject = (nObjects > 3 ? nObjects - 1 : -1);
	toolObj = -1;
	doRotate();
	glutPostRedisplay();
}

static void mainMenu(int id) {
	deactivateTool();
	if (id == 41 && currObject >= 0) {
		toolObj = currObject;
		setToolCallbacks(adjustLocXZ, camRotZ(),
				adjustScaleY, mat2(0.05, 0.0, 0.0, 10.0));
	} else if (id == 50) {
		doRotate();
	} else if (id == 55 && currObject >= 0) {
		setToolCallbacks(adjustAngleYX, mat2(400.0, 0.0, 0.0, 400.0),
				adjustAngleZTexScale, mat2(-400.0, 0.0, 0.0, 15.0));
	} else if (id == 90 && currObject >= 0) {
		duplicateObject(currObject);
	} else if (id == 91 && currObject >= 0) {
		deleteObject(currObject);
	} else if (id == 99) {
		exit(EXIT_SUCCESS);
	}
}

static void loadMenu(int id) {
	char fileName[256];
	sprintf(fileName, "slot%d.sav", id);

	FILE *file = fopen(fileName, "rb");
	if (file == NULL) {
		fprintf(stderr, "Error: Could not open '%s' for reading\n", fileName);
		return;
	}

	int header;
	fread(&header, sizeof(int), 1, file);
	if (header != saveHeader) {
		fprintf(stderr, "Error: Invalid save file header");
		fclose(file);
		return;
	}
	fread(&viewDist, sizeof(float), 1, file);
	fread(&camRotSidewaysDeg, sizeof(float), 1, file);
	fread(&camRotUpAndOverDeg, sizeof(float), 1, file);
	fread(&nObjects, sizeof(int), 1, file);
	memset(sceneObjs, 0, sizeof(SceneObject) * nObjects);
	fread(sceneObjs, sizeof(SceneObject), nObjects, file);

	currObject = nObjects - 1;
	toolObj = -1;
	doRotate();

	fclose(file);
}

static void saveMenu(int id) {
	char fileName[256];
	sprintf(fileName, "slot%d.sav", id);

	FILE *file = fopen(fileName, "wb");
	if (file == NULL) {
		fprintf(stderr, "Error: Could not open '%s' for writing\n", fileName);
		return;
	}

	fwrite(&saveHeader, sizeof(int), 1, file);
	fwrite(&viewDist, sizeof(float), 1, file);
	fwrite(&camRotSidewaysDeg, sizeof(float), 1, file);
	fwrite(&camRotUpAndOverDeg, sizeof(float), 1, file);
	fwrite(&nObjects, sizeof(int), 1, file);
	fwrite(sceneObjs, sizeof(SceneObject), nObjects, file);

	fflush(file);
	fclose(file);
}

static void makeMenu() {
	int objectId = createArrayMenu(numMeshes - 1, objectMenuEntries, objectMenu);

	int materialMenuId = glutCreateMenu(materialMenu);
	glutAddMenuEntry("R/G/B/All", 10);
	glutAddMenuEntry("Ambient/Diffuse/Specular/Shine", 20);

	int texMenuId = createArrayMenu(numTextures - 1, textureMenuEntries, texMenu);
	int groundMenuId = createArrayMenu(numTextures - 1, textureMenuEntries, groundMenu);

	int lightMenuId = glutCreateMenu(lightMenu);
	glutAddMenuEntry("Move Light 1", 70);
	glutAddMenuEntry("R/G/B/All Light 1", 71);
	glutAddMenuEntry("Move Light 2", 80);
	glutAddMenuEntry("R/G/B/All Light 2", 81);

	char saveMenuEntries[numSaves][128];
	for (int i = 0; i < numSaves; i++) {
		sprintf(saveMenuEntries[i], "Slot %d", i+1);
	}
	int loadMenuId = createArrayMenu(numSaves, saveMenuEntries, loadMenu);
	int saveMenuId = createArrayMenu(numSaves, saveMenuEntries, saveMenu);

	glutCreateMenu(mainMenu);
	glutAddMenuEntry("Rotate/Move Camera", 50);
	glutAddSubMenu("Add object", objectId);
	glutAddMenuEntry("Position/Scale", 41);
	glutAddMenuEntry("Rotation/Texture Scale", 55);
	glutAddSubMenu("Material", materialMenuId);
	glutAddSubMenu("Texture", texMenuId);
	glutAddSubMenu("Ground Texture", groundMenuId);
	glutAddSubMenu("Lights", lightMenuId);
	glutAddMenuEntry("Duplicate object", 90);
	glutAddMenuEntry("Delete object", 91);
	glutAddSubMenu("Load scene", loadMenuId);
	glutAddSubMenu("Save scene", saveMenuId);
	glutAddMenuEntry("EXIT", 99);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

// -----------------------------------------------------------------------------

void keyboard(unsigned char key, int x, int y) {
	switch (key) {
		case 0x1B:
			exit(EXIT_SUCCESS);
			break;
	}
}

// -----------------------------------------------------------------------------

void idle(void) {
	glutPostRedisplay();
}

void reshape(int width, int height) {
	windowWidth = width;
	windowHeight = height;

	glViewport(0, 0, width, height); CheckError();

	GLfloat nearDist = 0.04;
	GLfloat left, right, bottom, top;

	// [D] Include "closer" visible objects within the view.
	// [E] When the width is less than the height, adjust the view so that the
	//     same part of the scene is visible across the width of the window.
	if (width < height) {
		left = -nearDist;
		right = nearDist;
		bottom = -nearDist * (float) height / (float) width;
		top = nearDist * (float) height / (float) width;
	} else {
		left = -nearDist * (float) width / (float) height;
		right = nearDist * (float) width / (float) height;
		bottom = -nearDist;
		top = nearDist;
	}

	projection = Frustum(left, right, bottom, top, 0.2, 500.0);
}

void timer(int unused) {
	char title[256];
	sprintf(title, "%s %s: %d frames per second @ %d x %d", lab, programName, numDisplayCalls, windowWidth, windowHeight);

	glutSetWindowTitle(title);

	numDisplayCalls = 0;
	glutTimerFunc(1000, timer, 0);
}

char dirDefault1[] = "models-textures";
char dirDefault2[] = "/c/temp/models-textures";
char dirDefault3[] = "/tmp/models-textures";
char dirDefault4[] = "/cslinux/examples/CITS3003/project-files/models-textures";

void fileErr(char *fileName) {
	printf("Error reading file: %s\n\n", fileName);
	printf("Download and unzip the models-textures folder - either:\n");
	printf("a) as a subfolder here (on your machine)\n");
	printf("b) at C:\\temp\\models-textures (labs Windows)\n");
	printf("c) or /tmp/models-textures (labs Linux).\n\n");
	printf("Alternatively put the path to the models-textures folder on the command line.\n");

	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
	// Get the program name (excluding the directory) for the window title.
	programName = argv[0];
	for (char *p = argv[0]; *p != '\0'; p++) {
		if (*p == '/' || *p == '\\') programName = p+1;
	}

	// Set the models-textures directory, via the first argument or some handy defaults.
	if (argc > 1) {
		strcpy(dataDir, argv[1]);
	} else if (opendir(dirDefault1)) {
		strcpy(dataDir, dirDefault1);
	} else if (opendir(dirDefault2)) {
		strcpy(dataDir, dirDefault2);
	} else if (opendir(dirDefault3)) {
		strcpy(dataDir, dirDefault3);
	} else if (opendir(dirDefault4)) {
		strcpy(dataDir, dirDefault4);
	} else {
		fileErr(dirDefault1);
	}

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(windowWidth, windowHeight);

	glutInitContextVersion(3, 2);
	//glutInitContextProfile(GLUT_CORE_PROFILE);
	glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);

	glutCreateWindow("Initialising...");

	glewExperimental = GL_TRUE;
	glewInit(); CheckError();

	init();

	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutIdleFunc(idle);

	glutMouseFunc(mouseClickOrScroll);
	glutPassiveMotionFunc(mousePassiveMotion);
	glutMotionFunc(doToolUpdateXY);

	glutReshapeFunc(reshape);
	glutTimerFunc(1000, timer, 0);

	makeMenu();

	glutMainLoop();
	return 0;
}
