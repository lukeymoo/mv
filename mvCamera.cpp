#include "mvCamera.h"

extern mv::LogHandler logger;

mv::Camera::Camera(struct CameraInitStruct &p_CameraInitStruct)
{
    fov = p_CameraInitStruct.fov;
    aspect = p_CameraInitStruct.aspect;
    nearz = p_CameraInitStruct.nearz;
    farz = p_CameraInitStruct.farz;
    position = p_CameraInitStruct.position;
    viewUniformObject = p_CameraInitStruct.viewUniformObject;
    projectionUniformObject = p_CameraInitStruct.projectionUniformObject;

    type = p_CameraInitStruct.type;

    // ensure matrices specified
    if (p_CameraInitStruct.viewUniformObject == nullptr)
    {
        throw std::runtime_error("No view matrix uniform object specified");
    }
    if (p_CameraInitStruct.projectionUniformObject == nullptr)
    {
        throw std::runtime_error("No projection matrix uniform object specified");
    }

    if (type == CameraType::eInvalid)
        throw std::runtime_error("No camera type specified in camera initialization structure");

    // ensure target is given if type is third person
    if (type == CameraType::eThirdPerson)
    {
        if (p_CameraInitStruct.target == nullptr)
            throw std::runtime_error("Camera type is specified as third person yet no target "
                                     "specified in initialization structure");
        target = p_CameraInitStruct.target;
    }

    ptrLogger = &logger;
    if (!ptrLogger)
        throw std::runtime_error("Failed to configure gui log handler");

    front = glm::vec3(0.0f, 0.0f, -1.0f);
    rotation = glm::vec3(0.01f, 0.01f, 0.01f);

    update();

    projectionUniformObject->matrix = glm::perspective(glm::radians(fov), aspect, nearz, farz);
    projectionUniformObject->matrix[1][1] *= -1.0f;
    // TODO
    // add non host visible/coherent update support
    // update projection matrix buffer
    memcpy(projectionUniformObject->mvBuffer.mapped, &projectionUniformObject->matrix,
           sizeof(projectionUniformObject->matrix));
}

mv::Camera::~Camera()
{
}