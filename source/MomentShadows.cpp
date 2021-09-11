#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "glsw.h"
#include "model.h"
#include "shader_s.h"
#include "arcball_camera.h"
#include "framebuffer.h"
#include "utility.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>            // Initialize with gl3wInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>            // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>          // Initialize with gladLoadGL()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING2)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/Binding.h>  // Initialize with glbinding::Binding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLBINDING3)
#define GLFW_INCLUDE_NONE       // GLFW including OpenGL headers causes ambiguity or multiple definition errors.
#include <glbinding/glbinding.h>// Initialize with glbinding::initialize()
#include <glbinding/gl/gl.h>
using namespace gl;
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

#include <iostream>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#define PATH fs::current_path().generic_string()

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path, bool gammaCorrection);
void renderQuad();
void renderCube();


// settings
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;
const unsigned int SHADOW_MAP_SIZE = 2048;
const float MAX_CAMERA_DISTANCE = 200.0f;
const unsigned int LIGHT_GRID_WIDTH = 5;  // point light grid size
const unsigned int LIGHT_GRID_HEIGHT = 4;  // point light vertical grid height
const float INITIAL_POINT_LIGHT_RADIUS = 0.870f;

// compute shader related:
// 16 and 32 do well on BYT, anything in between or below is bad, values above were not thoroughly tested; 32 seems to do well on laptop/desktop Windows Intel and on NVidia/AMD as well
// (further hardware-specific tuning probably needed for optimal performance)
static const int CS_THREAD_GROUP_SIZE = 32;


