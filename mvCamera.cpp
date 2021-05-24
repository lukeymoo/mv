#include "mvCamera.h"
#include "mvCollection.h"
#include "mvModel.h"

extern LogHandler logger;

Camera::Camera(CameraInitStruct &p_CameraInitStruct)
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

Camera::~Camera()
{
}

void Camera::update(void)
{
    if (type == CameraType::eThirdPerson || type == CameraType::eFreeLook)
    {
        /*
          Zoom Implementation
        */
        if (zoomAccel)
        {
            if (checkZoom(zoomAccel))
            {
                zoomLevel += zoomAccel;
            }
            else
            {
                if (zoomAccel < 0)
                {
                    zoomLevel = minZoomLevel;
                }
                else
                {
                    zoomLevel = maxZoomLevel;
                }
                zoomAccel = 0.0f;
            }
            // Zoom friction implementation
            if (zoomAccel > 0.0f)
            {
                zoomAccel -= zoomFriction;
            }
            else if (zoomAccel < 0.0f)
            {
                zoomAccel += zoomFriction;
            }

            // clamp
            if (fabs(zoomAccel) < 0.01f)
            {
                zoomAccel = 0.0f;
            }
        }
        /*
          Orbit Linear Interpolation
        */
        if (orbitAngle != targetOrbit)
        {
            orbitAngle = orbitAngle * (1 - orbitSmoothness) + targetOrbit * orbitSmoothness;
        }
        else
        {
            realignOrbit(orbitAngle);
            realignOrbit(targetOrbit);
        }
        /*
          Pitch Linear Interpolation
        */
        if (pitch != targetPitch)
        {
            pitch = pitch * (1 - pitchSmoothness) + targetPitch * pitchSmoothness;
        }
    }

    if (type == CameraType::eFreeLook)
    {
        updateFreelook();
    }
    else if (type == CameraType::eFirstPerson)
    {
        throw std::runtime_error("first person camera mode currently unsupported");
    }
    else if (type == CameraType::eThirdPerson)
    {
        updateThirdPerson();
    }

    // TODO
    // add support for non host coherent/host visible buffers
    // end of update() copy to view matrix ubo
    memcpy(viewUniformObject->mvBuffer.mapped, &viewUniformObject->matrix,
           sizeof(viewUniformObject->matrix));
    return;
}

void Camera::adjustMovement(glm::vec3 p_Delta)
{
    getFrontFace();
    moveAccel += front + p_Delta;
    return;

    float moveX = (moveAccel.x + p_Delta.x);
    float moveY = (moveAccel.y + p_Delta.y);
    float moveZ = (moveAccel.z + p_Delta.z);
    if (moveX < maxMoveAccel && moveX > -maxMoveAccel)
    {
        moveAccel.x = moveX;
    }
    else
    {
        if (moveAccel.x > 0)
        {
            moveAccel.x = maxMoveAccel;
        }
        else if (moveAccel.x < 0)
        {
            moveAccel.x = -maxMoveAccel;
        }
    }
    if (moveY < maxMoveAccel && moveY > -maxMoveAccel)
    {
        moveAccel.y = moveY;
    }
    else
    {
        if (moveAccel.y > 0)
        {
            moveAccel.y = maxMoveAccel;
        }
        else if (moveAccel.y < 0)
        {
            moveAccel.y = -maxMoveAccel;
        }
    }
    if (moveZ < maxMoveAccel && moveZ > -maxMoveAccel)
    {
        moveAccel.z = moveZ;
    }
    else
    {
        if (moveAccel.z > 0)
        {
            moveAccel.z = maxMoveAccel;
        }
        else if (moveAccel.z < 0)
        {
            moveAccel.z = -maxMoveAccel;
        }
    }
    return;
}

void Camera::adjustZoom(float p_Delta)
{
    zoomAccel += p_Delta;
    return;
}

void Camera::adjustPitch(float p_Delta, float p_StartPitch)
{
    pitch = p_StartPitch + p_Delta;
    if (pitch > 89.9f)
    {
        pitch = 89.9f;
    }
    else if (pitch < -89.9f)
    {
        pitch = -89.9f;
    }
    return;
}

void Camera::adjustPitch(float &p_Angle)
{
    if (p_Angle > 89.9f)
    {
        p_Angle = 89.9f;
    }
    else if (p_Angle < -89.9f)
    {
        p_Angle = -89.9f;
    }
    return;
}

