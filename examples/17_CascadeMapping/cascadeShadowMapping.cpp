#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <vector>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shaders/shader.hpp"
#include "root_directory.h"
#include "cameras/cameraFirstPerson.hpp"
#include "performanceMonitor.hpp"

#include <iostream>
#include <ctime>
#include <algorithm>    // std::min
#include <limits>

#include <iomanip>
#include <map>
#include <random>

using namespace std;

// settings
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;

// camera
CameraFirstPerson camera(glm::vec3(-64.0f, 2.5f, 0.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct RenderObject {
    unsigned int VAO, VBO;
    unsigned indexCount;
    glm::mat4 transform;
    unsigned int textureId;
    glm::vec3 ka, kd, ks, color;
    float shininess;
};

struct DirectionalLight {
    glm::vec3 direction;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    // Shadows
    unsigned int depthMap;
    unsigned int depthMapFBO;
    glm::vec3 position;
    glm::mat4 spaceMatrix;
    glm::mat4 projection;
    glm::mat4 view;
    float nearPlane;
    float farPlane;
    float orthoDim;
};

struct OrthoProjInfo
{
    float r;        // right
    float l;        // left
    float b;        // bottom
    float t;        // top
    float n;        // z near
    float f;        // z far
};

struct PersProjInfo
{
    float fov;
    float width;
    float height;
    float zNear;
    float zFar;
};

typedef shared_ptr<RenderObject> RenderObjectPtr;
typedef vector<RenderObjectPtr> RenderBatch;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
RenderObjectPtr createTexCube(string path, float texScale, 
                            glm::vec3 ka=glm::vec3(0.5f), 
                            glm::vec3 kd=glm::vec3(0.5f), 
                            glm::vec3 ks=glm::vec3(0.5f), 
                            float shnss=32.0f);
RenderObjectPtr createClrCube(glm::vec3 color, 
                            glm::vec3 ka=glm::vec3(0.5f), 
                            glm::vec3 kd=glm::vec3(0.5f), 
                            glm::vec3 ks=glm::vec3(0.5f), 
                            float shnss=32.0f);
RenderObjectPtr createLightCube(glm::vec3 pos);
RenderObjectPtr createTexQuad();
unsigned int loadTexture(const char *path, bool gammaCorrection);
void CalcOrthoProjs();
glm::mat4 getOrthoProj(OrthoProjInfo& info);

#define NUM_CASCADES 3

bool showCascade = false;
int depthMapRendered = 0;
PersProjInfo cameraProjInfo;
float mCascadeEnd[NUM_CASCADES + 1];
OrthoProjInfo mShadowOrthoProjInfo[NUM_CASCADES];
DirectionalLight* dirLight;

float genRand() {
    return rand() / static_cast<float>(RAND_MAX);
}

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    string title = "Cascade Shadow Mapping";
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, title.c_str(), NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile our shader zprogram
    Shader dirLightTexShader(getPath("source/shaders/DirLightCSMTexShader.vs").string().c_str(), 
                               getPath("source/shaders/DirLightCSMTexShader.fs").string().c_str() );
    Shader dirLightClrShader(getPath("source/shaders/DirLightCSMClrShader.vs").string().c_str(), 
                               getPath("source/shaders/DirLightCSMClrShader.fs").string().c_str() );
    Shader depthMappingShader(getPath("source/shaders/ShadowMapDepthShader.vs").string().c_str(), 
                           getPath("source/shaders/ShadowMapDepthShader.fs").string().c_str() );
    Shader depthDebugShader(getPath("source/shaders/depthMapping.vs").string().c_str(), 
                           getPath("source/shaders/depthMapping.fs").string().c_str() );
    Shader cascadeDebugTexShader(getPath("source/shaders/CascadeMappingTexShader.vs").string().c_str(), 
                               getPath("source/shaders/CascadeMappingTexShader.fs").string().c_str() );
    Shader cascadeDebugClrShader(getPath("source/shaders/CascadeMappingClrShader.vs").string().c_str(), 
                               getPath("source/shaders/CascadeMappingClrShader.fs").string().c_str() );

    // shader configuration
    // --------------------
    depthDebugShader.use();
    depthDebugShader.setInt("depthMap", 0);

    dirLightTexShader.use();
    dirLightTexShader.setInt("texture_diffuse0", 0);
    dirLightTexShader.setInt("shadowMap[0]", 1);
    dirLightTexShader.setInt("shadowMap[1]", 2);
    dirLightTexShader.setInt("shadowMap[2]", 3);

    dirLightClrShader.use();
    dirLightClrShader.setInt("shadowMap[0]", 0);
    dirLightClrShader.setInt("shadowMap[1]", 1);
    dirLightClrShader.setInt("shadowMap[2]", 2);

    cascadeDebugTexShader.use();
    cascadeDebugTexShader.setInt("texture_diffuse0", 0);
    cascadeDebugTexShader.setInt("shadowMap[0]", 1);
    cascadeDebugTexShader.setInt("shadowMap[1]", 2);
    cascadeDebugTexShader.setInt("shadowMap[2]", 3);

    cascadeDebugClrShader.use();
    cascadeDebugClrShader.setInt("shadowMap[0]", 0);
    cascadeDebugClrShader.setInt("shadowMap[1]", 1);
    cascadeDebugClrShader.setInt("shadowMap[2]", 2);

    // ---------------------

    dirLight = new DirectionalLight;
    dirLight->direction = glm::normalize(glm::vec3(1.0f, -1.0f, 0.5f));
    dirLight->ambient = glm::vec3(0.7f);
    dirLight->diffuse = glm::vec3(1.0f);
    dirLight->specular = glm::vec3(1.0f);
    dirLight->nearPlane = 0.01f;
    dirLight->farPlane = 17.5f;
    dirLight->orthoDim = 10.0f;
    dirLight->projection = glm::ortho(-dirLight->orthoDim, dirLight->orthoDim, -dirLight->orthoDim, dirLight->orthoDim, dirLight->nearPlane, dirLight->farPlane);
    
    cout << "[2][3]" << dirLight->projection[2][3] << endl; // 0
    cout << "[3][2]" << dirLight->projection[3][2] << endl; // -1
    // Render batches
    RenderBatch phongTexObjects;
    RenderBatch phongClrObjects;
    RenderBatch coloredObjects;

    camera.MovementSpeed = 10.0f;

    cameraProjInfo.fov = camera.Zoom;
    cameraProjInfo.height = (float)SCR_HEIGHT;
    cameraProjInfo.width = (float)SCR_WIDTH;
    cameraProjInfo.zNear = 0.1f;
    cameraProjInfo.zFar = 100.0f;

    // ------- CASCADE SETUP ----------
    mCascadeEnd[0] = cameraProjInfo.zNear;
    mCascadeEnd[1] = cameraProjInfo.zNear + (cameraProjInfo.zFar - cameraProjInfo.zNear) * 0.15;
    mCascadeEnd[2] = cameraProjInfo.zNear + (cameraProjInfo.zFar - cameraProjInfo.zNear) * 0.45;
    mCascadeEnd[3] = cameraProjInfo.zFar;

    GLuint mFbo;
    GLuint mShadowMap[NUM_CASCADES];

    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

    // Create the FBO
    glGenFramebuffers(1, &mFbo);
    // Create the depth buffer
    glGenTextures(NUM_CASCADES, mShadowMap);
    for (unsigned int i = 0 ; i < NUM_CASCADES ; i++) {
        glBindTexture(GL_TEXTURE_2D, mShadowMap[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, mFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mShadowMap[0], 0);
    // Disable writes to the color buffer
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (Status != GL_FRAMEBUFFER_COMPLETE) {
        printf("FB error, status: 0x%x\n", Status);
        return false;
    }

    float mCascadeEndClipSpace[NUM_CASCADES];
    glm::mat4 mShadowMapProjs[NUM_CASCADES];


    // --------------------------------

    /// ------- FOR DIRECTIONAL LIGHT --------
    glGenFramebuffers(1, &(dirLight->depthMapFBO));
    glGenTextures(1, &(dirLight->depthMap)); // create depth texture
    glBindTexture(GL_TEXTURE_2D, (dirLight->depthMap));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    // attach depth texture as FBO's depth buffer
    glBindFramebuffer(GL_FRAMEBUFFER, (dirLight->depthMapFBO));
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, (dirLight->depthMap), 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    srand(time(NULL));
    glm::vec3 center(16.0f, 5.0f, 0.0f);
    float width = 32.0f;
    float height = 2.0f;
    float deep = 20.0f;
    int boxes = 42;
    for (int i = 0; i < boxes; i++) {
        RenderObjectPtr box = createTexCube("assets/box.png", 1.0f);
        box->transform = glm::translate(box->transform, center + glm::vec3( (genRand()-0.5f)*2.0f*width, (genRand()-0.5f)*2.0f*height, (genRand()-0.5f)*2.0f*deep ));
        box->transform = glm::rotate(box->transform, glm::radians(genRand()*360.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        box->transform = glm::rotate(box->transform, glm::radians(genRand()*360.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        box->transform = glm::rotate(box->transform, glm::radians(genRand()*360.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        box->transform = glm::scale(box->transform, glm::vec3(1.0f + (genRand()*2.0f)));
        phongTexObjects.push_back(box);
    }

    // Phong textured objects
    RenderObjectPtr floor = createTexCube("assets/grass.png", 5.0f);
    floor->transform = glm::translate(floor->transform, glm::vec3(0.0f, -8.0f, 0.0f));
    floor->transform = glm::scale(floor->transform, glm::vec3(128.0f, 16.0f, 64.0f));
    phongTexObjects.push_back(floor);

    // Phong colored objects
    RenderObjectPtr house1 = createClrCube(glm::vec3(0.6f, 0.6f, 0.6f));
    house1->transform = glm::translate(house1->transform, glm::vec3(-48.0f, 4.0f, -20.0f));
    house1->transform = glm::scale(house1->transform, glm::vec3(26.0f, 8.0f, 8.0f));
    phongClrObjects.push_back(house1);
    // Phong colored objects
    RenderObjectPtr house2 = createClrCube(glm::vec3(0.6f, 0.6f, 0.6f));
    house2->transform = glm::translate(house2->transform, glm::vec3(-48.0f, 16.0f, -20.0f));
    house2->transform = glm::scale(house2->transform, glm::vec3(8.0f, 32.0f, 8.0f));
    phongClrObjects.push_back(house2);

    // Depth map quad object
    RenderObjectPtr depthQuad = createTexQuad();

    PerformanceMonitor pMonitor(glfwGetTime(), 0.5f);

    // Use fcae culling to fix peter panning problwms with shadows
    //glEnable(GL_CULL_FACE);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        pMonitor.update(glfwGetTime());
        stringstream ss;
        ss << title << " " << pMonitor;
        glfwSetWindowTitle(window, ss.str().c_str());

        // input
        // -----
        processInput(window);
        
        // render
        // ------
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cameraProjInfo.fov = camera.Zoom;
        glm::mat4 projection = glm::perspective(glm::radians(cameraProjInfo.fov), cameraProjInfo.width / cameraProjInfo.height, cameraProjInfo.zNear, cameraProjInfo.zFar);

        dirLight->position = camera.Position + camera.Front * 3.0f + camera.Up *6.0f;
        dirLight->view = glm::lookAt(dirLight->position, dirLight->position + glm::normalize(dirLight->direction), glm::vec3(0.0, 1.0, 0.0));
        dirLight->spaceMatrix = dirLight->projection * dirLight->view;


        // 1. CALCULATE THE PROJECTION MATRIX FOR EACH CASCADE
        for (unsigned int i = 0; i < NUM_CASCADES; i++) {
            glm::vec4 vView(0.0f, 0.0f, mCascadeEnd[i+1], 1.0f);
            glm::vec4 vClip = projection * vView;
            mCascadeEndClipSpace[i] = -vClip.z;
        }
        CalcOrthoProjs();

        // 2. RENDER DEPTH OF SCENE TO TEXTURE FOR EACH CASCADE
        glm::mat4 lightView = glm::lookAt(dirLight->position, dirLight->position + glm::normalize(dirLight->direction), glm::vec3(0.0, 1.0, 0.0));
        for (unsigned int i = 0 ; i < NUM_CASCADES ; i++) {
            // Gen the proj and view matrix
            glm::mat4 proj = getOrthoProj(mShadowOrthoProjInfo[i]);
            //glm::mat4 proj = glm::ortho(mShadowOrthoProjInfo[i].l, mShadowOrthoProjInfo[i].r, mShadowOrthoProjInfo[i].b, 
            //                            mShadowOrthoProjInfo[i].t, mShadowOrthoProjInfo[i].n, mShadowOrthoProjInfo[i].f);
            mShadowMapProjs[i] = proj * lightView;
            // render the scene to the buffer
            glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mShadowMap[i], 0);
            glClear(GL_DEPTH_BUFFER_BIT);
            depthMappingShader.use();
            depthMappingShader.setMat4("lightSpaceMat", mShadowMapProjs[i]);
            // Render the textured objects
            for(auto& toRender: phongTexObjects) {
                depthMappingShader.setMat4("model", toRender->transform);
                // bind textures on corresponding texture units
                glBindVertexArray(toRender->VAO);
                glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
            }
            // Render the colored objects
            for(auto& toRender: phongClrObjects) {
                // material properties
                depthMappingShader.setMat4("model", toRender->transform);
                // bind textures on corresponding texture units
                glBindVertexArray(toRender->VAO);
                glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // RENDER DEPTH MAP FOR DIRECTIONAL LIGHT

        // render the scene to the buffer
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, (dirLight->depthMapFBO));
        glClear(GL_DEPTH_BUFFER_BIT);
        depthMappingShader.use();
        depthMappingShader.setMat4("lightSpaceMat", dirLight->spaceMatrix);
        // Render the textured objects
        for(auto& toRender: phongTexObjects) {
            depthMappingShader.setMat4("model", toRender->transform);
            // bind textures on corresponding texture units
            glBindVertexArray(toRender->VAO);
            glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
        }
        // Render the colored objects
        for(auto& toRender: phongClrObjects) {
            // material properties
            depthMappingShader.setMat4("model", toRender->transform);
            // bind textures on corresponding texture units
            glBindVertexArray(toRender->VAO);
            glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // reset viewport
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //glCullFace(GL_BACK);

        // 2. render scene as normal using the generated depth/shadow map  
        // --------------------------------------------------------------
        if (!showCascade){
            dirLightTexShader.use();
            // light properties
            dirLightTexShader.setVec3("light.direction", dirLight->direction);
            dirLightTexShader.setVec3("light.position",  dirLight->position);
            dirLightTexShader.setVec3("light.ambient", dirLight->ambient);
            dirLightTexShader.setVec3("light.diffuse", dirLight->diffuse);
            dirLightTexShader.setVec3("light.specular", dirLight->specular);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, mShadowMap[0]);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, mShadowMap[1]);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, mShadowMap[2]);
            dirLightTexShader.setFloat("cascadeEndClipSpace[0]", mCascadeEndClipSpace[0]);
            dirLightTexShader.setFloat("cascadeEndClipSpace[1]", mCascadeEndClipSpace[1]);
            dirLightTexShader.setFloat("cascadeEndClipSpace[2]", mCascadeEndClipSpace[2]);
            // view/projection transformations
            dirLightTexShader.setVec3("viewPos", camera.Position);
            dirLightTexShader.setMat4("projection", projection);
            dirLightTexShader.setMat4("view", camera.GetViewMatrix());
            dirLightTexShader.setMat4("FragPosLP[0]", mShadowMapProjs[0]);
            dirLightTexShader.setMat4("FragPosLP[1]", mShadowMapProjs[1]);
            dirLightTexShader.setMat4("FragPosLP[2]", mShadowMapProjs[2]);
            for(auto& toRender: phongTexObjects) {
                // material properties
                dirLightTexShader.setVec3("material.ambient", toRender->ka);
                dirLightTexShader.setVec3("material.diffuse", toRender->kd);
                dirLightTexShader.setVec3("material.specular", toRender->ks); // specular lighting doesn't have full effect on this object's material
                dirLightTexShader.setFloat("material.shininess", toRender->shininess);
                dirLightTexShader.setMat4("model", toRender->transform);
                // bind textures on corresponding texture units
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, toRender->textureId);
                glBindVertexArray(toRender->VAO);
                glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
            }

            // be sure to activate shader when setting uniforms/drawing objects
            dirLightClrShader.use();
            // light properties
            dirLightClrShader.setVec3("light.direction", dirLight->direction);
            dirLightClrShader.setVec3("light.position", dirLight->position);
            dirLightClrShader.setVec3("light.ambient", dirLight->ambient);
            dirLightClrShader.setVec3("light.diffuse", dirLight->diffuse);
            dirLightClrShader.setVec3("light.specular", dirLight->specular);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mShadowMap[0]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, mShadowMap[1]);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, mShadowMap[2]);
            dirLightClrShader.setFloat("cascadeEndClipSpace[0]", mCascadeEndClipSpace[0]);
            dirLightClrShader.setFloat("cascadeEndClipSpace[1]", mCascadeEndClipSpace[1]);
            dirLightClrShader.setFloat("cascadeEndClipSpace[2]", mCascadeEndClipSpace[2]);
            // view/projection transformations
            dirLightClrShader.setVec3("viewPos", camera.Position);
            dirLightClrShader.setMat4("projection", projection);
            dirLightClrShader.setMat4("view", camera.GetViewMatrix());
            dirLightClrShader.setMat4("FragPosLP[0]", mShadowMapProjs[0]);
            dirLightClrShader.setMat4("FragPosLP[1]", mShadowMapProjs[1]);
            dirLightClrShader.setMat4("FragPosLP[2]", mShadowMapProjs[2]);
            for(auto& toRender: phongClrObjects) {
                // material properties
                dirLightClrShader.setVec3("material.ambient", toRender->ka);
                dirLightClrShader.setVec3("material.diffuse", toRender->kd);
                dirLightClrShader.setVec3("material.specular", toRender->ks); // specular lighting doesn't have full effect on this object's material
                dirLightClrShader.setFloat("material.shininess", toRender->shininess);
                dirLightClrShader.setVec3("color", toRender->color);
                dirLightClrShader.setMat4("model", toRender->transform);
                
                // bind textures on corresponding texture units
                glBindVertexArray(toRender->VAO);
                glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
            }
        }
        else {
            // debug render to show the cascade
            cascadeDebugTexShader.use();

            // light properties
            cascadeDebugTexShader.setVec3("light.direction", dirLight->direction);
            cascadeDebugTexShader.setVec3("light.position",  dirLight->position);
            cascadeDebugTexShader.setVec3("light.ambient", dirLight->ambient);
            cascadeDebugTexShader.setVec3("light.diffuse", dirLight->diffuse);
            cascadeDebugTexShader.setVec3("light.specular", dirLight->specular);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, mShadowMap[0]);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, mShadowMap[1]);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, mShadowMap[2]);
            cascadeDebugTexShader.setFloat("cascadeEndClipSpace[0]", mCascadeEndClipSpace[0]);
            cascadeDebugTexShader.setFloat("cascadeEndClipSpace[1]", mCascadeEndClipSpace[1]);
            cascadeDebugTexShader.setFloat("cascadeEndClipSpace[2]", mCascadeEndClipSpace[2]);

            // view/projection transformations
            cascadeDebugTexShader.setVec3("viewPos", camera.Position);
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, cameraProjInfo.zNear, cameraProjInfo.zFar);
            cascadeDebugTexShader.setMat4("projection", projection);
            cascadeDebugTexShader.setMat4("view", camera.GetViewMatrix());
            cascadeDebugTexShader.setMat4("FragPosLP[0]", mShadowMapProjs[0]);
            cascadeDebugTexShader.setMat4("FragPosLP[1]", mShadowMapProjs[1]);
            cascadeDebugTexShader.setMat4("FragPosLP[2]", mShadowMapProjs[2]);
            for(auto& toRender: phongTexObjects) {
                // material properties
                cascadeDebugTexShader.setVec3("material.ambient", toRender->ka);
                cascadeDebugTexShader.setVec3("material.diffuse", toRender->kd);
                cascadeDebugTexShader.setVec3("material.specular", toRender->ks); // specular lighting doesn't have full effect on this object's material
                cascadeDebugTexShader.setFloat("material.shininess", toRender->shininess);
                cascadeDebugTexShader.setMat4("model", toRender->transform);
                // bind textures on corresponding texture units
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, toRender->textureId);
                glBindVertexArray(toRender->VAO);
                glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
            }

            // be sure to activate shader when setting uniforms/drawing objects
            cascadeDebugClrShader.use();
            // light properties
            cascadeDebugClrShader.setVec3("light.direction", dirLight->direction);
            cascadeDebugClrShader.setVec3("light.position", dirLight->position);
            cascadeDebugClrShader.setVec3("light.ambient", dirLight->ambient);
            cascadeDebugClrShader.setVec3("light.diffuse", dirLight->diffuse);
            cascadeDebugClrShader.setVec3("light.specular", dirLight->specular);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mShadowMap[0]);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, mShadowMap[1]);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, mShadowMap[2]);
            cascadeDebugClrShader.setFloat("cascadeEndClipSpace[0]", mCascadeEndClipSpace[0]);
            cascadeDebugClrShader.setFloat("cascadeEndClipSpace[1]", mCascadeEndClipSpace[1]);
            cascadeDebugClrShader.setFloat("cascadeEndClipSpace[2]", mCascadeEndClipSpace[2]);
            // view/projection transformations
            cascadeDebugClrShader.setVec3("viewPos", camera.Position);
            cascadeDebugClrShader.setMat4("projection", projection);
            cascadeDebugClrShader.setMat4("view", camera.GetViewMatrix());
            cascadeDebugClrShader.setMat4("FragPosLP[0]", mShadowMapProjs[0]);
            cascadeDebugClrShader.setMat4("FragPosLP[1]", mShadowMapProjs[1]);
            cascadeDebugClrShader.setMat4("FragPosLP[2]", mShadowMapProjs[2]);
            for(auto& toRender: phongClrObjects) {
                // material properties
                cascadeDebugClrShader.setVec3("material.ambient", toRender->ka);
                cascadeDebugClrShader.setVec3("material.diffuse", toRender->kd);
                cascadeDebugClrShader.setVec3("material.specular", toRender->ks); // specular lighting doesn't have full effect on this object's material
                cascadeDebugClrShader.setFloat("material.shininess", toRender->shininess);
                cascadeDebugClrShader.setVec3("color", toRender->color);
                cascadeDebugClrShader.setMat4("model", toRender->transform);
                
                // bind textures on corresponding texture units
                glBindVertexArray(toRender->VAO);
                glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
            }
        }

        if (depthMapRendered > 0) {
            depthDebugShader.use();
            glActiveTexture(GL_TEXTURE0);
            switch (depthMapRendered)
            {
            case 1:
                glBindTexture(GL_TEXTURE_2D, mShadowMap[0]);
                break;
            case 2:
                glBindTexture(GL_TEXTURE_2D, mShadowMap[1]);
                break;
            case 3:
                glBindTexture(GL_TEXTURE_2D, mShadowMap[2]);
                break;
            
            default:
                glBindTexture(GL_TEXTURE_2D, dirLight->depthMap);
                break;
            }
            depthDebugShader.setBool("orthographic", true);
            depthDebugShader.setFloat("nearPlane", dirLight->nearPlane);
            depthDebugShader.setFloat("farPlane", dirLight->farPlane);
            glBindVertexArray(depthQuad->VAO);
            glDrawElements(GL_TRIANGLES, depthQuad->indexCount, GL_UNSIGNED_INT, 0);
        }

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    for(auto& toRender: phongTexObjects) {
        glDeleteVertexArrays(1, &toRender->VAO);
        glDeleteBuffers(1, &toRender->VBO);
    }
    for(auto& toRender: phongClrObjects) {
        glDeleteVertexArrays(1, &toRender->VAO);
        glDeleteBuffers(1, &toRender->VBO);
    }
    for(auto& toRender: coloredObjects) {
        glDeleteVertexArrays(1, &toRender->VAO);
        glDeleteBuffers(1, &toRender->VBO);
    }

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
        showCascade = false;
        depthMapRendered = 0;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        showCascade = true;
        depthMapRendered = 0;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        showCascade = false;
        depthMapRendered = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
        showCascade = false;
        depthMapRendered = 2;
    }
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
        showCascade = false;
        depthMapRendered = 3;
    }
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

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}