// camera
ArcballCamera arcballCamera(glm::vec3(0.0f, 1.5f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
// global light
ArcballCamera arcballLight(glm::vec3(-2.5f, 5.0f, -1.25f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;
bool leftMouseButtonPressed = false;
bool rightMouseButtonPressed = false;
int mouseControl = 0;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// struct to hold information about scene light
struct SceneLight {
    SceneLight(const glm::vec3& _position, const glm::vec3& _color, float _radius)
        : position(_position), color(_color), radius(_radius)
    {}
    glm::vec3 position;      // world light position
    glm::vec3 color;         // light's color
    float     radius;        // light's radius

};

// buffer for light instance data
unsigned int matrixBuffer;
unsigned int colorSizeBuffer;

void configurePointLights(std::vector<glm::mat4>& modelMatrices, std::vector<glm::vec4>& modelColorSizes, float radius = 1.0f, float separation = 1.0f, float yOffset = 0.0f);
void updatePointLights(std::vector<glm::mat4>& modelMatrices, std::vector<glm::vec4>& modelColorSizes, float separation, float yOffset, float radiusScale);

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    const char* glsl_version = "#version 430";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Moment Shadow Mapping (Roman Timurson)", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL); // set depth function to less than AND equal for cubemap depth trick.

    std::string globalShaderConstants;
    //int computeShaderKernelSize = 15; // 7, 15, 23, 35, 63, 127
    int computeShaderKernel[6]{ 7, 15, 23, 35, 63, 127 };

    glswInit();
    glswSetPath("OpenGL/shaders/", ".glsl");
    glswAddDirectiveToken("", "#version 430 core");

    // define shader constants
    globalShaderConstants = cStringFormatA("#define cRTScreenSizeI ivec4( %d, %d, %d, %d ) \n", SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    glswAddDirectiveToken("*", globalShaderConstants.c_str());

    //globalShaderConstants = cStringFormatA("#define COMPUTE_SHADER_KERNEL_SIZE %d\n", computeShaderKernelSize);
    //glswAddDirectiveToken("*", globalShaderConstants.c_str());

    globalShaderConstants = cStringFormatA("#define CS_THREAD_GROUP_SIZE %d\n", CS_THREAD_GROUP_SIZE);
    glswAddDirectiveToken("*", globalShaderConstants.c_str());


    // hdr cubemap shaders
    Shader equirectangularToCubemapShader(glswGetShader("equirectToCubemap.Vertex"), glswGetShader("equirectToCubemap.Fragment"));
    Shader cubemapShader(glswGetShader("cubemap.Vertex"), glswGetShader("cubemap.Fragment"));
    // Shader for writing into a depth texture
    Shader shaderDepthWrite(glswGetShader("momentShadowMap.Vertex"), glswGetShader("momentShadowMap.Fragment"));
    // Compute shader for doing multi-pass moving average box filtering
    Shader computeBlurShaderH(glswGetShader("blurCompute.ComputeH"));
    Shader computeBlurShaderV(glswGetShader("blurCompute.ComputeV"));
    // Shader for visualiazing the depth texture
    Shader shaderDebugDepthMap(glswGetShader("debugMSM.Vertex"), glswGetShader("debugMSM.Fragment"));
    // G-Buffer pass shader for models w/o textures and just Kd, Ks, etc colors 
    Shader shaderGeometryPass(glswGetShader("gBuffer.Vertex"), glswGetShader("gBuffer.Fragment"));
    // G-Buffer pass shader for the models with textures (diffuse, specular, etc)
    Shader shaderTexturedGeometryPass(glswGetShader("gBufferTextured.Vertex"), glswGetShader("gBufferTextured.Fragment"));
    // First pass of deferred shader that will render the scene with a global light and shadow mapping
    Shader shaderLightingPass(glswGetShader("deferredShading.Vertex"), glswGetShader("deferredShading.Fragment"));
    // Shader for debugging the G-Buffer contents
    Shader shaderGBufferDebug(glswGetShader("gBufferDebug.Vertex"), glswGetShader("gBufferDebug.Fragment"));
    // Shader to render the light geometry for visualization and debugging
    Shader shaderGlobalLightSphere(glswGetShader("deferredLight.Vertex"), glswGetShader("deferredLight.Fragment"));
    Shader shaderLightSphere(glswGetShader("deferredLightInstanced.Vertex"), glswGetShader("deferredLightInstanced.Fragment"));
    // Shader for a final composite rendering of point(area) lights with generated G-Buffer
    Shader shaderPointLightingPass(glswGetShader("deferredPointLightInstanced.Vertex"), glswGetShader("deferredPointLightInstanced.Fragment"));

    // pbr: load the HDR environment map and render it into cubemap
    // ---------------------------------
    unsigned int captureFBO;
    unsigned int captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    int width, height, nrComponents;
    std::string hdrMapPath = PATH + "/OpenGL/images/newport_loft.hdr";
    float *data = stbi_loadf(hdrMapPath.c_str(), &width, &height, &nrComponents, 0);
    unsigned int hdrTexture;
    if (data)
    {
        glGenTextures(1, &hdrTexture);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);
        // load a floating point HDR texture data
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Failed to load HDR image." << std::endl;
    }

    // pbr: setup cubemap to render to and attach to framebuffer
    // ---------------------------------------------------------
    unsigned int envCubemap;
    glGenTextures(1, &envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // pbr: set up projection and view matrices for capturing data onto the 6 cubemap face directions
    // ----------------------------------------------------------------------------------------------
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] =
    {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    // pbr: convert HDR equirectangular environment map to cubemap equivalent
    // ----------------------------------------------------------------------
    equirectangularToCubemapShader.use();
    equirectangularToCubemapShader.setUniformInt("equirectangularMap", 0);
    equirectangularToCubemapShader.setUniformMat4("projection", captureProjection);

    // set viewport for rendering into cubemap
    glViewport(0, 0, 512, 512);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        equirectangularToCubemapShader.setUniformMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderCube();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    const float PLANE_HALF_WIDTH = 6.0f;
    float planeVertices[] = {
        // positions            // normals         // texcoords
         PLANE_HALF_WIDTH, -0.5f,  PLANE_HALF_WIDTH,  0.0f, 1.0f, 0.0f,  10.0f,  10.0f,
        -PLANE_HALF_WIDTH, -0.5f, -PLANE_HALF_WIDTH,  0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
        -PLANE_HALF_WIDTH, -0.5f,  PLANE_HALF_WIDTH,  0.0f, 1.0f, 0.0f,   0.0f,  10.0f,
        
         PLANE_HALF_WIDTH, -0.5f,  PLANE_HALF_WIDTH,  0.0f, 1.0f, 0.0f,  10.0f,  10.0f,
         PLANE_HALF_WIDTH, -0.5f, -PLANE_HALF_WIDTH,  0.0f, 1.0f, 0.0f,  10.0f, 0.0f,
        -PLANE_HALF_WIDTH, -0.5f, -PLANE_HALF_WIDTH,  0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
    };
    // create floor plane VAO
    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glBindVertexArray(0);

    // load textures
    // -------------
    std::string woodTexturePath = PATH + "/OpenGL/images/wood.png";
    unsigned int woodTexture = loadTexture(woodTexturePath.c_str(), false);

    // load models
    // -----------
    std::string bunnyPath = PATH + "/OpenGL/models/Bunny.obj";
    std::string dragonPath = PATH + "/OpenGL/models/Dragon.obj";
    std::string ajaxPath = PATH + "/OpenGL/models/Ajax.obj";
    std::string lucyPath = PATH + "/OpenGL/models/Lucy.obj";
    std::string heptoroid = PATH + "/OpenGL/models/heptoroid.obj";
    //std::string modelPath = PATH + "/OpenGL/models/Aphrodite.obj";
    Model meshModelA(dragonPath);
    //Model meshModelB(dragonPath);
   // Model meshModelC(bunnyPath);
    std::string spherePath = PATH + "/OpenGL/models/Sphere.obj";
    Model lightModel(spherePath);
    std::vector<glm::vec3> objectPositions;
    objectPositions.push_back(glm::vec3(0.0, 1.0, 0.0));
   /* objectPositions.push_back(glm::vec3(2.5, 1.0, -0.5));
    objectPositions.push_back(glm::vec3(-2.5, 1.0, -0.5));
    objectPositions.push_back(glm::vec3(0.0, 1.0, 2.0));*/
    std::vector<Model*> meshModels;
    meshModels.push_back(&meshModelA);
   // meshModels.push_back(&meshModelB);
    //meshModels.push_back(&meshModelC);

    // configure depth map framebuffer for shadow generation/filtering
    // ----------------------
    FrameBuffer sBuffer(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
    sBuffer.attachTexture(GL_RGBA32F);
    sBuffer.attachTexture(GL_RGBA32F);            // attach secondary texture for ping-pong blurring
    sBuffer.attachRender(GL_DEPTH_COMPONENT);     // attach Depth render buffer
    sBuffer.bindInput(0);
    // Remove artefacts on the edges of the shadowmap
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    sBuffer.bindInput(1);
    // Remove artefacts on the edges of the shadowmap
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // configure g-buffer framebuffer
    // ------------------------------
    FrameBuffer gBuffer(SCR_WIDTH, SCR_HEIGHT);
    gBuffer.attachTexture(GL_RGB16F, GL_NEAREST); // Position color buffer
    gBuffer.attachTexture(GL_RGB16F, GL_NEAREST); // Normal color buffer
    gBuffer.attachTexture(GL_RGB, GL_NEAREST);    // Diffuse (Kd)
    gBuffer.attachTexture(GL_RGBA, GL_NEAREST);   // Specular (Ks)
    gBuffer.bindOutput();                         // calls glDrawBuffers[i] for all attached textures
    gBuffer.attachRender(GL_DEPTH_COMPONENT);     // attach Depth render buffer
    gBuffer.check();
    FrameBuffer::unbind();                        // unbind framebuffer for now

    // lighting info
    // -------------
    // instance array data for our light volumes
    std::vector<glm::mat4> modelMatrices;
    std::vector<glm::vec4> modelColorSizes;

    // single global light
    SceneLight globalLight(glm::vec3(-2.5f, 5.0f, -1.25f), glm::vec3(1.0f, 1.0f, 1.0f), 0.125f);

    // option settings
    int gBufferMode = 0;
    int ShadowMethod = 1;  // 0 - Standard, 1 - MSM
    int KernelSizeOption = 0; // 7, 15, 23, 35, 63, 127
    bool enableShadows = true;
    bool drawPointLights = false;
    bool showDepthMap = false;
    bool drawPointLightsWireframe = true;
    glm::vec3 diffuseColor = glm::vec3(0.847f, 0.52f, 0.19f);
    glm::vec4 specularColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.8f);
    float glossiness = 16.0f;
    float gLinearAttenuation = 0.09f;
    float gQuadraticAttenuation = 0.032f;
    float pointLightIntensity = 0.545f;
    float pointLightRadius = INITIAL_POINT_LIGHT_RADIUS;
    float pointLightVerticalOffset = 1.205f;
    float pointLightSeparation = 0.620f;
    float modelScale = 0.9f;

    const int totalLights = LIGHT_GRID_WIDTH * LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT;
    // initialize point lights
    configurePointLights(modelMatrices, modelColorSizes, pointLightRadius, pointLightSeparation, pointLightVerticalOffset);
    
    // configure instanced array of model transform matrices
    // -------------------------
    glGenBuffers(1, &matrixBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, matrixBuffer);
    glBufferData(GL_ARRAY_BUFFER, totalLights * sizeof(glm::mat4), &modelMatrices[0], GL_STATIC_DRAW);

    // light model has only one mesh
    unsigned int VAO = lightModel.meshes[0].VAO;
    glBindVertexArray(VAO);

    // set attribute pointers for matrix (4 times vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(2 * sizeof(glm::vec4)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(3 * sizeof(glm::vec4)));

    glVertexAttribDivisor(3, 1);
    glVertexAttribDivisor(4, 1);
    glVertexAttribDivisor(5, 1);
    glVertexAttribDivisor(6, 1);

    // configure instanced array of light colors
    // -------------------------
    unsigned int colorSizeBuffer;
    glGenBuffers(1, &colorSizeBuffer);
   
    glBindVertexArray(VAO);
    // set attribute pointers for light color + radius (vec4)
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, colorSizeBuffer);
    glBufferData(GL_ARRAY_BUFFER, totalLights * sizeof(glm::vec4), &modelColorSizes[0], GL_STATIC_DRAW);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);
    
    // shader configuration
    // --------------------
    shaderLightingPass.use();
    shaderLightingPass.setUniformInt("gPosition", 0);
    shaderLightingPass.setUniformInt("gNormal", 1);
    shaderLightingPass.setUniformInt("gDiffuse", 2);
    shaderLightingPass.setUniformInt("gSpecular", 3);
    shaderLightingPass.setUniformInt("shadowMap", 4);
    shaderLightingPass.setUniformInt("shadowMethod", ShadowMethod);

    // deferred point lighting shader
    shaderPointLightingPass.use();
    shaderPointLightingPass.setUniformInt("gPosition", 0);
    shaderPointLightingPass.setUniformInt("gNormal", 1);
    shaderPointLightingPass.setUniformInt("gDiffuse", 2);
    shaderPointLightingPass.setUniformInt("gSpecular", 3);
    shaderPointLightingPass.setUniformVec2f("screenSize", SCR_WIDTH, SCR_HEIGHT);

    // G-Buffer debug shader
    shaderGBufferDebug.use();
    shaderGBufferDebug.setUniformInt("gPosition", 0);
    shaderGBufferDebug.setUniformInt("gNormal", 1);
    shaderGBufferDebug.setUniformInt("gDiffuse", 2);
    shaderGBufferDebug.setUniformInt("gSpecular", 3);
    shaderGBufferDebug.setUniformInt("gBufferMode", 1);

    // cubemap render shader
    cubemapShader.use();
    cubemapShader.setUniformInt("environmentMap", 0);

    // Shadow texture debug shader
    shaderDebugDepthMap.use();
    shaderDebugDepthMap.setUniformInt("depthMap", 0);


    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glEnable(GL_DEPTH_TEST);

        // 1. render depth of scene to texture (from light's perspective)
        // --------------------------------------------------------------
        glm::mat4 lightProjection, lightView;
        glm::mat4 lightSpaceMatrix;
        glm::mat4 model = glm::mat4(1.0f);
        float zNear = 1.0f, zFar = 10.0f;

        if (enableShadows) {
            lightProjection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, zNear, zFar);
            glm::vec3 lightPosition = arcballLight.eye();
            lightView = glm::lookAt(lightPosition, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
            lightSpaceMatrix = lightProjection * lightView;
            // render scene from light's point of view
            shaderDepthWrite.use();
            shaderDepthWrite.setUniformMat4("lightSpaceMatrix", lightSpaceMatrix);
            shaderDepthWrite.setUniformMat4("model", model);

            glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
            sBuffer.bindOutput();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            // render the textured floor
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, woodTexture);
            glBindVertexArray(planeVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            for (unsigned int i = 0; i < objectPositions.size(); i++)
            {
                model = glm::mat4(1.0f);
                model = glm::translate(model, objectPositions[i]);
                model = glm::scale(model, glm::vec3(modelScale));
                shaderDepthWrite.setUniformMat4("model", model);
                meshModels[i]->draw(shaderDepthWrite);
            }
            FrameBuffer::unbind();

            if (ShadowMethod == 1) { // MSM4
                // perform shadow map blurring 
                int width = (int)SHADOW_MAP_SIZE;
                int height = (int)SHADOW_MAP_SIZE;

                // Horizontal
                {
                    computeBlurShaderH.use();
                    computeBlurShaderH.setUniformInt("ComputeKernelSize", computeShaderKernel[KernelSizeOption]);
                    sBuffer.bindImage(0, 0, GL_RGBA32F);
                    sBuffer.bindImage(1, 1, GL_RGBA32F);
                    glDispatchCompute((height + CS_THREAD_GROUP_SIZE - 1) / CS_THREAD_GROUP_SIZE, 1, 1);
                    // make sure writing to image has finished before read
                    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                    sBuffer.bindImage(1, 0, GL_RGBA32F);
                    sBuffer.bindImage(0, 1, GL_RGBA32F);
                    glDispatchCompute((height + CS_THREAD_GROUP_SIZE - 1) / CS_THREAD_GROUP_SIZE, 1, 1);
                    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                }

                // Vertical
                {
                    computeBlurShaderV.use();
                    computeBlurShaderV.setUniformInt("ComputeKernelSize", computeShaderKernel[KernelSizeOption]);
                    sBuffer.bindImage(0, 0, GL_RGBA32F);
                    sBuffer.bindImage(1, 1, GL_RGBA32F);
                    glDispatchCompute((width + CS_THREAD_GROUP_SIZE - 1) / CS_THREAD_GROUP_SIZE, 1, 1);
                    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                    sBuffer.bindImage(1, 0, GL_RGBA32F);
                    sBuffer.bindImage(0, 1, GL_RGBA32F);
                    glDispatchCompute((width + CS_THREAD_GROUP_SIZE - 1) / CS_THREAD_GROUP_SIZE, 1, 1);
                    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                }
            }     
        }
        else {
            // just clear the depth texture if shadows aren't being generated
            glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
            sBuffer.bindOutput();
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }
        
        // 2. geometry pass: render scene's geometry/color data into gbuffer
        // -----------------------------------------------------------------
        // reset viewport
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        gBuffer.bindOutput();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 150.0f);
        glm::mat4 view = arcballCamera.transform();
        model = glm::mat4(1.0f);
        cubemapShader.use();
        cubemapShader.setUniformMat4("projection", projection);

        shaderTexturedGeometryPass.use();
        shaderTexturedGeometryPass.setUniformMat4("projection", projection);
        shaderTexturedGeometryPass.setUniformMat4("view", view);
        shaderTexturedGeometryPass.setUniformMat4("model", model);
        glm::vec4 floorSpecular = glm::vec4(0.5f, 0.5f, 0.5f, 0.8f);
        shaderTexturedGeometryPass.setUniformVec4f("specularCol", floorSpecular);
        // render the textured floor
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, woodTexture);
        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // render non-textured models
        shaderGeometryPass.use();
        shaderGeometryPass.setUniformMat4("projection", projection);
        shaderGeometryPass.setUniformMat4("view", view);
        shaderGeometryPass.setUniformMat4("model", model);
        shaderGeometryPass.setUniformVec3f("diffuseCol", diffuseColor);
        glm::vec4 specular = glm::vec4(1.0f, 1.0f, 1.0f, 0.1f);
        glm::vec4 spec = glm::vec4(1.0f, 1.0f, 1.0f, 0.1f);
        shaderGeometryPass.setUniformVec4f("specularCol", specularColor);
        for (unsigned int i = 0; i < objectPositions.size(); i++)
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, objectPositions[i]);
            model = glm::scale(model, glm::vec3(modelScale));
            shaderGeometryPass.setUniformMat4("model", model);
            meshModels[i]->draw(shaderGeometryPass);
        }
        FrameBuffer::unbind();

        // 3. lighting pass: calculate lighting by iterating over a screen filled quad pixel-by-pixel using the gbuffer's content and shadow map
        // -----------------------------------------------------------------------------------------------------------------------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (gBufferMode == 0)
        {
            shaderLightingPass.use();
            // bind all of our input textures
            gBuffer.bindInput();

            // bind depth texture
            glActiveTexture(GL_TEXTURE4);
            sBuffer.bindTex(0);

            glm::vec3 lightPosition = arcballLight.eye();
            shaderLightingPass.setUniformVec3f("gLight.Position", lightPosition);
            shaderLightingPass.setUniformVec3f("gLight.Color", globalLight.color);
            shaderLightingPass.setUniformFloat("gLight.Linear", gLinearAttenuation);
            shaderLightingPass.setUniformFloat("gLight.Quadratic", gQuadraticAttenuation);

            glm::vec3 camPosition = arcballCamera.eye();
            shaderLightingPass.setUniformVec3f("viewPos", camPosition);
            shaderLightingPass.setUniformMat4("lightSpaceMatrix", lightSpaceMatrix);
            shaderLightingPass.setUniformFloat("glossiness", glossiness);
            shaderLightingPass.setUniformInt("shadowMethod", ShadowMethod);
        }
        else // for G-Buffer debuging 
        {
            shaderGBufferDebug.use();
            shaderGBufferDebug.setUniformInt("gBufferMode", gBufferMode);
            // bind all of our input textures
            gBuffer.bindInput();
        }
        
        // finally render quad
        renderQuad();

        static bool colorSizeBufferDirty = false;

        // 3.5 lighting pass: render point lights on top of main scene with additive blending and utilizing G-Buffer for lighting.
        // -----------------------------------------------------------------------------------------------------------------------
        if (gBufferMode == 0) {
            shaderPointLightingPass.use();
            gBuffer.bindInput();
            shaderPointLightingPass.setUniformMat4("projection", projection);
            shaderPointLightingPass.setUniformMat4("view", view);

            glEnable(GL_CULL_FACE);
            // only render the back faces of the light volume spheres
            glFrontFace(GL_CW);
            glDisable(GL_DEPTH_TEST);
            // enable additive blending
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glm::vec3 camPosition = arcballCamera.eye();
            shaderPointLightingPass.setUniformVec3f("viewPos", camPosition);
            shaderPointLightingPass.setUniformFloat("lightIntensity", pointLightIntensity);
            shaderPointLightingPass.setUniformFloat("glossiness", glossiness);
            glBindVertexArray(lightModel.meshes[0].VAO);
            // don't update the color and size buffer every frame
            if (colorSizeBufferDirty) {
                glBindBuffer(GL_ARRAY_BUFFER, colorSizeBuffer);
                glBufferData(GL_ARRAY_BUFFER, LIGHT_GRID_WIDTH * LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT * sizeof(glm::vec4), &modelColorSizes[0], GL_STATIC_DRAW);
            }
            glDrawElementsInstanced(GL_TRIANGLES, lightModel.meshes[0].indices.size(), GL_UNSIGNED_INT, 0, totalLights);
            glBindVertexArray(0);

            glDisable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glFrontFace(GL_CCW);
            glDisable(GL_CULL_FACE);
        }

        // render cubemap with depth testing enabled
        if (gBufferMode == 0) { 
            // copy content of geometry's depth buffer to default framebuffer's depth buffer
            // ----------------------------------------------------------------------------------
            gBuffer.bindRead();
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
            // blit to default framebuffer. 
            glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT, 0, 0, SCR_WIDTH, SCR_HEIGHT, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            // unbind framebuffer for now
            FrameBuffer::unbind();

            glEnable(GL_DEPTH_TEST);
            cubemapShader.use();
            cubemapShader.setUniformMat4("view", view);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
            renderCube();
        }

        // strictly used for debugging point light volumes (sizes, positions, etc)
        if (drawPointLights && gBufferMode == 0) {
            // re-enable the depth testing 
            glEnable(GL_DEPTH_TEST);

            // render lights on top of scene with Z-testing
            // --------------------------------
            shaderLightSphere.use();
            shaderLightSphere.setUniformMat4("projection", projection);
            shaderLightSphere.setUniformMat4("view", view);

            glPolygonMode(GL_FRONT_AND_BACK, drawPointLightsWireframe ? GL_LINE : GL_FILL);
            glBindVertexArray(lightModel.meshes[0].VAO);
            glDrawElementsInstanced(GL_TRIANGLES, lightModel.meshes[0].indices.size(), GL_UNSIGNED_INT, 0, totalLights);
            glBindVertexArray(0);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            shaderGlobalLightSphere.use();
            shaderGlobalLightSphere.setUniformMat4("projection", projection);
            shaderGlobalLightSphere.setUniformMat4("view", view);
            // render the global light model
            model = glm::mat4(1.0f);
            model = glm::translate(model, arcballLight.eye());
            shaderGlobalLightSphere.setUniformMat4("model", model);
            shaderGlobalLightSphere.setUniformVec3f("lightColor", globalLight.color);
            shaderGlobalLightSphere.setUniformFloat("lightRadius", globalLight.radius);
            lightModel.draw(shaderGlobalLightSphere);
        }

        if (showDepthMap) {
            // render Depth map to quad for visual debugging
            // ---------------------------------------------
            model = glm::mat4(1.0f);
            //model = glm::translate(model, glm::vec3(0.7f, -0.7f, 0.0f));
            //model = glm::scale(model, glm::vec3(0.3f, 0.3f, 1.0f)); // Make it 30% of total screen size
            shaderDebugDepthMap.use();
            shaderDebugDepthMap.setUniformMat4("transform", model);
            shaderDebugDepthMap.setUniformFloat("zNear", zNear);
            shaderDebugDepthMap.setUniformFloat("zFar", zFar);
            glActiveTexture(GL_TEXTURE0);
            sBuffer.bindInput(0);
            renderQuad();
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Controls");                          // Create a window called "Controls" and append into it.

            if (ImGui::CollapsingHeader("Model Config")) {
                ImGui::ColorEdit3("Diffuse (Kd)", (float*)&diffuseColor);   // Edit 3 floats representing Kd color (r, g, b)
                ImGui::ColorEdit4("Specular (Ks)", (float*)&specularColor); // Edit 4 floats representing Ks color (r, g, b, alpha)
                ImGui::SliderFloat("Glossiness", &glossiness, 8.0, 128.0f);
            }
            if (ImGui::CollapsingHeader("Lighting Config")) {
                if (ImGui::CollapsingHeader("Global Light")) {
                    ImGui::Text("Attenuation");
                    ImGui::SliderFloat("Linear", &gLinearAttenuation, 0.022f, 0.7f);
                    ImGui::SliderFloat("Quadratic", &gQuadraticAttenuation, 0.0019f, 1.8f);     
                }

                if (ImGui::CollapsingHeader("Point Lights")) {
                    ImGui::SliderFloat("Intensity", &pointLightIntensity, 0.0f, 3.0f, "%.3f");
                    if (ImGui::SliderFloat("Radius", &pointLightRadius, 0.3f, 2.5f, "%.3f")) {
                        updatePointLights(modelMatrices, modelColorSizes, pointLightSeparation, pointLightVerticalOffset, pointLightRadius);
                        colorSizeBufferDirty = true;
                    }
                    else {
                        colorSizeBufferDirty = false;
                    }
                    if (ImGui::SliderFloat("Separation", &pointLightSeparation, 0.4f, 1.5f, "%.3f")) {
                        updatePointLights(modelMatrices, modelColorSizes, pointLightSeparation, pointLightVerticalOffset, pointLightRadius);
                    }
                    if (ImGui::SliderFloat("Vertical Offset", &pointLightVerticalOffset, -2.0f, 3.0f)) {
                        updatePointLights(modelMatrices, modelColorSizes, pointLightSeparation, pointLightVerticalOffset, pointLightRadius);
                    }
                }

                // Shadows
                if (ImGui::CollapsingHeader("Shadows")) {
                    ImGui::Checkbox("Enabled", &enableShadows);
                    const char* shadowMethod[] = { "Standard", "Moment Shadow Map"};
                    ImGui::Combo("Shadow Method", &ShadowMethod, shadowMethod, IM_ARRAYSIZE(shadowMethod));
                    // 7, 15, 23, 35, 63, 127
                    const char* kernelSize[] = { "7x7", "15x15", "23x23", "35x35", "63x63", "127x127" };
                    ImGui::Combo("Blur Kernel", &KernelSizeOption, kernelSize, IM_ARRAYSIZE(kernelSize));
                }
            }
            if (ImGui::CollapsingHeader("Debug")) {
                const char* gBuffers[] = { "Final render", "Position (world)", "Normal (world)", "Diffuse", "Specular"};
                ImGui::Combo("G-Buffer View", &gBufferMode, gBuffers, IM_ARRAYSIZE(gBuffers));
                shaderLightingPass.setUniformInt("gBufferMode", gBufferMode);
                ImGui::Checkbox("Point lights volumes", &drawPointLights);
                ImGui::SameLine(); ImGui::Checkbox("Wireframe", &drawPointLightsWireframe);
                ImGui::Checkbox("Show depth texture", &showDepthMap);
                ImGui::Text("Mouse Controls:");
                ImGui::RadioButton("Camera", &mouseControl, 0); ImGui::SameLine();
                ImGui::RadioButton("Light", &mouseControl, 1);
            }
                                                                    
            //ImGui::ShowDemoWindow();

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::Text("Point lights in scene: %i", LIGHT_GRID_WIDTH * LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT);
            ImGui::End();

        }

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);

    glfwTerminate();
    return 0;

}

