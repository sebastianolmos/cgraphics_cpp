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

using namespace std;

// settings
const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 1024;

// camera
CameraFirstPerson camera(glm::vec3(0.0f, 0.1f, 0.0f));
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

struct PointLight {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    // attenuation
    float constant;
    float linear;
    float quadratic;
    // Shadows
    unsigned int depthMap;
    unsigned int depthMapFBO;
    glm::vec3 direction;
    glm::mat4 spaceMatrix;
    glm::mat4 projection;
    glm::mat4 view;
    float nearPlane;
    float farPlane;
    float fov;
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

struct SpotLight {
    glm::vec3 position;
    glm::vec3 direction;
    float cutOff;
    float outerCutOff;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    // attenuation
    float constant;
    float linear;
    float quadratic;
    // Shadows
    unsigned int depthMap;
    unsigned int depthMapFBO;
    glm::mat4 spaceMatrix;
    glm::mat4 projection;
    glm::mat4 view;
    float nearPlane;
    float farPlane;
};

enum ELightType {
    Point,
    Directional,
    Spot
};
ELightType currentLighting = ELightType::Point;

enum EDepthMap {
    None,
    Ortho,
    Projection
};
EDepthMap currentDepthMap = EDepthMap::None;

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

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    string title = "Single Lighting Shadow";
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
    // ------------------------------------
    Shader pointLightTexShader(getPath("source/shaders/PointLightShadowTexShader.vs").string().c_str(), 
                               getPath("source/shaders/PointLightShadowTexShader.fs").string().c_str() );
    Shader pointLightClrShader(getPath("source/shaders/PointLightShadowClrShader.vs").string().c_str(), 
                               getPath("source/shaders/PointLightShadowClrShader.fs").string().c_str() );
    Shader dirLightTexShader(getPath("source/shaders/DirLightShadowTexShader.vs").string().c_str(), 
                               getPath("source/shaders/DirLightShadowTexShader.fs").string().c_str() );
    Shader dirLightClrShader(getPath("source/shaders/DirLightShadowClrShader.vs").string().c_str(), 
                               getPath("source/shaders/DirLightShadowClrShader.fs").string().c_str() );
    Shader spotLightTexShader(getPath("source/shaders/SpotLightShadowTexShader.vs").string().c_str(), 
                               getPath("source/shaders/SpotLightShadowTexShader.fs").string().c_str() );
    Shader spotLightClrShader(getPath("source/shaders/SpotLightShadowClrShader.vs").string().c_str(), 
                               getPath("source/shaders/SpotLightShadowClrShader.fs").string().c_str() );
    Shader lightCubeShader(getPath("source/shaders/colorMVPShader.vs").string().c_str(), 
                           getPath("source/shaders/colorMVPShader.fs").string().c_str() );
    Shader depthMappingShader(getPath("source/shaders/ShadowMapDepthShader.vs").string().c_str(), 
                           getPath("source/shaders/ShadowMapDepthShader.fs").string().c_str() );
    Shader depthDebugShader(getPath("source/shaders/depthMapping.vs").string().c_str(), 
                           getPath("source/shaders/depthMapping.fs").string().c_str() );
    Shader* currentLightTexShader = nullptr;
    Shader* currentLightClrShader = nullptr;

    // shader configuration
    // --------------------
    depthDebugShader.use();
    depthDebugShader.setInt("depthMap", 0);

    pointLightTexShader.use();
    pointLightTexShader.setInt("texture_diffuse0", 0);
    pointLightTexShader.setInt("shadowMap", 1);
    
    pointLightClrShader.use();
    pointLightClrShader.setInt("shadowMap", 0);

    dirLightTexShader.use();
    dirLightTexShader.setInt("texture_diffuse0", 0);
    dirLightTexShader.setInt("shadowMap", 1);

    dirLightClrShader.use();
    dirLightClrShader.setInt("shadowMap", 0);

    spotLightTexShader.use();
    spotLightTexShader.setInt("texture_diffuse0", 0);
    spotLightTexShader.setInt("shadowMap", 1);

    spotLightClrShader.use();
    spotLightClrShader.setInt("shadowMap", 0);
     
