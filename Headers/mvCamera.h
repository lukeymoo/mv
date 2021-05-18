#ifndef HEADERS_MVCAMERA_H_
#define HEADERS_MVCAMERA_H_

#include <memory>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>

#include "mvBuffer.h"
#include "mvCollection.h"
#include "mvHelper.h"
#include "mvModel.h"

namespace mv
{
    enum CameraType
    {
        eInvalid = 0,
        eFreeLook,
        eFirstPerson,
        eThirdPerson,
        eIsometric // MOBA / Old RTS style
    };

    struct CameraInitStruct
    {
        // ptr to view & projection matrices
        struct mv::UniformObject *viewUniformObject = nullptr;
        struct mv::UniformObject *projectionUniformObject = nullptr;

        float fov = -1;
        float aspect = -1;
        float nearz = -1;
        float farz = -1;

        CameraType type = eInvalid;
        struct mv::Object *target = nullptr;

        glm::vec3 position = {0.0f, 0.0f, 0.0f};
    };

    /*
        ------------------------------
        --      camera class        --
        ------------------------------
    */

    struct Camera
    {
      public:
        Camera(struct CameraInitStruct &p_CameraInitStruct);
        ~Camera();
        Camera(const Camera &) = delete;
        Camera &operator=(const Camera &) = delete;

      public:
        mv::Object *target = nullptr;
        CameraType type = CameraType::eFreeLook;
        float zoomLevel = 10.0f;

        // -- free look --
        // movement
        glm::vec3 moveAccel = {0.0f, 0.0f, 0.0f};
        static constexpr float moveStep = 0.05f;
        static constexpr float maxMoveAccel = 0.25f; // normal speed
        // static constexpr float max_move_accel = 0.5f; // maybe speed boost for freelook?
        static constexpr float moveFriction = moveStep * 0.3f;

        mv::LogHandler *ptrLogger = nullptr;

        // -- third person --
        float pitch = 40.0f;
        float targetPitch = 40.0f;

        float orbitAngle = 0.0f;
        float targetOrbit = 0.0f;
        static constexpr float orbitSmoothness = 0.7f;
        static constexpr float pitchSmoothness = 0.7f;

        // camera zoom
        float zoomAccel = 0.0f;
        static constexpr float zoomStep = 0.25f;
        static constexpr float maxZoomLevel = 20.0f;
        static constexpr float minZoomLevel = 3.4f;
        static constexpr float zoomFriction = zoomStep * 0.125f;

        glm::vec3 position = {0.0f, 0.0f, 0.0f};
        glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
        glm::vec3 front = {0.0f, 0.0f, 1.0f};

      private:
        float aspect = 0;
        float fov = 50.0f;
        float nearz = 0.1f;
        float farz = 100.0f;

        mv::UniformObject *viewUniformObject = nullptr;
        mv::UniformObject *projectionUniformObject = nullptr;

        static constexpr glm::vec3 DEFAULT_UP_VECTOR = {0.0f, 1.0f, 0.0f};
        static constexpr glm::vec3 DEFAULT_DOWN_VECTOR = {0.0f, -1.0f, 0.0f};
        static constexpr glm::vec3 DEFAULT_FORWARD_VECTOR = {0.0f, 0.0f, 1.0f};
        static constexpr glm::vec3 DEFAULT_BACKWARD_VECTOR = {0.0f, 0.0f, -1.0f};
        static constexpr glm::vec3 DEFAULT_LEFT_VECTOR = {-1.0f, 0.0f, 0.0f};
        static constexpr glm::vec3 DEFAULT_RIGHT_VECTOR = {1.0f, 0.0f, 0.0f};

      public:
        // Updates view matrix
        inline void update(void)
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
            memcpy(viewUniformObject->mvBuffer.mapped, &viewUniformObject->matrix, sizeof(viewUniformObject->matrix));
            return;
        }

        inline void adjustMovement(glm::vec3 p_Delta)
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

        // third person methods
        inline void adjustZoom(float p_Delta)
        {
            zoomAccel += p_Delta;
            return;
        }

        inline void adjustPitch(float p_Delta, float p_StartPitch)
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

        inline void adjustPitch(float &p_Angle)
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

        inline void adjustOrbit(float p_Delta, float p_StartOrbit)
        {
            constexpr float orbitStep = 0.125f;
            orbitAngle = p_StartOrbit + (orbitStep * p_Delta);
            return;
        }
        inline void adjustOrbit(float p_Delta)
        {
            orbitAngle += p_Delta;
            return;
        }