// Node: separation < 1.0 will cause lights to penetrate each other, and > 1.0 they will separate (1.0 is just touching)
void configurePointLights(std::vector<glm::mat4>& modelMatrices, std::vector<glm::vec4>& modelColorSizes, float radius, float separation, float yOffset)
{
    srand(glfwGetTime());
    // add some uniformly spaced point lights
    for (unsigned int lightIndexX = 0; lightIndexX < LIGHT_GRID_WIDTH; lightIndexX++)
    {
        for (unsigned int lightIndexZ = 0; lightIndexZ < LIGHT_GRID_WIDTH; lightIndexZ++)
        {
            for (unsigned int lightIndexY = 0; lightIndexY < LIGHT_GRID_HEIGHT; lightIndexY++)
            {
                float diameter = 2.0f * radius;
                float xPos = (lightIndexX - (LIGHT_GRID_WIDTH - 1.0f) / 2.0f) * (diameter * separation);
                float zPos = (lightIndexZ - (LIGHT_GRID_WIDTH - 1.0f) / 2.0f) * (diameter * separation);
                float yPos = (lightIndexY - (LIGHT_GRID_HEIGHT - 1.0f) / 2.0f) * (diameter * separation) + yOffset;
                double angle = double(rand()) * 2.0 * glm::pi<float>() / (double(RAND_MAX));
                double length = double(rand()) * 0.5 / (double(RAND_MAX));
                float xOffset = cos(angle) * length;
                float zOffset = sin(angle) * length;
                xPos += xOffset;
                zPos += zOffset;
                // also calculate random color
                float rColor = ((rand() % 100) / 200.0f) + 0.5; // between 0.5 and 1.0
                float gColor = ((rand() % 100) / 200.0f) + 0.5; // between 0.5 and 1.0
                float bColor = ((rand() % 100) / 200.0f) + 0.5; // between 0.5 and 1.0

                int curLight = lightIndexX * LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT + lightIndexZ * LIGHT_GRID_HEIGHT + lightIndexY;
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(xPos, yPos, zPos));
                // now add to list of matrices
                modelMatrices.emplace_back(model);
                modelColorSizes.emplace_back(glm::vec4(rColor, gColor, bColor, radius));
            }
        }
    }
}

