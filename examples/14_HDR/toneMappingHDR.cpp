#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <vector>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

#include "shaders/shader.hpp"
#include "root_directory.h"
#include "cameras/cameraFirstPerson.hpp"
#include "performanceMonitor.hpp"

#include "imgui.h"
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_opengl3.h"

#include <iostream>

using namespace std;

// settings
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;

// camera
CameraFirstPerson camera(glm::vec3(0.0f, 1.5f, 0.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
bool lightsState[10] = {true, false, false, true, false, false, true, false, false, false};

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

struct Light {
    bool on;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

struct PointLight : Light {
    glm::vec3 position;
    // attenuation
    float constant;
    float linear;
    float quadratic;
};

struct DirectionalLight : Light {
    glm::vec3 direction;
};

struct SpotLight : Light {
    glm::vec3 position;
    glm::vec3 direction;
    float cutOff;
    float outerCutOff;
    // attenuation
    float constant;
    float linear;
    float quadratic;
};

enum ELightType {
    Point,
    Directional,
    Spot
};
ELightType currentLighting = ELightType::Point;

typedef shared_ptr<RenderObject> RenderObjectPtr;
typedef vector<RenderObjectPtr> RenderBatch;

typedef vector<shared_ptr<DirectionalLight>> DirectionalLights;
typedef vector<shared_ptr<PointLight>> PointLights;
typedef vector<shared_ptr<SpotLight>> SpotLights;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
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
RenderObjectPtr createLightCube();
RenderObjectPtr createLightPrism();
RenderObjectPtr createLightCylinder();
glm::mat4 rotateFromTo(glm::vec3 a, glm::vec3 b);

bool showMenu = false;

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
    string title = "HDR - Tone Mapping";
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
    glfwSetMouseButtonCallback(window, mouse_button_callback);
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
    Shader lightCubeShader(getPath("source/shaders/colorMVPShader.vs").string().c_str(), 
                           getPath("source/shaders/colorMVPShader.fs").string().c_str() );

    Shader* lightClrShader = new Shader();
    Shader* lightTexShader = new Shader();
     
    // Lights settings

    PointLights pointLights;

    shared_ptr<PointLight> pLight1 = make_shared<PointLight>();
    pLight1->position = glm::vec3(1.2f, 1.5f, 3.0f);
    pLight1->ambient = glm::vec3(0.0f);
    pLight1->diffuse = glm::vec3(1.0f);
    pLight1->specular = glm::vec3(1.0f);
    pLight1->constant = 1.0f;
    pLight1->linear = 0.09f;
    pLight1->quadratic = 0.032f;
    pointLights.push_back(pLight1);
    shared_ptr<PointLight> pLight2 = make_shared<PointLight>();
    pLight2->position = glm::vec3(2.2f, 0.7f, -3.0f);
    pLight2->ambient = glm::vec3(0.0f);
    pLight2->diffuse = glm::vec3(1.0f, 0.0f, 0.0f);
    pLight2->specular = glm::vec3(0.5f, 0.3f, 0.3f);
    pLight2->constant = 1.0f;
    pLight2->linear = 0.09f;
    pLight2->quadratic = 0.032f;
    pointLights.push_back(pLight2);
    shared_ptr<PointLight> pLight3 = make_shared<PointLight>();
    pLight3->position = glm::vec3(-2.5f, 3.5f, 0.0f);
    pLight3->ambient = glm::vec3(0.0f);
    pLight3->diffuse = glm::vec3(0.2f, 1.0f, 0.0f);
    pLight3->specular = glm::vec3(0.2f, 1.0f, 0.0f);
    pLight3->constant = 1.0f;
    pLight3->linear = 0.09f;
    pLight3->quadratic = 0.032f;
    pointLights.push_back(pLight3);


    DirectionalLights dirLights;

    shared_ptr<DirectionalLight> dLight1 = make_shared<DirectionalLight>();
    dLight1->direction = glm::vec3(1.0f, -1.0f, 0.0f);
    dLight1->ambient = glm::vec3(0.3f);
    dLight1->diffuse = glm::vec3(1.0f);
    dLight1->specular = glm::vec3(1.0f);
    dirLights.push_back(dLight1);
    shared_ptr<DirectionalLight> dLight2 = make_shared<DirectionalLight>();
    dLight2->direction = glm::vec3(0.0f, -1.0f, 0.0f);
    dLight2->ambient = glm::vec3(0.2f, 0.15f, 0.0f);
    dLight2->diffuse = glm::vec3(0.8f, 0.6f, 0.0f);
    dLight2->specular = glm::vec3(0.8f, 0.6f, 0.0f);
    dirLights.push_back(dLight2);
    shared_ptr<DirectionalLight> dLight3 = make_shared<DirectionalLight>();
    dLight3->direction = glm::vec3(-0.2f, -0.6f, 0.5f);
    dLight3->ambient = glm::vec3(0.07f, 0.07f, 0.1f);
    dLight3->diffuse = glm::vec3(0.4f, 0.2f, 0.6f);
    dLight3->specular = glm::vec3(0.1f, 0.1f, 0.15f);
    dirLights.push_back(dLight3);


    SpotLights spotLights;

    shared_ptr<SpotLight> sLight1 = make_shared<SpotLight>();
    sLight1->position = glm::vec3(4.0f, 3.0f, 0.0f);
    sLight1->direction = glm::vec3(1.0f, -1.0f, 0.0f);
    sLight1->cutOff = glm::cos(glm::radians(10.5f));
    sLight1->outerCutOff = glm::cos(glm::radians(15.5f));
    sLight1->ambient = glm::vec3(0.0f);
    sLight1->diffuse = glm::vec3(1.0f, 0.7f, 0.7f);
    sLight1->specular = glm::vec3(0.1f, 0.07f, 0.07f);
    sLight1->constant = 1.0f;
    sLight1->linear = 0.09f;
    sLight1->quadratic = 0.032f;
    spotLights.push_back(sLight1);
    shared_ptr<SpotLight> sLight2 = make_shared<SpotLight>();
    sLight2->position = glm::vec3(-3.5f, 3.5f, -3.5f);
    sLight2->direction = glm::vec3(1.0f, -1.0f, 1.0f);
    sLight2->cutOff = glm::cos(glm::radians(25.5f));
    sLight2->outerCutOff = glm::cos(glm::radians(30.5f));
    sLight2->ambient = glm::vec3(0.0f);
    sLight2->diffuse = glm::vec3(1.0f, 1.0f, 0.0f);
    sLight2->specular = glm::vec3(1.0f, 1.0f, 0.0f);
    sLight2->constant = 0.5f;
    sLight2->linear = 0.03f;
    sLight2->quadratic = 0.005f;
    spotLights.push_back(sLight2);
    shared_ptr<SpotLight> sLight3 = make_shared<SpotLight>();
    sLight3->position = glm::vec3(5.0f, 6.5f, -4.0f);
    sLight3->direction = glm::vec3(0.0f, -1.0f, 0.0f);
    sLight3->cutOff = glm::cos(glm::radians(16.0f));
    sLight3->outerCutOff = glm::cos(glm::radians(20.0f));
    sLight3->ambient = glm::vec3(0.0f);
    sLight3->diffuse = glm::vec3(0.0f, 0.5f, 1.0f);
    sLight3->specular = glm::vec3(0.0f, 1.0f, 1.0f);
    sLight3->constant = 1.0f;
    sLight3->linear = 0.09f;
    sLight3->quadratic = 0.032f;
    spotLights.push_back(sLight3);
    shared_ptr<SpotLight> sLight4 = make_shared<SpotLight>();
    sLight4->position = glm::vec3(1.0f);
    sLight4->direction = glm::vec3(0.0f, -1.0f, 0.0f);
    sLight4->cutOff = glm::cos(glm::radians(12.5f));
    sLight4->outerCutOff = glm::cos(glm::radians(17.5f));
    sLight4->ambient = glm::vec3(0.0f);
    sLight4->diffuse = glm::vec3(1.0f);
    sLight4->specular = glm::vec3(1.0f);
    sLight4->constant = 0.5f;
    sLight4->linear = 0.02f;
    sLight4->quadratic = 0.002f;
    spotLights.push_back(sLight4);

    // Init the shaders with the exact number of lights
    lightClrShader->StartUp(getPath("source/shaders/MultipleLightClrShader.vs").string().c_str(), 
                               getPath("source/shaders/MultipleLightClrShader.fs").string().c_str(),
                               dirLights.size(), pointLights.size(), spotLights.size());
    lightTexShader->StartUp(getPath("source/shaders/MultipleLightTexShader.vs").string().c_str(), 
                               getPath("source/shaders/MultipleLightTexShader.fs").string().c_str(),
                               dirLights.size(), pointLights.size(), spotLights.size());    

    // Render batches
    RenderBatch phongTexObjects;
    RenderBatch phongClrObjects;

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

    // Phong colored objects
    RenderObjectPtr wall = createClrCube(glm::vec3(1.0f, 0.5f, 0.0f));
    wall->transform = glm::translate(wall->transform, glm::vec3(16.0f, 0.0f, 0.0f));
    wall->transform = glm::scale(wall->transform, glm::vec3(16.0f));
    phongClrObjects.push_back(wall);
    RenderObjectPtr jumpingBox = createClrCube(glm::vec3(0.3f, 0.0f, 1.0f));
    phongClrObjects.push_back(jumpingBox);
    RenderObjectPtr rotBox = createClrCube(glm::vec3(0.2f, 1.0f, 0.0f));
    phongClrObjects.push_back(rotBox);

    // light Objects
    RenderObjectPtr lightCube = createLightCube();
    RenderObjectPtr LightPrism = createLightPrism();
    RenderObjectPtr lightCylinder = createLightCylinder();

    PerformanceMonitor pMonitor(glfwGetTime(), 0.5f);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

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
        
        spotLights[3]->position = camera.Position +
                                    camera.Right * 0.5f +
                                    camera.Front * 1.5f + 
                                    camera.Up * -0.5f;
        spotLights[3]->direction = camera.Front;


        // be sure to activate shader when setting uniforms/drawing objects
        lightTexShader->use();
        
        // view/projection transformations
        lightTexShader->setVec3("viewPos", camera.Position);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        lightTexShader->setMat4("projection", projection);
        lightTexShader->setMat4("view", camera.GetViewMatrix());

        // Directional Lights
        for (int i = 0; i < dirLights.size(); i++) {
            string number = to_string(i);
            lightTexShader->setVec3("dirLights["+ number + "].direction", dirLights[i]->direction);
            lightTexShader->setVec3("dirLights["+ number + "].ambient", dirLights[i]->ambient);
            lightTexShader->setVec3("dirLights["+ number + "].diffuse", dirLights[i]->diffuse);
            lightTexShader->setVec3("dirLights["+ number + "].specular", dirLights[i]->specular);
            lightTexShader->setBool("dirLights["+ number + "].on", lightsState[i]);
        }
        // Point Lights
        for (int i = 0; i < pointLights.size(); i++) {
            string number = to_string(i);
            lightTexShader->setVec3("pointLights["+ number + "].position", pointLights[i]->position);
            lightTexShader->setVec3("pointLights["+ number + "].ambient", pointLights[i]->ambient);
            lightTexShader->setVec3("pointLights["+ number + "].diffuse", pointLights[i]->diffuse);
            lightTexShader->setVec3("pointLights["+ number + "].specular", pointLights[i]->specular);
            lightTexShader->setFloat("pointLights["+ number + "].constant", pointLights[i]->constant);
            lightTexShader->setFloat("pointLights["+ number + "].linear", pointLights[i]->linear);
            lightTexShader->setFloat("pointLights["+ number + "].quadratic", pointLights[i]->quadratic);
            lightTexShader->setBool("pointLights["+ number + "].on", lightsState[i+3]);
        }
        // Spot Lights
        for (int i = 0; i < spotLights.size(); i++) {
            string number = to_string(i);
            lightTexShader->setVec3("spotLights["+ number + "].position", spotLights[i]->position);
            lightTexShader->setVec3("spotLights["+ number + "].direction", spotLights[i]->direction);
            lightTexShader->setVec3("spotLights["+ number + "].ambient", spotLights[i]->ambient);
            lightTexShader->setVec3("spotLights["+ number + "].diffuse", spotLights[i]->diffuse);
            lightTexShader->setVec3("spotLights["+ number + "].specular", spotLights[i]->specular);
            lightTexShader->setFloat("spotLights["+ number + "].cutOff", spotLights[i]->cutOff);
            lightTexShader->setFloat("spotLights["+ number + "].outerCutOff", spotLights[i]->outerCutOff);
            lightTexShader->setFloat("spotLights["+ number + "].constant", spotLights[i]->constant);
            lightTexShader->setFloat("spotLights["+ number + "].linear", spotLights[i]->linear);
            lightTexShader->setFloat("spotLights["+ number + "].quadratic", spotLights[i]->quadratic);
            lightTexShader->setBool("spotLights["+ number + "].on", lightsState[i+6]);	
        }
        // Render Textured Objects
        for(auto& toRender: phongTexObjects) {
            // material properties
            lightTexShader->setVec3("material.ambient", toRender->ka);
            lightTexShader->setVec3("material.diffuse", toRender->kd);
            lightTexShader->setVec3("material.specular", toRender->ks);
            lightTexShader->setFloat("material.shininess", toRender->shininess);
            lightTexShader->setMat4("model", toRender->transform);
            // bind textures on corresponding texture units
            glBindTexture(GL_TEXTURE_2D, toRender->textureId);
            glBindVertexArray(toRender->VAO);
            glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
        }

        // be sure to activate shader when setting uniforms/drawing objects
        lightClrShader->use();

        // view/projection transformations
        lightClrShader->setVec3("viewPos", camera.Position);
        lightClrShader->setMat4("projection", projection);
        lightClrShader->setMat4("view", camera.GetViewMatrix());

        // Directional Lights
        for (int i = 0; i < dirLights.size(); i++) {
            string number = to_string(i);
            lightClrShader->setVec3("dirLights["+ number + "].direction", dirLights[i]->direction);
            lightClrShader->setVec3("dirLights["+ number + "].ambient", dirLights[i]->ambient);
            lightClrShader->setVec3("dirLights["+ number + "].diffuse", dirLights[i]->diffuse);
            lightClrShader->setVec3("dirLights["+ number + "].specular", dirLights[i]->specular);
            lightClrShader->setBool("dirLights["+ number + "].on", lightsState[i]);
        }
        // Point Lights
        for (int i = 0; i < pointLights.size(); i++) {
            string number = to_string(i);
            lightClrShader->setVec3("pointLights["+ number + "].position", pointLights[i]->position);
            lightClrShader->setVec3("pointLights["+ number + "].ambient", pointLights[i]->ambient);
            lightClrShader->setVec3("pointLights["+ number + "].diffuse", pointLights[i]->diffuse);
            lightClrShader->setVec3("pointLights["+ number + "].specular", pointLights[i]->specular);
            lightClrShader->setFloat("pointLights["+ number + "].constant", pointLights[i]->constant);
            lightClrShader->setFloat("pointLights["+ number + "].linear", pointLights[i]->linear);
            lightClrShader->setFloat("pointLights["+ number + "].quadratic", pointLights[i]->quadratic);
            lightClrShader->setBool("pointLights["+ number + "].on", lightsState[i+3]);
        }
        // Spot Lights
        for (int i = 0; i < spotLights.size(); i++) {
            string number = to_string(i);
            lightClrShader->setVec3("spotLights["+ number + "].position", spotLights[i]->position);
            lightClrShader->setVec3("spotLights["+ number + "].direction", spotLights[i]->direction);
            lightClrShader->setVec3("spotLights["+ number + "].ambient", spotLights[i]->ambient);
            lightClrShader->setVec3("spotLights["+ number + "].diffuse", spotLights[i]->diffuse);
            lightClrShader->setVec3("spotLights["+ number + "].specular", spotLights[i]->specular);
            lightClrShader->setFloat("spotLights["+ number + "].cutOff", spotLights[i]->cutOff);
            lightClrShader->setFloat("spotLights["+ number + "].outerCutOff", spotLights[i]->outerCutOff);
            lightClrShader->setFloat("spotLights["+ number + "].constant", spotLights[i]->constant);
            lightClrShader->setFloat("spotLights["+ number + "].linear", spotLights[i]->linear);
            lightClrShader->setFloat("spotLights["+ number + "].quadratic", spotLights[i]->quadratic);
            lightClrShader->setBool("spotLights["+ number + "].on", lightsState[i+6]);	
        }
        // Render Colored Objects
        for(auto& toRender: phongClrObjects) {
            // material properties
            lightClrShader->setVec3("material.ambient", toRender->ka);
            lightClrShader->setVec3("material.diffuse", toRender->kd);
            lightClrShader->setVec3("material.specular", toRender->ks); 
            lightClrShader->setFloat("material.shininess", toRender->shininess);
            lightClrShader->setVec3("color", toRender->color);
            lightClrShader->setMat4("model", toRender->transform);
            // bind textures on corresponding texture units
            glBindVertexArray(toRender->VAO);
            glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
        }

        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", camera.GetViewMatrix());
        int c = 0;
        for(auto& light: dirLights) {

            lightCubeShader.setVec3("Color", (lightsState[c])?light->diffuse:glm::vec3(0.0f));
            glm::mat4 lightTr = glm::translate(glm::mat4(1.0f), light->direction * -5.0f);
            lightTr = lightTr * rotateFromTo(light->direction, glm::vec3(0.0f, -1.0f, 0.0f));
            lightTr = glm::scale(lightTr, glm::vec3(0.3f, 0.6f, 0.3f));
            lightCubeShader.setMat4("model", lightTr);      
            glBindVertexArray(lightCylinder->VAO);
            glDrawElements(GL_TRIANGLES, lightCylinder->indexCount, GL_UNSIGNED_INT, 0);
            c++;
        }
        c = 3;
        for(auto& light: pointLights) {

            lightCubeShader.setVec3("Color", (lightsState[c])?light->diffuse:glm::vec3(0.0f));
            glm::mat4 lightTr = glm::translate(glm::mat4(1.0f), light->position);
            lightTr = glm::scale(lightTr, glm::vec3(0.3f));
            lightCubeShader.setMat4("model", lightTr);      
            glBindVertexArray(lightCube->VAO);
            glDrawElements(GL_TRIANGLES, lightCube->indexCount, GL_UNSIGNED_INT, 0);
            c++;
        }
        c = 6;
        for(auto& light: spotLights) {

            lightCubeShader.setVec3("Color", (lightsState[c])?light->diffuse:glm::vec3(0.0f));
            glm::mat4 lightTr = glm::translate(glm::mat4(1.0f), light->position);
            lightTr = lightTr * rotateFromTo(light->direction, glm::vec3(0.0f, -1.0f, 0.0f));
            lightTr = glm::scale(lightTr, glm::vec3(0.3f, 0.6f, 0.3f));
            lightCubeShader.setMat4("model", lightTr);      
            glBindVertexArray(LightPrism->VAO);
            glDrawElements(GL_TRIANGLES, LightPrism->indexCount, GL_UNSIGNED_INT, 0);
            c++;
        }

        // Our state
        bool show_demo_window = true;
        bool show_another_window = false;
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        static float f = 0.0f;
        if (showMenu)
        {
            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            static int counter = 0;

            ImGui::Begin("HDR Menu");                      

            //ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            //ImGui::Checkbox("Demo Window", &show_demo_window); 

            ImGui::SliderFloat("Some value", &f, 0.0f, 1.0f);          
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            //if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when                       edited/activated)
            //    counter++;
            //ImGui::SameLine();
            //ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
    glDeleteVertexArrays(1, &lightCube->VAO);
    glDeleteBuffers(1, &lightCube->VBO);

    glDeleteVertexArrays(1, &LightPrism->VAO);
    glDeleteBuffers(1, &LightPrism->VBO);

    glDeleteVertexArrays(1, &lightCylinder->VAO);
    glDeleteBuffers(1, &lightCylinder->VBO);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

bool numberKeys[10] = {false, false, false, false, false, false, false, false, false, false};
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

    int c = 0;
    for (int key = GLFW_KEY_1; key <= GLFW_KEY_9; key++) {
        if (!numberKeys[c] && glfwGetKey(window, key) == GLFW_PRESS)
            lightsState[c] = !lightsState[c];
        numberKeys[c] = glfwGetKey(window, key) == GLFW_PRESS;
        c++;
    }
    if (!numberKeys[9] && glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS)
        lightsState[9] = !lightsState[9];
    numberKeys[9] = glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS;
}


void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (!showMenu && button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        showMenu = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && showMenu && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        showMenu = false;
        // tell GLFW to capture our mouse
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        firstMouse = true;
    }

    //if (!io.WantCaptureMouse)
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
    if (showMenu)
        return;

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
        0, 1, 2,     2, 3, 0,
        4, 5, 6,     6, 7, 4,
        8, 9, 10,    10, 11, 8,
        12, 13, 14,  14, 15, 12,
        16, 17, 18,  18, 19, 16,
        20, 21, 22,  22, 23, 20        
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
        0, 1, 2,     2, 3, 0,
        4, 5, 6,     6, 7, 4,
        8, 9, 10,    10, 11, 8,
        12, 13, 14,  14, 15, 12,
        16, 17, 18,  18, 19, 16,
        20, 21, 22,  22, 23, 20        
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

RenderObjectPtr createLightCube()
{
    RenderObjectPtr lightObject = make_shared<RenderObject>();
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
        0, 1, 2,     2, 3, 0,
        4, 5, 6,     6, 7, 4,
        8, 9, 10,    10, 11, 8,
        12, 13, 14,  14, 15, 12,
        16, 17, 18,  18, 19, 16,
        20, 21, 22,  22, 23, 20        
    };

    glGenVertexArrays(1, &lightObject->VAO);
    glGenBuffers(1, &lightObject->VBO);
    glGenBuffers(1, &cubeEBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(lightObject->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, lightObject->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // note that we update the lamp's position attribute's stride to reflect the updated buffer data
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    lightObject->indexCount = 36;
    lightObject->color = glm::vec3(1.0f);
    return lightObject;
}

RenderObjectPtr createLightPrism()
{
    RenderObjectPtr lightObject = make_shared<RenderObject>();
    unsigned int cubeEBO;
    float vertices[] = {
         0.0f,  0.5f,  0.0f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f,  -0.5f,  0.0f,  0.0f, -1.0f
    };
    unsigned int indices[] = {
        1, 2, 0,
        2, 4, 0,
        4, 3, 0,
        3, 1, 0,
        1, 3, 4,   2, 1, 4
    };

    glGenVertexArrays(1, &lightObject->VAO);
    glGenBuffers(1, &lightObject->VBO);
    glGenBuffers(1, &cubeEBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(lightObject->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, lightObject->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // note that we update the lamp's position attribute's stride to reflect the updated buffer data
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    lightObject->indexCount = 18;
    lightObject->color = glm::vec3(1.0f);
    return lightObject;
}

RenderObjectPtr createLightCylinder()
{
    RenderObjectPtr lightObject = make_shared<RenderObject>();
    unsigned int cubeEBO;
    int segments = 32;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    float dTheta = 2*3.14159265358979323846f / segments;
    float v0[2*6] = { 0.0f, -0.5f, 0.0f, 0.0f, 0.0f, -1.0f,
                    0.0f, 0.5f, 0.0f, 0.0f, 0.0f,  1.0f
    };
    vertices.insert(vertices.end(), std::begin(v0), std::end(v0));

    for (unsigned int i = 0; i < segments+1; i++) {
        float tempX = 0.5f * glm::cos(i * dTheta);
        float tempY = 0.5f * glm::sin(i * dTheta);
        
        glm::vec3 sideNormal= {glm::cos(i * dTheta), glm::sin(i * dTheta), 0};
        glm::vec3 lowerNormal = (sideNormal + glm::vec3(0, 0, -1))/2.0f;
        glm::vec3 upperNormal = (sideNormal + glm::vec3(0, 0,  1))/2.0f;
        float v[4*6] = {tempX, -0.5, tempY, lowerNormal.x, lowerNormal.y, lowerNormal.z,
                        tempX,  0.5, tempY, upperNormal.x, upperNormal.y, upperNormal.z,
                        tempX, -0.5, tempY, lowerNormal.x, lowerNormal.y, lowerNormal.z,
                        tempX,  0.5, tempY, upperNormal.x, upperNormal.y, upperNormal.z,
            };
        vertices.insert(vertices.end(), std::begin(v), std::end(v));

        if (i != segments) {
            unsigned ind[4*3] = {0    , 4*i+2, 4*i+6,
                                    1    , 4*i+3, 4*i+7,
                                    4*i+4, 4*i+8, 4*i+9,
                                    4*i+9, 4*i+5, 4*i+4
            };
            indices.insert(indices.end(), std::begin(ind), std::end(ind));
        }
    }
        
    float* vert = &vertices[0];
    unsigned int* indi = &indices[0];

    glGenVertexArrays(1, &lightObject->VAO);
    glGenBuffers(1, &lightObject->VBO);
    glGenBuffers(1, &cubeEBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(lightObject->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, lightObject->VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size()*sizeof(float), vert, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size()*sizeof(unsigned int), indi, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // note that we update the lamp's position attribute's stride to reflect the updated buffer data
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    lightObject->indexCount = indices.size();
    lightObject->color = glm::vec3(1.0f);
    return lightObject;
}

glm::mat4 rotateFromTo(glm::vec3 a, glm::vec3 b)
{
    glm::vec3 v = glm::cross(b, a);
    float angle = acos(glm::dot(b, a) / (glm::length(b) * glm::length(a)));
    glm::mat4 rotmat = glm::rotate(angle, v);

        // special cases lead to NaN values in the rotation matrix
    if (glm::any(glm::isnan(rotmat * glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)))) {
        if (angle < 0.1f) {
            rotmat = glm::mat4(1.0f);
        }
        else if (angle > 3.1f) {
            // rotate about any perpendicular vector
            rotmat = glm::rotate(angle, glm::cross(b,
                                        glm::vec3(b.y, b.z, b.x)));
        }
        else {
            assert(false);
        }
    }

    return rotmat;
}