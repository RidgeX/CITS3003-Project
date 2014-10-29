// UWA CITS3003
// Graphics 'n Animation Tool Interface & Data Reader (gnatidread.h)

// You shouldn't need to modify the code in this file, but feel free to.
// If you do, it would be good to mark your changes with comments.

#include "bitmap.h"

char dataDir[256];  // Stores the path to the models-textures folder.
const int numMeshes = 59;
const int numTextures = 31;

// ---- [Functions to fail with an error message then a string or int] ---------

void fail(const char *msg1, char *msg2) {
	fprintf(stderr, "%s %s\n", msg1, msg2);
	exit(EXIT_FAILURE);
}

void failInt(const char *msg, int i) {
	fprintf(stderr, "%s %d\n", msg, i);
	exit(EXIT_FAILURE);
}

// ---- [Texture data reading] -------------------------------------------------

// A type for a 2D texture, with width and height in pixels.
typedef struct {
	int width;
	int height;
	GLubyte *rgbData;  // Array of bytes with the colour data for the texture.
} texture;

// Load a texture via Michael Sweet's bitmap.c.
texture* loadTexture(char *fileName) {
	texture *t = (texture*) malloc(sizeof(texture));
	BITMAPINFO *info;

	t->rgbData = LoadDIBitmap(fileName, &info);
	if (t->rgbData == NULL) {
		fail("Error loading image:", fileName);
	}

	t->width = info->bmiHeader.biWidth;
	t->height = info->bmiHeader.biHeight;

	printf("\nLoaded a %d by %d texture.\n\n", t->width, t->height);

	return t;
}

// Load the texture with number texNum from the models-textures directory.
texture* loadTextureNum(int texNum) {
	if (texNum < 0 || texNum >= numTextures) {
		failInt("Error in loading texture - wrong texture number:", texNum);
	}

	char fileName[256];
	sprintf(fileName, "%s/texture%d.bmp", dataDir, texNum);
	return loadTexture(fileName);
}

// -----------------------------------------------------------------------------

// Initialise the Open Asset Importer toolkit.
void aiInit() {
	struct aiLogStream stream;

	// Get a handle to the predefined STDOUT log stream and attach
	// it to the logging system. It remains active for all further
	// calls to aiImportFile(Ex) and aiApplyPostProcessing.
	stream = aiGetPredefinedLogStream(aiDefaultLogStream_STDOUT, NULL);
	aiAttachLogStream(&stream);

	// ...same procedure, but this stream now writes the
	// log messages to assimp_log.txt.
	stream = aiGetPredefinedLogStream(aiDefaultLogStream_FILE, "assimp_log.txt");
	aiAttachLogStream(&stream);
}

// Load a mesh by number from the models-textures directory via the Open Asset Importer.
aiMesh* loadMesh(int meshNum) {
	char fileName[256];
	sprintf(fileName, "%s/model%d.x", dataDir, meshNum);
	const aiScene *scene = aiImportFile(fileName, aiProcessPreset_TargetRealtime_Quality | aiProcess_ConvertToLeftHanded);
	return scene->mMeshes[0];
}

// ---- [Strings for the texture and mesh menus] -------------------------------

char textureMenuEntries[numTextures][128] = {
	"1 Plain", "2 Rust", "3 Concrete", "4 Carpet", "5 Beach Sand",
	"6 Rocky", "7 Brick", "8 Water", "9 Paper", "10 Marble",
	"11 Wood", "12 Scales", "13 Fur", "14 Denim", "15 Hessian",
	"16 Orange Peel", "17 Ice Crystals", "18 Grass", "19 Corrugated Iron", "20 Styrofoam",
	"21 Bubble Wrap", "22 Leather", "23 Camouflage", "24 Asphalt", "25 Scratched Ice",
	"26 Rattan", "27 Snow", "28 Dry Mud", "29 Old Concrete", "30 Leopard Skin"
};