void Camera::adjustOrbit(float p_Delta, float p_StartOrbit)
{
    constexpr float orbitStep = 0.125f;
    orbitAngle = p_StartOrbit + (orbitStep * p_Delta);
    return;
}
void Camera::adjustOrbit(float p_Delta)
{
    orbitAngle += p_Delta;
    return;
}

void Camera::lerpOrbit(float p_Delta)
{
    targetOrbit += p_Delta;
    // realign_orbit(target_orbit);
    return;
}
void Camera::lerpPitch(float p_Delta)
{
    targetPitch += p_Delta;
    adjustPitch(targetPitch);
    return;
}

void Camera::realignOrbit(void)
{
    if (orbitAngle > 359.9f)
    {
        orbitAngle = abs(orbitAngle) - 359.9f;
    }
    else if (orbitAngle < 0.0f)
    {
        orbitAngle = 359.9f - abs(orbitAngle);
    }
    return;
}
void Camera::realignOrbit(float &p_Angle)
{
    if (p_Angle > 359.9f)
    {
        p_Angle = abs(p_Angle) - 359.9f;
    }
    else if (p_Angle < 0.0f)
    {
        p_Angle = 359.9f - abs(p_Angle);
    }
    return;
}

bool Camera::checkZoom(float p_Delta)
{
    if (p_Delta > 0) // zooming out
    {
        if ((zoomLevel + p_Delta) < maxZoomLevel)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else // zooming in
    {
        if ((zoomLevel + p_Delta) > minZoomLevel)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}
void Camera::getFrontFace(void)
{
    glm::vec3 fr;
    fr.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
    fr.y = sin(glm::radians(rotation.x));
    fr.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
    fr = glm::normalize(fr);

    front = fr;
    return;
}

void Camera::getFrontFace(float p_OrbitAngle)
{
    glm::vec3 fr;
    fr.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(p_OrbitAngle));
    fr.y = sin(glm::radians(rotation.x));
    fr.z = cos(glm::radians(rotation.x)) * cos(glm::radians(p_OrbitAngle));
    fr = glm::normalize(fr);

    front = fr;
    return;
}

void Camera::move(float p_Angle, glm::vec3 p_TargetAxii, bool p_Boost)
{
    constexpr float newMoveSpeed = MOVESPEED * 10.0f;
    position +=
        rotateVector(p_Angle, {p_TargetAxii, 1.0f}) * ((p_Boost) ? newMoveSpeed : MOVESPEED);
    return;
}

void Camera::rotate(glm::vec3 p_Delta,
                    float p_FrameDelta) // should remain unused while using timesteps
{
    float speedLimit = 0.5f;
    float toApplyX = (p_Delta.x * p_FrameDelta * speedLimit);
    float toApplyY = (p_Delta.y * p_FrameDelta * speedLimit);

    float upcomingZ = 0.0f;

    float upcomingX = rotation.x + toApplyX;
    float upcomingY = rotation.y + toApplyY;

    if (fabs(upcomingX) >= 89.99f)
    {
        bool isLookingUp = (rotation.x > 0) ? true : false;
        if (isLookingUp)
        {
            toApplyX = (89.99f - rotation.x);
            upcomingX = rotation.x + toApplyX;
        }
        else
        {
            toApplyX = -(rotation.x + 89.99f);
            upcomingX = rotation.x + toApplyX;
        }
    }

    rotation = glm::vec3(upcomingX, upcomingY, upcomingZ);
    return;
}

void Camera::moveUp(void)
{
    position.y -= MOVESPEED * 1.5f;
    return;
}
void Camera::moveUp(bool p_IsBoost)
{
    if(p_IsBoost)
        position.y -= MOVESPEED * 5.0f;
    else
        position.y -= MOVESPEED * 1.5f;
}
void Camera::moveDown(void)
{
    position.y += MOVESPEED * 1.5f;
    return;
}
void Camera::moveDown(bool p_IsBoost)
{
    if(p_IsBoost)
        position.y += MOVESPEED * 5.0f;
    else
        position.y += MOVESPEED * 1.5f;
}
void Camera::moveLeft(void)
{
    getFrontFace();
    position -= glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED;
}
void Camera::moveRight(void)
{
    getFrontFace();
    position += glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED;
}
void Camera::moveForward(void)
{
    getFrontFace();
    position += front * MOVESPEED * 1.5f;
}
void Camera::moveBackward(void)
{
    getFrontFace();
    position -= front * MOVESPEED * 1.5f;
}

bool Camera::setCameraType(CameraType p_CameraType, glm::vec3 p_Position)
{
    switch (p_CameraType)
    {
        using enum CameraType;
        case eThirdPerson:
            {
                using enum LogHandler::MessagePriority;
                if (!target)
                {
                    ptrLogger->logMessage(eError, "Can't set to third person due to no target");
                    return false;
                }
                break;
            }
        case eFirstPerson:
            {
                using enum LogHandler::MessagePriority;
                ptrLogger->logMessage(eError, "First person mode not implemented yet");
                return false;
                break;
            }
        default:
            {
                break;
            }
    }
    position = p_Position;
    type = p_CameraType;
    return true;
}

void Camera::updateFreelook(void)
{
    glm::mat4 xMat = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), {1.0f, 0.0f, 0.0f});
    glm::mat4 yMat = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), {0.0f, 1.0f, 0.0f});

    glm::mat4 rotMat = yMat * xMat;

    glm::mat4 posMat = glm::translate(glm::mat4(1.0f), position);

    viewUniformObject->matrix = glm::inverse(rotMat) * glm::inverse(posMat);
    return;
}