    // Lights settings
    PointLight* pointLight = new PointLight;
    pointLight->position = glm::vec3(1.2f, 1.2f, 1.0f);
    pointLight->ambient = glm::vec3(0.5f);
    pointLight->diffuse = glm::vec3(1.0f);
    pointLight->specular = glm::vec3(1.0f);
    pointLight->constant = 1.0f;
    pointLight->linear = 0.09f;
    pointLight->quadratic = 0.032f;
    pointLight->nearPlane = 0.1f;
    pointLight->farPlane = 40.0f;
    pointLight->fov = 95.0f;
    pointLight->projection = glm::perspective(glm::radians(pointLight->fov), 1.0f, pointLight->nearPlane, pointLight->farPlane);

    DirectionalLight* dirLight = new DirectionalLight;
    dirLight->direction = glm::normalize(glm::vec3(1.0f, -1.0f, 0.5f));
    dirLight->ambient = glm::vec3(0.5f);
    dirLight->diffuse = glm::vec3(1.0f);
    dirLight->specular = glm::vec3(1.0f);
    dirLight->nearPlane = 0.01f;
    dirLight->farPlane = 17.5f;
    dirLight->orthoDim = 10.0f;
    dirLight->projection = glm::ortho(-dirLight->orthoDim, dirLight->orthoDim, -dirLight->orthoDim, dirLight->orthoDim, dirLight->nearPlane, dirLight->farPlane);

    SpotLight* spotLight = new SpotLight;
    spotLight->position = glm::vec3(1.0f);
    spotLight->direction = glm::vec3(1.0f);
    spotLight->cutOff = glm::cos(glm::radians(15.0f));
    spotLight->outerCutOff = glm::cos(glm::radians(20.0f));
    spotLight->ambient = glm::vec3(0.1f);
    spotLight->diffuse = glm::vec3(1.0f);
    spotLight->specular = glm::vec3(1.0f);
    spotLight->constant = 1.0f;
    spotLight->linear = 0.09f;
    spotLight->quadratic = 0.032f;
    spotLight->nearPlane = 0.1f;
    spotLight->farPlane = 50.0f;
    spotLight->projection = glm::perspective(glm::radians(75.0f), 1.0f, spotLight->nearPlane, spotLight->farPlane);

    // Render batches
    RenderBatch phongTexObjects;
    RenderBatch phongClrObjects;
    RenderBatch coloredObjects;