char objectMenuEntries[numMeshes][128] = {
	"1 Thin Dinosaur", "2 Big Dog", "3 Saddle Dinosaur", "4 Dragon", "5 Cleopatra",
	"6 Bone I", "7 Bone II", "8 Rabbit", "9 Long Dragon", "10 Buddha",
	"11 Sitting Rabbit", "12 Frog", "13 Cow", "14 Monster", "15 Sea Horse",
	"16 Head", "17 Pelican", "18 Horse", "19 Kneeling Angel", "20 Porsche I",
	"21 Truck", "22 Statue of Liberty", "23 Sitting Angel", "24 Metal Part", "25 Car",
	"26 Apatosaurus", "27 Airliner", "28 Motorbike", "29 Dolphin", "30 Spaceman",
	"31 Winnie the Pooh", "32 Shark", "33 Crocodile", "34 Toddler", "35 Fat Dinosaur",
	"36 Chihuahua", "37 Sabre-toothed Tiger", "38 Lioness", "39 Fish", "40 Horse (head down)",
	"41 Horse (head up)", "42 Skull", "43 Fighter Jet I", "44 Toad", "45 Convertible",
	"46 Porsche II", "47 Hare", "48 Vintage Car", "49 Fighter Jet II", "50 Gargoyle",
	"51 Chef", "52 Parasaurolophus", "53 Rooster", "54 T-rex", "55 Sphere",
	"56 Gingerbread Man", "57 Monkey Head", "58 Thriller"
};

// ---- [Code for using the mouse to adjust floats] ----------------------------
// Calling setTool(vX, vY, vMat, wX, wY, wMat) below makes the left button adjust *vX and *vY
// as the mouse moves in the X and Y directions, via the transformation vMat which can be used
// for scaling and rotation. Similarly the middle button adjusts *wX and *wY via wMat.
// Any of vX, vY, wX, wY may be NULL in which case nothing is adjusted for that component.

static vec2 prevPos;
static mat2 leftTrans, middTrans;
static int currButton = -1;

static void doNothingCallback(vec2 xy) {
	return;
}

static void(*leftCallback)(vec2) = &doNothingCallback;
static void(*middCallback)(vec2) = &doNothingCallback;

static int mouseX = 0, mouseY = 0;  // Updated in the mousePassiveMotion function.

static vec2 currMouseXYScreen(float x, float y) {
	return vec2(x / windowWidth, ((float) windowHeight - y) / windowHeight);
}

static void doToolUpdateXY(int x, int y) {
	if (currButton == GLUT_LEFT_BUTTON || currButton == GLUT_MIDDLE_BUTTON) {
		vec2 currPos = vec2(currMouseXYScreen(x, y));
		if (currButton == GLUT_LEFT_BUTTON) {
			leftCallback(leftTrans * (currPos - prevPos));
		} else {
			middCallback(middTrans * (currPos - prevPos));
		}
		prevPos = currPos;
		glutPostRedisplay();
	}
}

static mat2 rotZ(float rotSidewaysDeg) {
	mat4 rot4 = RotateZ(rotSidewaysDeg);
	return mat2(rot4[0][0], rot4[0][1], rot4[1][0], rot4[1][1]);  // Extract X-Y part
}

static vec2 currMouseXYWorld(float rotSidewaysDeg) {
	return rotZ(rotSidewaysDeg) * currMouseXYScreen(mouseX, mouseY);
}

// See the comment about 40 lines above.
static void setToolCallbacks(void(*newLeftCallback)(vec2 transformedMovement), mat2 leftT, void(*newMiddCallback)(vec2 transformedMovement), mat2 middT) {
	leftCallback = newLeftCallback;
	leftTrans = leftT;
	middCallback = newMiddCallback;
	middTrans = middT;

	currButton = -1;  // No current button to start with
}

static void activateTool(int button) {
	currButton = button;
	prevPos = currMouseXYScreen(mouseX, mouseY);
}

static void deactivateTool() {
	currButton = -1;
}

// -----------------------------------------------------------------------------
