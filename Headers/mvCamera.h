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
#include "mvModel.h"

namespace mv
{
    enum CameraType
    {
        eInvalid = 0,
        eFreeLook,
        eFirstPerson,
        eThirdPerson
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
        Camera(struct CameraInitStruct &p_CameraInitStruct)
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
        ~Camera()
        {
        }
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

        glm::vec3 position = glm::vec3(1.0);
        glm::vec3 rotation = glm::vec3(0.1f, 0.1f, 0.1f);
        glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);

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
            if (type == CameraType::eThirdPerson)
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

        inline void increasePitch(void)
        {
            constexpr float speed = 0.35f;
            float fZoom = zoomLevel + speed;
            if (fZoom < 20.0f)
            {
                zoomLevel += speed;
                pitch += speed;
            }
            return;
        }
        inline void increasePitch(float p_Delta)
        {
            constexpr float speed = 0.35f;
            float fZoom = zoomLevel + (speed * p_Delta);
            if (fZoom < 20.0f)
            {
                zoomLevel += speed;
                pitch += speed;
            }
            return;
        }
        inline void decreasePitch(void)
        {
            constexpr float speed = 0.35f;
            float fZoom = zoomLevel - speed;
            if (fZoom > 3.4f)
            {
                zoomLevel -= speed;
                pitch -= speed;
            }
            return;
        }
        inline void decreasePitch(float p_Delta)
        {
            constexpr float speed = 0.35f;
            float fZoom = zoomLevel - (speed * p_Delta);
            if (fZoom > 3.4f)
            {
                zoomLevel -= speed;
                pitch -= speed;
            }
            return;
        }
        inline void increaseOrbit(void)
        {
            constexpr float speed = 2.0f;
            float fOrbit = orbitAngle + speed;
            if (fOrbit >= 360.0f)
            {
                fOrbit = 0.0f;
            }
            orbitAngle = fOrbit;
            return;
        }
        inline void increaseOrbit(float p_Delta)
        {
            constexpr float speed = 0.25f;
            float fOrbit = orbitAngle + (speed * p_Delta);
            if (fOrbit > 359.9f)
            {
                fOrbit = 0.0f;
            }
            orbitAngle = fOrbit;
            return;
        }
        inline void decreaseOrbit(void)
        {
            constexpr float speed = 2.0f;
            float fOrbit = orbitAngle - speed;
            if (fOrbit < 0.0f)
            {
                fOrbit = 359.9f;
            }
            orbitAngle = fOrbit;
            return;
        }
        inline void decreaseOrbit(float p_Delta)
        {
            constexpr float speed = 0.25f;
            float fOrbit = orbitAngle - (speed * p_Delta);
            if (fOrbit < 0.0f)
            {
                fOrbit = 359.9f;
            }
            orbitAngle = fOrbit;
            return;
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
        inline void rotate(glm::vec3 p_Delta, float p_FrameDelta) // should remain unused while using timesteps
        {
            std::cout << "delta => " << p_Delta.x << ", " << p_Delta.y << "\n";
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
        inline void moveUp(float p_FrameDelta)
        {
            position.y += MOVESPEED * p_FrameDelta;
            return;
        }
        inline void moveDown(float p_FrameDelta)
        {
            position.y -= MOVESPEED * p_FrameDelta;
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
            position += front * MOVESPEED;
        }
        inline void moveBackward(void)
        {
            getFrontFace();
            position -= front * MOVESPEED;
        }

        inline void updateFreelook(void)
        {
            glm::mat4 rotationMatrix = glm::mat4(1.0f);
            glm::mat4 translationMatrix = glm::mat4(1.0f);

            // rotate about each axis
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            rotationMatrix = glm::rotate(rotationMatrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

            translationMatrix = glm::translate(glm::mat4(1.0), position);

            viewUniformObject->matrix = rotationMatrix * translationMatrix;
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
    };
}; // namespace mv

#endif