    // configure depth map FBO for each light type
    // -----------------------
    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

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
    // ------- FOR POINTLIGHT --------
    glGenFramebuffers(1, &(pointLight->depthMapFBO));
    glGenTextures(1, &(pointLight->depthMap)); // create depth texture
    glBindTexture(GL_TEXTURE_2D, (pointLight->depthMap));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    // attach depth texture as FBO's depth buffer
    glBindFramebuffer(GL_FRAMEBUFFER, (pointLight->depthMapFBO));
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, (pointLight->depthMap), 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // ------- FOR SPOTLIGHT --------
    glGenFramebuffers(1, &(spotLight->depthMapFBO));
    glGenTextures(1, &(spotLight->depthMap)); // create depth texture
    glBindTexture(GL_TEXTURE_2D, (spotLight->depthMap));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    // attach depth texture as FBO's depth buffer
    glBindFramebuffer(GL_FRAMEBUFFER, (spotLight->depthMapFBO));
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, (spotLight->depthMap), 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Phong textured objects
    RenderObjectPtr floor = createTexCube("assets/wood.png", 5.0f);
    floor->transform = glm::translate(floor->transform, glm::vec3(0.0f, -8.0f, 0.0f));
    floor->transform = glm::scale(floor->transform, glm::vec3(16.0f));
    phongTexObjects.push_back(floor);
    RenderObjectPtr box = createTexCube("assets/box.png", 1.0f);
    box->transform = glm::translate(box->transform, glm::vec3(5.0f, 2.0f, 4.0f));
    box->transform = glm::rotate(box->transform, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    box->transform = glm::scale(box->transform, glm::vec3(1.0f));
    phongTexObjects.push_back(box);
    RenderObjectPtr box2 = createTexCube("assets/box.png", 1.0f);
    box2->transform = glm::translate(box2->transform, glm::vec3(1.0f, 0.3501f, 3.0f));
    box2->transform = glm::scale(box2->transform, glm::vec3(0.7f));
    phongTexObjects.push_back(box2);

    // Phong colored objects
    RenderObjectPtr wall = createClrCube(glm::vec3(1.0f, 0.5f, 0.0f));
    wall->transform = glm::translate(wall->transform, glm::vec3(16.0f, 0.0f, 0.0f));
    wall->transform = glm::scale(wall->transform, glm::vec3(16.0f));
    phongClrObjects.push_back(wall);
    RenderObjectPtr jumpingBox = createClrCube(glm::vec3(0.3f, 0.0f, 1.0f));
    phongClrObjects.push_back(jumpingBox);
    RenderObjectPtr rotBox = createClrCube(glm::vec3(0.2f, 1.0f, 0.0f));
    phongClrObjects.push_back(rotBox);

    // Depth map quad object
    RenderObjectPtr depthQuad = createTexQuad();

    // light cube
    coloredObjects.push_back(createLightCube(pointLight->position));

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

        // refreshing the transforms
        jumpingBox->transform = glm::translate(glm::mat4(1.0f), 
                                    glm::vec3(0.0f, 
                                            glm::abs(glm::sin(glfwGetTime()*1.2f)*4.0f), 
                                            0.0f));
        jumpingBox->transform = glm::translate(jumpingBox->transform, glm::vec3(5.0f, 0.6f, -4.0f));
        jumpingBox->transform = glm::scale(jumpingBox->transform, glm::vec3(1.2f));
        rotBox->transform = glm::translate(glm::mat4(1.0f), glm::vec3(-4.0f, 2.5f, 0.0f));
        rotBox->transform = glm::rotate(rotBox->transform, (float)glfwGetTime()*1.0f, 
                                        glm::vec3(1.0f, 0.0f, 0.0f));
        rotBox->transform = glm::rotate(rotBox->transform, (float)glfwGetTime()*1.0f, 
                                        glm::vec3(0.0f, 1.0f, 0.0f));
        rotBox->transform = glm::rotate(rotBox->transform, (float)glfwGetTime()*1.0f, 
                                        glm::vec3(0.0f, 0.0f, 1.0f));
        rotBox->transform = glm::scale(rotBox->transform, glm::vec3(1.2f, 1.2f, 4.0f));

        switch (currentLighting)
        {
        case ELightType::Point:
            currentLightTexShader = &pointLightTexShader;
            currentLightClrShader = &pointLightClrShader;
            break;
        case ELightType::Directional:
            currentLightTexShader = &dirLightTexShader;
            currentLightClrShader = &dirLightClrShader;
            break;
        case ELightType::Spot:
            currentLightTexShader = &spotLightTexShader;
            currentLightClrShader = &spotLightClrShader;
            break;
        default:
            break;
        }
        
        float cameraNear = 0.1f;
        float cameraFar = 100.0f;
        // render
        // ------
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // 1. render depth of scene to texture (for each light type)
        // --------------------------------------------------------------
        //glCullFace(GL_FRONT);

        // RENDER DEPTH MAP FOR POINT LIGHT
        glm::vec3 goalPoint = camera.Position + camera.Front*( (cameraFar-cameraNear)/2.0f );
        pointLight->direction = glm::normalize( goalPoint - pointLight->position);
        pointLight->view = glm::lookAt(pointLight->position, pointLight->position + pointLight->direction, glm::vec3(0.0, 1.0, 0.0));
        pointLight->spaceMatrix = pointLight->projection * pointLight->view;
        // render the scene to the buffer
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, (pointLight->depthMapFBO));
        glClear(GL_DEPTH_BUFFER_BIT);
        depthMappingShader.use();
        depthMappingShader.setMat4("lightSpaceMat", pointLight->spaceMatrix);
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

        // RENDER DEPTH MAP FOR DIRECTIONAL LIGHT
        dirLight->position = dirLight->direction * -8.0f;
        dirLight->view = glm::lookAt(dirLight->position, dirLight->position + glm::normalize(dirLight->direction), glm::vec3(0.0, 1.0, 0.0));
        dirLight->spaceMatrix = dirLight->projection * dirLight->view;
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