RenderObjectPtr createTexCube(string path, float texScale, glm::vec3 ka, glm::vec3 kd, glm::vec3 ks, float shnss)
{
    RenderObjectPtr cubeObject = make_shared<RenderObject>();
    unsigned int cubeEBO;
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, texScale,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, texScale, texScale,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, texScale, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, texScale,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, texScale, texScale,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, texScale, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 0.0f, texScale,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f, texScale, texScale,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, texScale, 0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 0.0f, texScale,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f, texScale, texScale,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f, texScale, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 0.0f, texScale,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, texScale, texScale,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, texScale, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 0.0f, texScale,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, texScale, texScale,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, texScale, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f
    };
    unsigned int indices[] = {
        0, 2, 1,     2, 0, 3,
        4, 5, 6,     6, 7, 4,
        8, 9, 10,    10, 11, 8,
        12, 14, 13,  14, 12, 15,
        16, 17, 18,  18, 19, 16,
        20, 22, 21,  22, 20, 23        
    };
    glGenVertexArrays(1, &cubeObject->VAO);
    glGenBuffers(1, &cubeObject->VBO);
    glGenBuffers(1, &cubeEBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(cubeObject->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeObject->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texture coord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    // load and create a texture 
    cubeObject->textureId = loadTexture(getPath(path).string().c_str(), false);
    cubeObject->transform = glm::mat4(1.0f);
    cubeObject->indexCount = 36;
    cubeObject->ka = ka;
    cubeObject->kd = kd;
    cubeObject->ks = ks;
    cubeObject->shininess = shnss;
    return cubeObject;
}

RenderObjectPtr createClrCube(glm::vec3 color, glm::vec3 ka, glm::vec3 kd, glm::vec3 ks, float shnss)
{
    RenderObjectPtr cubeObject = make_shared<RenderObject>();
    unsigned int cubeEBO;
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
    };
    unsigned int indices[] = {
        0, 2, 1,     2, 0, 3,
        4, 5, 6,     6, 7, 4,
        8, 9, 10,    10, 11, 8,
        12, 14, 13,  14, 12, 15,
        16, 17, 18,  18, 19, 16,
        20, 22, 21,  22, 20, 23            
    };
    glGenVertexArrays(1, &cubeObject->VAO);
    glGenBuffers(1, &cubeObject->VBO);
    glGenBuffers(1, &cubeEBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(cubeObject->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeObject->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    cubeObject->transform = glm::mat4(1.0f);
    cubeObject->indexCount = 36;
    cubeObject->ka = ka;
    cubeObject->kd = kd;
    cubeObject->ks = ks;
    cubeObject->shininess = shnss;
    cubeObject->color = color;
    return cubeObject;
}

RenderObjectPtr createLightCube(glm::vec3 pos)
{
    RenderObjectPtr lightObject = make_shared<RenderObject>();;
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f
    };
    // first, configure the cube's VAO (and VBO)
    glGenVertexArrays(1, &lightObject->VAO);
    glGenBuffers(1, &lightObject->VBO);
    glBindBuffer(GL_ARRAY_BUFFER, lightObject->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindVertexArray(lightObject->VAO);
    // note that we update the lamp's position attribute's stride to reflect the updated buffer data
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    lightObject->indexCount = 36;
    lightObject->transform = glm::translate(glm::mat4(1.0f), pos);
    lightObject->transform = glm::scale(lightObject->transform, glm::vec3(0.2f)); // a smaller cube;
    lightObject->color = glm::vec3(1.0f);
    return lightObject;
}

RenderObjectPtr createTexQuad()
{
    RenderObjectPtr cubeObject = make_shared<RenderObject>();
    unsigned int cubeEBO;
    float vertices[] = {
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
    };
    unsigned int indices[] = {
        0, 1, 2,     2, 3, 0  
    };
    glGenVertexArrays(1, &cubeObject->VAO);
    glGenBuffers(1, &cubeObject->VBO);
    glGenBuffers(1, &cubeEBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(cubeObject->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeObject->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    cubeObject->transform = glm::mat4(1.0f);
    //cubeObject->transform = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f*(float)SCR_HEIGHT/(float)SCR_WIDTH, 2.0f, 1.0f));
    cubeObject->indexCount = 6;
    return cubeObject;
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

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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

void CalcOrthoProjs()
{
    glm::mat4 camView = camera.GetViewMatrix();
    glm::mat4 camT = glm::transpose(camView);
    glm::mat4 camInverse = glm::inverse(camT);

    glm::mat4 lightM =glm::lookAt(dirLight->position, dirLight->position + glm::normalize(dirLight->direction), glm::vec3(0.0, 1.0, 0.0));

    float ar = cameraProjInfo.height / cameraProjInfo.width;
    float tanHalfHFOV = glm::tan(glm::radians(cameraProjInfo.fov / 2.0f));
    float tanHalfVFOV = glm::tan(glm::radians((cameraProjInfo.fov * ar) / 2.0f));

    for (unsigned int i = 0 ; i < NUM_CASCADES ; i++) {
        float xn = mCascadeEnd[i]     * tanHalfHFOV;
        float xf = mCascadeEnd[i + 1] * tanHalfHFOV;
        float yn = mCascadeEnd[i]     * tanHalfVFOV;
        float yf = mCascadeEnd[i + 1] * tanHalfVFOV;

        glm::vec4 frustumCorners[8] = {
            // near face
            glm::vec4(xn,   yn, mCascadeEnd[i], 1.0),
            glm::vec4(-xn,  yn, mCascadeEnd[i], 1.0),
            glm::vec4(xn,  -yn, mCascadeEnd[i], 1.0),
            glm::vec4(-xn, -yn, mCascadeEnd[i], 1.0),

            // far face
            glm::vec4(xf,   yf, mCascadeEnd[i + 1], 1.0),
            glm::vec4(-xf,  yf, mCascadeEnd[i + 1], 1.0),
            glm::vec4(xf,  -yf, mCascadeEnd[i + 1], 1.0),
            glm::vec4(-xf, -yf, mCascadeEnd[i + 1], 1.0)
        };

        glm::vec4 frustumCornersL[8];
        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::min();
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::min();
        float minZ = std::numeric_limits<float>::max();
        float maxZ = std::numeric_limits<float>::min();

        for (unsigned j = 0 ; j < 8 ; j++) {
            glm::vec4 vW = camInverse * frustumCorners[j];
            frustumCornersL[j] = lightM * vW;
            minX = min(minX, frustumCornersL[j].x);
            maxX = max(maxX, frustumCornersL[j].x);
            minY = min(minY, frustumCornersL[j].y);
            maxY = max(maxY, frustumCornersL[j].y);
            minZ = min(minZ, frustumCornersL[j].z);
            maxZ = max(maxZ, frustumCornersL[j].z);
        }
        mShadowOrthoProjInfo[i].r = maxX;
        mShadowOrthoProjInfo[i].l = minX;
        mShadowOrthoProjInfo[i].b = minY;
        mShadowOrthoProjInfo[i].t = maxY;
        mShadowOrthoProjInfo[i].f = maxZ;
        mShadowOrthoProjInfo[i].n = minZ;
    }
}

glm::mat4 getOrthoProj(OrthoProjInfo& info)
{
    glm::mat4 proj(1.0f);
    proj[0][0] = 2.0f/(info.l - info.r);
    proj[1][1] = 2.0f/(info.t - info.b);
    proj[2][2] = 2.0f/(info.f - info.n);
    proj[3][0] = -(info.r + info.l)/(info.r - info.l);
    proj[3][1] = -(info.t + info.b)/(info.t - info.b);
    proj[3][2] = -(info.f + info.n)/(info.f - info.n);
    return proj;
}