        inline void lerpOrbit(float p_Delta)
        {
            targetOrbit += p_Delta;
            // realign_orbit(target_orbit);
            return;
        }
        inline void lerpPitch(float p_Delta)
        {
            targetPitch += p_Delta;
            adjustPitch(targetPitch);
            return;
        }

        inline void realignOrbit(void)
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
        inline void realignOrbit(float &p_Angle)
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

        inline bool checkZoom(float p_Delta)
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
        // free look methods
        inline void getFrontFace(void)
        {
            glm::vec3 fr;
            fr.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
            fr.y = sin(glm::radians(rotation.x));
            fr.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
            fr = glm::normalize(fr);

            front = fr;
            return;
        }
        inline void getFrontFace(float p_OrbitAngle)
        {
            glm::vec3 fr;
            fr.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(p_OrbitAngle));
            fr.y = sin(glm::radians(rotation.x));
            fr.z = cos(glm::radians(rotation.x)) * cos(glm::radians(p_OrbitAngle));
            fr = glm::normalize(fr);

            front = fr;
            return;
        }
        inline void move(float p_Angle, glm::vec3 p_TargetAxii, bool p_Boost = false)
        {
            constexpr float newMoveSpeed = MOVESPEED * 3.0f;
            position += rotateVector(p_Angle, {p_TargetAxii, 1.0f}) * ((p_Boost) ? newMoveSpeed : MOVESPEED);
            return;
        }
        inline void rotate(glm::vec3 p_Delta, float p_FrameDelta) // should remain unused while using timesteps
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
        inline void moveUp(void)
        {
            position.y -= MOVESPEED * 1.5f;
            return;
        }
        inline void moveDown(void)
        {
            position.y += MOVESPEED * 1.5f;
            return;
        }
        inline void moveLeft(void)
        {
            getFrontFace();
            position -= glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED;
        }
        inline void moveRight(void)
        {
            getFrontFace();
            position += glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED;
        }
        inline void moveForward(void)
        {
            getFrontFace();
            position += front * MOVESPEED * 1.5f;
        }
        inline void moveBackward(void)
        {
            getFrontFace();
            position -= front * MOVESPEED * 1.5f;
        }

        inline bool setCameraType(CameraType p_CameraType, glm::vec3 p_Position)
        {
            switch (p_CameraType)
            {
                case CameraType::eThirdPerson:
                    {
                        if (!target)
                        {
                            ptrLogger->logMessage(LogHandler::MessagePriority::eError,
                                                  "Can't set to third person due to no target");
                            return false;
                        }
                        break;
                    }
                case CameraType::eFirstPerson:
                    {
                        ptrLogger->logMessage(LogHandler::MessagePriority::eError,
                                              "First person mode not implemented yet");
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

        inline void updateFreelook(void)
        {
            glm::mat4 xMat = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), {1.0f, 0.0f, 0.0f});
            glm::mat4 yMat = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), {0.0f, 1.0f, 0.0f});

            glm::mat4 rotMat = yMat * xMat;

            glm::mat4 posMat = glm::translate(glm::mat4(1.0f), position);

            viewUniformObject->matrix = glm::inverse(rotMat) * glm::inverse(posMat);
            return;
        }

        inline void updateThirdPerson(void)
        {
            // set camera origin to target's position
            position = glm::vec3(target->position.x, target->position.y - 1.0f, target->position.z);

            // angle around player
            // to make the camera stick at to a particular angle relative to player
            // add the following to orbit_angle `target->rotation.y` => orbit_angle + target.y
            glm::mat4 y_mat = glm::rotate(glm::mat4(1.0f), glm::radians(orbitAngle), glm::vec3(0.0f, 1.0f, 0.0f));

            // camera pitch angle
            glm::mat4 x_mat = glm::rotate(glm::mat4(1.0f), glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));

            glm::mat4 rotationMatrix = y_mat * x_mat;

            glm::mat4 targetMat = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, position.z));
            glm::mat4 distanceMat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, zoomLevel));

            viewUniformObject->matrix = targetMat * rotationMatrix * distanceMat;
            viewUniformObject->matrix = glm::inverse(viewUniformObject->matrix);
            return;
        }

        // P_TargetAxii essentially a unit vector specifying direction
        // p_OrbitAngle is the angle you wish to transform the unit vector by
        inline glm::vec3 rotateVector(float p_OrbitAngle, glm::vec4 p_TargetAxii)
        {
            // construct rotation matrix from orbit angle
            glm::mat4 rotationMatrix = glm::mat4(1.0);
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(p_OrbitAngle), glm::vec3(0.0f, 1.0f, 0.0f));

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
    };
}; // namespace mv

#endif