        // RENDER DEPTH MAP FOR SPOT LIGHT
        spotLight->position = camera.Position + camera.Right*0.6f - camera.WorldUp*0.7f;
        spotLight->direction = camera.Front;
        spotLight->view = glm::lookAt(spotLight->position, spotLight->position + spotLight->direction, glm::vec3(0.0, 1.0, 0.0));
        spotLight->spaceMatrix = spotLight->projection * spotLight->view;
        // render the scene to the buffer
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, (spotLight->depthMapFBO));
        glClear(GL_DEPTH_BUFFER_BIT);
        depthMappingShader.use();
        depthMappingShader.setMat4("lightSpaceMat", spotLight->spaceMatrix);
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

        currentLightTexShader->use();
        // light properties
        if (currentLighting==ELightType::Point) {
            currentLightTexShader->setVec3("light.position", pointLight->position);
            currentLightTexShader->setMat4("lightSpaceMat", pointLight->spaceMatrix);
            currentLightTexShader->setVec3("light.ambient", pointLight->ambient);
            currentLightTexShader->setVec3("light.diffuse", pointLight->diffuse);
            currentLightTexShader->setVec3("light.specular", pointLight->specular);
            currentLightTexShader->setFloat("light.constant", pointLight->constant);
            currentLightTexShader->setFloat("light.linear", pointLight->linear);
            currentLightTexShader->setFloat("light.quadratic", pointLight->quadratic);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, pointLight->depthMap);
        }
        else if (currentLighting==ELightType::Directional)
        {
            currentLightTexShader->setVec3("light.direction", dirLight->direction);
            currentLightTexShader->setVec3("light.position",  dirLight->position);
            currentLightTexShader->setMat4("lightSpaceMat", dirLight->spaceMatrix);
            currentLightTexShader->setVec3("light.ambient", dirLight->ambient);
            currentLightTexShader->setVec3("light.diffuse", dirLight->diffuse);
            currentLightTexShader->setVec3("light.specular", dirLight->specular);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, dirLight->depthMap);
        }
        else if (currentLighting==ELightType::Spot)
        {
            currentLightTexShader->setVec3("light.position", spotLight->position);
            currentLightTexShader->setVec3("light.direction", spotLight->direction);
            currentLightTexShader->setMat4("lightSpaceMat", spotLight->spaceMatrix);
            currentLightTexShader->setVec3("light.ambient", spotLight->ambient);
            currentLightTexShader->setVec3("light.diffuse", spotLight->diffuse);
            currentLightTexShader->setVec3("light.specular", spotLight->specular);
            currentLightTexShader->setFloat("light.cutOff", spotLight->cutOff);
            currentLightTexShader->setFloat("light.outerCutOff", spotLight->outerCutOff);
            currentLightTexShader->setFloat("light.constant", spotLight->constant);
            currentLightTexShader->setFloat("light.linear", spotLight->linear);
            currentLightTexShader->setFloat("light.quadratic", spotLight->quadratic);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, spotLight->depthMap);	
        }
        // view/projection transformations
        currentLightTexShader->setVec3("viewPos", camera.Position);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, cameraNear, cameraFar);
        currentLightTexShader->setMat4("projection", projection);
        currentLightTexShader->setMat4("view", camera.GetViewMatrix());
        for(auto& toRender: phongTexObjects) {
            // material properties
            currentLightTexShader->setVec3("material.ambient", toRender->ka);
            currentLightTexShader->setVec3("material.diffuse", toRender->kd);
            currentLightTexShader->setVec3("material.specular", toRender->ks); // specular lighting doesn't have full effect on this object's material
            currentLightTexShader->setFloat("material.shininess", toRender->shininess);
            currentLightTexShader->setMat4("model", toRender->transform);
            // bind textures on corresponding texture units
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, toRender->textureId);
            glBindVertexArray(toRender->VAO);
            glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
        }

        // be sure to activate shader when setting uniforms/drawing objects
        currentLightClrShader->use();
        // light properties
        if (currentLighting==ELightType::Point) {
            currentLightClrShader->setVec3("light.position", pointLight->position);
            currentLightClrShader->setMat4("lightSpaceMat", pointLight->spaceMatrix);
            currentLightClrShader->setVec3("light.ambient", pointLight->ambient);
            currentLightClrShader->setVec3("light.diffuse", pointLight->diffuse);
            currentLightClrShader->setVec3("light.specular", pointLight->specular);
            currentLightClrShader->setFloat("light.constant", pointLight->constant);
            currentLightClrShader->setFloat("light.linear", pointLight->linear);
            currentLightClrShader->setFloat("light.quadratic", pointLight->quadratic);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, pointLight->depthMap);
        }
        else if (currentLighting==ELightType::Directional)
        {
            currentLightClrShader->setVec3("light.direction", dirLight->direction);
            currentLightClrShader->setVec3("light.position", dirLight->position);
            currentLightClrShader->setMat4("lightSpaceMat", dirLight->spaceMatrix);
            currentLightClrShader->setVec3("light.ambient", dirLight->ambient);
            currentLightClrShader->setVec3("light.diffuse", dirLight->diffuse);
            currentLightClrShader->setVec3("light.specular", dirLight->specular);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, dirLight->depthMap);
        }
        else if (currentLighting==ELightType::Spot)
        {
            currentLightClrShader->setVec3("light.position", spotLight->position);
            currentLightClrShader->setVec3("light.direction", spotLight->direction);
            currentLightClrShader->setMat4("lightSpaceMat", spotLight->spaceMatrix);
            currentLightClrShader->setVec3("light.ambient", spotLight->ambient);
            currentLightClrShader->setVec3("light.diffuse", spotLight->diffuse);
            currentLightClrShader->setVec3("light.specular", spotLight->specular);
            currentLightClrShader->setFloat("light.cutOff", spotLight->cutOff);
            currentLightClrShader->setFloat("light.outerCutOff", spotLight->outerCutOff);
            currentLightClrShader->setFloat("light.constant", spotLight->constant);
            currentLightClrShader->setFloat("light.linear", spotLight->linear);
            currentLightClrShader->setFloat("light.quadratic", spotLight->quadratic);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, spotLight->depthMap);	
        }
        // view/projection transformations
        currentLightClrShader->setVec3("viewPos", camera.Position);
        currentLightClrShader->setMat4("projection", projection);
        currentLightClrShader->setMat4("view", camera.GetViewMatrix());
        for(auto& toRender: phongClrObjects) {
            // material properties
            currentLightClrShader->setVec3("material.ambient", toRender->ka);
            currentLightClrShader->setVec3("material.diffuse", toRender->kd);
            currentLightClrShader->setVec3("material.specular", toRender->ks); // specular lighting doesn't have full effect on this object's material
            currentLightClrShader->setFloat("material.shininess", toRender->shininess);
            currentLightClrShader->setVec3("color", toRender->color);
            currentLightClrShader->setMat4("model", toRender->transform);
            
            // bind textures on corresponding texture units
            glBindVertexArray(toRender->VAO);
            glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
        }

        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", camera.GetViewMatrix());
        for(auto& toRender: coloredObjects) {

            lightCubeShader.setVec3("Color", (currentLighting==ELightType::Point)?toRender->color:glm::vec3(0.0f));
            lightCubeShader.setMat4("model", toRender->transform);
            lightCubeShader.setMat4("model", toRender->transform);        
            glBindVertexArray(toRender->VAO);
            glDrawArrays(GL_TRIANGLES, 0, toRender->indexCount);
        }

        if (currentDepthMap != EDepthMap::None) {
            depthDebugShader.use();
            glActiveTexture(GL_TEXTURE0);
            if (currentLighting==ELightType::Point) 
            {
                glBindTexture(GL_TEXTURE_2D, pointLight->depthMap);
                depthDebugShader.setBool("orthographic", false);
                depthDebugShader.setFloat("nearPlane", pointLight->nearPlane);
                depthDebugShader.setFloat("farPlane", pointLight->farPlane);
            }
            else if (currentLighting==ELightType::Directional)
            {
                glBindTexture(GL_TEXTURE_2D, dirLight->depthMap);
                depthDebugShader.setBool("orthographic", true);
                depthDebugShader.setFloat("nearPlane", dirLight->nearPlane);
                depthDebugShader.setFloat("farPlane", dirLight->farPlane);
            }
            else if (currentLighting==ELightType::Spot) 
            {
                glBindTexture(GL_TEXTURE_2D, spotLight->depthMap);
                depthDebugShader.setBool("orthographic", false);
                depthDebugShader.setFloat("nearPlane", spotLight->nearPlane);
                depthDebugShader.setFloat("farPlane", spotLight->farPlane);
            }
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
        currentLighting = ELightType::Point;
        currentDepthMap = EDepthMap::None;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
        currentLighting = ELightType::Directional;
        currentDepthMap = EDepthMap::None;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
        currentLighting = ELightType::Spot;
        currentDepthMap = EDepthMap::None;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
        currentDepthMap = EDepthMap::Ortho;
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS)
        currentDepthMap = EDepthMap::Projection;
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
    glGenTextures(1, &cubeObject->textureId);
    glBindTexture(GL_TEXTURE_2D, cubeObject->textureId);
    // set the texture wrapping parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // load image, create texture and generate mipmaps
    int width, height, nrChannels;
    unsigned char* data = stbi_load(getPath(path).string().c_str(), &width, &height, &nrChannels, 0);
    if (data)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data);
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