#ifndef CAMERA2D_H
#define CAMERA2D_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
enum Camera_Movement {
    UP,
    DOWN,
    LEFT,
    RIGHT
};

// Default camera values
const float SPEED = 2.5f;
const float ZOOM = 1.0f;

// Camera class
class Camera2D
{
public:
    // camera Attributes
    glm::vec2 Position;
    float MovementSpeed;
    float Zoom;
    bool Drag = false;
    glm::vec2 currentPos;
    glm::vec2 lastPos;

    //Constructor
    Camera2D(glm::vec2 position = glm::vec2(0.0f, 0.0f)) : MovementSpeed(SPEED), Zoom(ZOOM)
    {
        Position = position;
        currentPos = position;
        lastPos = position;
    }

    // returns the transform matrix
    glm::mat4 GetTransformMatrix()
    {
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::scale(transform, glm::vec3(Zoom, Zoom, 1));

        if (Drag)
        {
            glm::vec2 motion = Position + currentPos - lastPos;
            transform = glm::translate(transform, glm::vec3(motion.x, motion.y, 0.0f));
        }

        transform = glm::translate(transform, glm::vec3(-Position.x, -Position.y, 1.0f));
        return transform;

    }

    // returns Zoom value
    float GetZoom()
    {
        return Zoom;
    }

    void SetDrag(bool value)
    {
        if (!Drag && value)
        {
            lastPos = Position + currentPos;
        }

        if (Drag && !value)
        {
            Position = -(currentPos - lastPos);
        }

        Drag = value;
    }

    void SetCurrentPos(float xPos, float yPos)
    {
        currentPos.x = xPos;
        currentPos.y = yPos;
    }

    // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM 
    void ProcessKeyboard(Camera_Movement direction, float deltaTime)
    {
        float velocity = MovementSpeed * deltaTime;
        if (direction == UP)
            Position += glm::vec2(0.0f, 1.0f) * velocity;
        if (direction == DOWN)
            Position -= glm::vec2(0.0f, 1.0f) * velocity;
        if (direction == LEFT)
            Position -= glm::vec2(1.0f, 0.0f) * velocity;
        if (direction == RIGHT)
            Position += glm::vec2(1.0f, 0.0f) * velocity;
    }

    // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
    void ProcessMouseScroll(float yoffset)
    {
        Zoom += (float)yoffset * 0.1;
        if (Zoom < 0.2f)
            Zoom = 0.2f;
        if (Zoom > 20.0f)
            Zoom = 20.0f;
    }


};

#endif