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
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;

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
};

struct DirectionalLight {
    glm::vec3 direction;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
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
};

enum ELightType {
    Point,
    Directional,
    Spot
};
ELightType currentLighting = ELightType::Point;

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
    string title = "Single Lighting";
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
    Shader pointLightTexShader(getPath("source/shaders/PointLightTexturedShader.vs").string().c_str(), 
                               getPath("source/shaders/PointLightTexturedShader.fs").string().c_str() );
    Shader pointLightClrShader(getPath("source/shaders/PointLightColoredShader.vs").string().c_str(), 
                               getPath("source/shaders/PointLightColoredShader.fs").string().c_str() );
    Shader dirLightTexShader(getPath("source/shaders/DirLightTexturedShader.vs").string().c_str(), 
                               getPath("source/shaders/DirLightTexturedShader.fs").string().c_str() );
    Shader dirLightClrShader(getPath("source/shaders/DirLightColoredShader.vs").string().c_str(), 
                               getPath("source/shaders/DirLightColoredShader.fs").string().c_str() );
    Shader spotLightTexShader(getPath("source/shaders/SpotLightTexturedShader.vs").string().c_str(), 
                               getPath("source/shaders/SpotLightTexturedShader.fs").string().c_str() );
    Shader spotLightClrShader(getPath("source/shaders/SpotLightColoredShader.vs").string().c_str(), 
                               getPath("source/shaders/SpotLightColoredShader.fs").string().c_str() );
    Shader lightCubeShader(getPath("source/shaders/colorMVPShader.vs").string().c_str(), 
                           getPath("source/shaders/colorMVPShader.fs").string().c_str() );
    Shader* currentLightTexShader = nullptr;
    Shader* currentLightClrShader = nullptr;
     
    // Lights settings
    PointLight* pointLight = new PointLight;
    pointLight->position = glm::vec3(1.2f, 1.2f, 1.0f);
    pointLight->ambient = glm::vec3(0.5f);
    pointLight->diffuse = glm::vec3(1.0f);
    pointLight->specular = glm::vec3(1.0f);
    pointLight->constant = 1.0f;
    pointLight->linear = 0.09f;
    pointLight->quadratic = 0.032f;

    DirectionalLight* dirLight = new DirectionalLight;
    dirLight->direction = glm::vec3(1.0f, -1.0f, 0.0f);
    dirLight->ambient = glm::vec3(0.5f);
    dirLight->diffuse = glm::vec3(1.0f);
    dirLight->specular = glm::vec3(1.0f);

    SpotLight* spotLight = new SpotLight;
    spotLight->position = glm::vec3(1.0f);
    spotLight->direction = glm::vec3(1.0f);
    spotLight->cutOff = glm::cos(glm::radians(12.5f));
    spotLight->outerCutOff = glm::cos(glm::radians(17.5f));
    spotLight->ambient = glm::vec3(0.1f);
    spotLight->diffuse = glm::vec3(1.0f);
    spotLight->specular = glm::vec3(1.0f);
    spotLight->constant = 1.0f;
    spotLight->linear = 0.09f;
    spotLight->quadratic = 0.032f;

    // Render batches
    RenderBatch phongTexObjects;
    RenderBatch phongClrObjects;
    RenderBatch coloredObjects;

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

    // light cube
    coloredObjects.push_back(createLightCube(pointLight->position));

    PerformanceMonitor pMonitor(glfwGetTime(), 0.5f);

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
        // be sure to activate shader when setting uniforms/drawing objects
        currentLightTexShader->use();
        // light properties
        if (currentLighting==ELightType::Point) {
            currentLightTexShader->setVec3("light.position", pointLight->position);
            currentLightTexShader->setVec3("light.ambient", pointLight->ambient);
            currentLightTexShader->setVec3("light.diffuse", pointLight->diffuse);
            currentLightTexShader->setVec3("light.specular", pointLight->specular);
            currentLightTexShader->setFloat("light.constant", pointLight->constant);
            currentLightTexShader->setFloat("light.linear", pointLight->linear);
            currentLightTexShader->setFloat("light.quadratic", pointLight->quadratic);	
        }
        else if (currentLighting==ELightType::Directional)
        {
            currentLightTexShader->setVec3("light.direction", dirLight->direction);
            currentLightTexShader->setVec3("light.ambient", dirLight->ambient);
            currentLightTexShader->setVec3("light.diffuse", dirLight->diffuse);
            currentLightTexShader->setVec3("light.specular", dirLight->specular);
        }
        else if (currentLighting==ELightType::Spot)
        {
            spotLight->position = camera.Position;
            spotLight->direction = camera.Front;
            currentLightTexShader->setVec3("light.position", spotLight->position);
            currentLightTexShader->setVec3("light.direction", spotLight->direction);
            currentLightTexShader->setVec3("light.ambient", spotLight->ambient);
            currentLightTexShader->setVec3("light.diffuse", spotLight->diffuse);
            currentLightTexShader->setVec3("light.specular", spotLight->specular);
            currentLightTexShader->setFloat("light.cutOff", spotLight->cutOff);
            currentLightTexShader->setFloat("light.outerCutOff", spotLight->outerCutOff);
            currentLightTexShader->setFloat("light.constant", spotLight->constant);
            currentLightTexShader->setFloat("light.linear", spotLight->linear);
            currentLightTexShader->setFloat("light.quadratic", spotLight->quadratic);	
        }
        // view/projection transformations
        currentLightTexShader->setVec3("viewPos", camera.Position);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
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
            glBindTexture(GL_TEXTURE_2D, toRender->textureId);
            glBindVertexArray(toRender->VAO);
            glDrawElements(GL_TRIANGLES, toRender->indexCount, GL_UNSIGNED_INT, 0);
        }

        // be sure to activate shader when setting uniforms/drawing objects
        currentLightClrShader->use();
        // light properties
        if (currentLighting==ELightType::Point) {
            currentLightClrShader->setVec3("light.position", pointLight->position);
            currentLightClrShader->setVec3("light.ambient", pointLight->ambient);
            currentLightClrShader->setVec3("light.diffuse", pointLight->diffuse);
            currentLightClrShader->setVec3("light.specular", pointLight->specular);
            currentLightClrShader->setFloat("light.constant", pointLight->constant);
            currentLightClrShader->setFloat("light.linear", pointLight->linear);
            currentLightClrShader->setFloat("light.quadratic", pointLight->quadratic);	
        }
        else if (currentLighting==ELightType::Directional)
        {
            currentLightClrShader->setVec3("light.direction", dirLight->direction);
            currentLightClrShader->setVec3("light.ambient", dirLight->ambient);
            currentLightClrShader->setVec3("light.diffuse", dirLight->diffuse);
            currentLightClrShader->setVec3("light.specular", dirLight->specular);
        }
        else if (currentLighting==ELightType::Spot)
        {
            currentLightClrShader->setVec3("light.position", spotLight->position);
            currentLightClrShader->setVec3("light.direction", spotLight->direction);
            currentLightClrShader->setVec3("light.ambient", spotLight->ambient);
            currentLightClrShader->setVec3("light.diffuse", spotLight->diffuse);
            currentLightClrShader->setVec3("light.specular", spotLight->specular);
            currentLightClrShader->setFloat("light.cutOff", spotLight->cutOff);
            currentLightClrShader->setFloat("light.outerCutOff", spotLight->outerCutOff);
            currentLightClrShader->setFloat("light.constant", spotLight->constant);
            currentLightClrShader->setFloat("light.linear", spotLight->linear);
            currentLightClrShader->setFloat("light.quadratic", spotLight->quadratic);	
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

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        currentLighting = ELightType::Point;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        currentLighting = ELightType::Directional;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
        currentLighting = ELightType::Spot;
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

RenderObjectPtr createLightCube(glm::vec3 pos)
{
    RenderObjectPtr lightObject = make_shared<RenderObject>();;
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

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
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
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