void Camera::updateThirdPerson(void)
{
    // set camera origin to target's position
    position = glm::vec3(target->position.x, target->position.y - 1.0f, target->position.z);

    // angle around player
    // to make the camera stick at to a particular angle relative to
    // player add the following to orbit_angle `target->rotation.y` =>
    // orbit_angle + target.y
    glm::mat4 y_mat =
        glm::rotate(glm::mat4(1.0f), glm::radians(orbitAngle), glm::vec3(0.0f, 1.0f, 0.0f));

    // camera pitch angle
    glm::mat4 x_mat =
        glm::rotate(glm::mat4(1.0f), glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));

    glm::mat4 rotationMatrix = y_mat * x_mat;

    glm::mat4 targetMat =
        glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, position.z));
    glm::mat4 distanceMat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, zoomLevel));

    viewUniformObject->matrix = targetMat * rotationMatrix * distanceMat;
    viewUniformObject->matrix = glm::inverse(viewUniformObject->matrix);
    return;
}

glm::vec3 Camera::rotateVector(float p_OrbitAngle, glm::vec4 p_TargetAxii)
{
    // construct rotation matrix from orbit angle
    glm::mat4 rotationMatrix = glm::mat4(1.0);
    rotationMatrix =
        glm::rotate(rotationMatrix, glm::radians(p_OrbitAngle), glm::vec3(0.0f, 1.0f, 0.0f));

    // multiply our def vec

    glm::vec4 x_col = {0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 y_col = {0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 z_col = {0.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 w_col = {0.0f, 0.0f, 0.0f, 0.0f};

    // x * x_column
    x_col.x = p_TargetAxii.x * rotationMatrix[0][0];
    x_col.y = p_TargetAxii.x * rotationMatrix[0][1];
    x_col.z = p_TargetAxii.x * rotationMatrix[0][2];
    x_col.w = p_TargetAxii.x * rotationMatrix[0][3];

    // y * y_column
    y_col.x = p_TargetAxii.y * rotationMatrix[1][0];
    y_col.y = p_TargetAxii.y * rotationMatrix[1][1];
    y_col.z = p_TargetAxii.y * rotationMatrix[1][2];
    y_col.w = p_TargetAxii.y * rotationMatrix[1][3];

    // z * z_column
    z_col.x = p_TargetAxii.z * rotationMatrix[2][0];
    z_col.y = p_TargetAxii.z * rotationMatrix[2][1];
    z_col.z = p_TargetAxii.z * rotationMatrix[2][2];
    z_col.w = p_TargetAxii.z * rotationMatrix[2][3];

    // w * w_column
    w_col.x = p_TargetAxii.w * rotationMatrix[3][0];
    w_col.y = p_TargetAxii.w * rotationMatrix[3][1];
    w_col.z = p_TargetAxii.w * rotationMatrix[3][2];
    w_col.w = p_TargetAxii.w * rotationMatrix[3][3];

    glm::vec4 f = x_col + y_col + z_col + w_col;

    // extract relevant data & return
    return {f.x, f.y, f.z};
}