void updatePointLights(std::vector<glm::mat4>& modelMatrices, std::vector<glm::vec4>& modelColorSizes, float separation, float yOffset, float radius)
{
    if (separation < 0.0f) {
        return;
    }
    // add some uniformly spaced point lights
    for (unsigned int lightIndexX = 0; lightIndexX < LIGHT_GRID_WIDTH; lightIndexX++)
    {
        for (unsigned int lightIndexZ = 0; lightIndexZ < LIGHT_GRID_WIDTH; lightIndexZ++)
        {
            for (unsigned int lightIndexY = 0; lightIndexY < LIGHT_GRID_HEIGHT; lightIndexY++)
            {
                int curLight = lightIndexX * LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT + lightIndexZ * LIGHT_GRID_HEIGHT + lightIndexY;
                float diameter = 2.0f * INITIAL_POINT_LIGHT_RADIUS;
                float xPos = (lightIndexX - (LIGHT_GRID_WIDTH - 1.0f) / 2.0f) * (diameter * separation);
                float zPos = (lightIndexZ - (LIGHT_GRID_WIDTH - 1.0f) / 2.0f) * (diameter * separation);
                float yPos = (lightIndexY - (LIGHT_GRID_HEIGHT - 1.0f) / 2.0f) * (diameter * separation);
                
                // modify matrix translation
                modelMatrices[curLight][3] = glm::vec4(xPos, yPos + yOffset, zPos, 1.0);
                modelColorSizes[curLight].w = radius;
            }
        }
    }

    // update the instance matrix buffer
    glBindBuffer(GL_ARRAY_BUFFER, matrixBuffer);
    glBufferData(GL_ARRAY_BUFFER, LIGHT_GRID_WIDTH * LIGHT_GRID_WIDTH * LIGHT_GRID_HEIGHT * sizeof(glm::mat4), &modelMatrices[0], GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    ImGuiIO& io = ImGui::GetIO();

    // only rotate the camera if we aren't over imGui
    if (leftMouseButtonPressed && !io.WantCaptureMouse) {
        //std::cout << "Xpos = " << xpos << ", Ypos = " << ypos << std::endl;
        float prevMouseX = 2.0f * lastX / SCR_WIDTH - 1;
        float prevMouseY = -1.0f * (2.0f * lastY / SCR_HEIGHT - 1);
        float curMouseX = 2.0f * xpos / SCR_WIDTH - 1;
        float curMouseY = -1.0f * (2.0f * ypos / SCR_HEIGHT - 1);
        if (mouseControl == 1) { // apply rotation to the global light
            arcballLight.rotate(glm::vec2(prevMouseX, prevMouseY), glm::vec2(curMouseX, curMouseY));
        }
        else {
            arcballCamera.rotate(glm::vec2(prevMouseX, prevMouseY), glm::vec2(curMouseX, curMouseY));
        } 
    }

    // pan the camera when the right mouse is pressed
    if (rightMouseButtonPressed && !io.WantCaptureMouse) {
        float prevMouseX = 2.0f * lastX / SCR_WIDTH - 1;
        float prevMouseY = -1.0f * (2.0f * lastY / SCR_HEIGHT - 1);
        float curMouseX = 2.0f * xpos / SCR_WIDTH - 1;
        float curMouseY = -1.0f * (2.0f * ypos / SCR_HEIGHT - 1);
        glm::vec2 mouseDelta = glm::vec2(curMouseX - prevMouseX, curMouseY - prevMouseY);
        arcballCamera.pan(mouseDelta);
    }

    lastX = xpos;
    lastY = ypos;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        leftMouseButtonPressed = true;
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        leftMouseButtonPressed = false;
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        rightMouseButtonPressed = true;
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        rightMouseButtonPressed = false;
    }
      
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    float distanceSq = glm::distance2(arcballCamera.center(), arcballCamera.eye());
    if (distanceSq < MAX_CAMERA_DISTANCE && yoffset < 0)
    {
        // zoom out
        arcballCamera.zoom(yoffset);
    }
    else if (yoffset > 0)
    {
        // zoom in
        arcballCamera.zoom(yoffset);
    }
}

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const * path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum internalFormat;
        GLenum dataFormat;
        if (nrComponents == 1)
        {
            internalFormat = dataFormat = GL_RED;
        }
        else if (nrComponents == 3)
        {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        }
        else if (nrComponents == 4)
        {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, internalFormat == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT); // for this tutorial: use GL_CLAMP_TO_EDGE to prevent semi-transparent borders. Due to interpolation it takes texels from next repeat 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, internalFormat == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}