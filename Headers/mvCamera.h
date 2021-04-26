#ifndef HEADERS_MVCAMERA_H_
#define HEADERS_MVCAMERA_H_

#include <memory>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "mvModel.h"
#include "mvBuffer.h"

namespace mv
{
    typedef struct camera_init_struct
    {
        float fov = -1;
        float aspect = -1;
        float nearz = -1;
        float farz = -1;

        int camera_type = 0;
        // if camera type is third person
        // the camera initalizer must be given an object as a target
        Object *target = nullptr;

        glm::vec3 position = glm::vec3(1.0);
    } camera_init_struct;

    class Camera
    {
    protected:
        glm::vec3 rotation = glm::vec3(0.1f, 0.1f, 0.1f);
        glm::vec3 position = glm::vec3(1.0);
        glm::vec3 camera_front = glm::vec3(0.0f, 0.0f, -1.0f);

    private:
        float fov = 50.0f;
        float farz = 100.0f;
        float nearz = 0.1f;
        float aspect = 0;

    public:
        const glm::vec3 DEFAULT_UP_VECTOR = {0.0f, 1.0f, 0.0f};
        const glm::vec3 DEFAULT_DOWN_VECTOR = {0.0f, -1.0f, 0.0f};
        const glm::vec3 DEFAULT_FORWARD_VECTOR = {0.0f, 0.0f, 1.0f};
        const glm::vec3 DEFAULT_BACKWARD_VECTOR = {0.0f, 0.0f, -1.0f};
        const glm::vec3 DEFAULT_LEFT_VECTOR = {-1.0f, 0.0f, 0.0f};
        const glm::vec3 DEFAULT_RIGHT_VECTOR = {1.0f, 0.0f, 0.0f};

        /*
            Third person camera params
        */
        Object *target = nullptr;
        float pitch = 45.0f;
        float orbit_angle = 0.0f;
        float zoom_level = 10.0f;

    public:
        // init object variables and configure the projection matrix & initial view matrix
        Camera(camera_init_struct &init_params)
        {
            assert(fov != -1);
            assert(aspect != -1);
            assert(nearz != -1);
            assert(farz != -1);

            fov = init_params.fov;
            aspect = init_params.aspect;
            nearz = init_params.nearz;
            farz = init_params.farz;
            position = init_params.position;

            camera_type = (Type)init_params.camera_type;

            // ensure target is given if type is third person
            if (camera_type == Type::third_person)
            {
                if (init_params.target == nullptr)
                {
                    throw std::runtime_error("Camera type is specified as third person yet no target specified in initialization structure");
                }

                // set target
                target = init_params.target;
            }

            rotation = glm::vec3(0.01f, 0.01f, 0.01f);
            camera_front = glm::vec3(0.0f, 0.0f, -1.0f);

            update();

            matrices.perspective = glm::perspective(glm::radians(fov), aspect, nearz, farz);
            matrices.perspective[1][1] *= -1.0f;
        }
        ~Camera()
        {
        }

    private:
        Camera(const Camera &) = delete;
        Camera &operator=(const Camera &) = delete;

    public:
        bool has_init = false;
        enum Type
        {
            first_person,
            free_look,
            third_person
        };
        Type camera_type = third_person;

        struct
        {
            glm::mat4 view = glm::mat4(1.0);
            glm::mat4 perspective = glm::mat4(1.0);
        } matrices;

        glm::mat4 look_at_object(mv::Object object)
        {
            glm::mat4 r = glm::lookAt(get_position(), object.position, get_default_up_direction());
            return r;
        }

        Type get_type(void)
        {
            return camera_type;
        }

        // Updates view matrix
        void update(void)
        {
            // Third person update view matrix origin to specified object target
            if (camera_type == Camera::Type::third_person)
            {
                set_position(glm::vec3(target->position.x, target->position.y, target->position.z));

                //matrices.view = glm::lookAt(position, target->position, DEFAULT_UP_VECTOR);
                // float pitch = 0.0f;
                // float orbit_angle = 0.0f;
                // float zoom_level = 0.0f;

                // angle around player
                glm::mat4 y_mat = glm::rotate(glm::mat4(1.0f), glm::radians(orbit_angle), glm::vec3(0.0f, 1.0f, 0.0f));
                // camera pitch angle(should point to target)
                glm::mat4 x_mat = glm::rotate(glm::mat4(1.0f), glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));

                glm::mat4 rotation_matrix = y_mat * x_mat;

                glm::mat4 target_mat = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, position.z));
                glm::mat4 distance_mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, zoom_level));

                matrices.view = rotation_matrix * target_mat * distance_mat;
                matrices.view = glm::inverse(matrices.view);
                return;
            }

            printf("Camera Position => (%f, %f, %f)\n", position.x, position.y, position.z);

            glm::mat4 rotation_matrix = glm::mat4(1.0f);
            glm::mat4 translation_matrix = glm::mat4(1.0f);

            // rotate about each axis
            rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

            translation_matrix = glm::translate(glm::mat4(1.0), position);

            matrices.view = rotation_matrix * translation_matrix;
        }

        /*
            Third person camera functions
        */
        void increase_pitch(float frame_delta)
        {
            zoom_level += 0.01f * frame_delta;
            pitch += 0.01f * frame_delta;
            return;
        }

        void decrease_pitch(float frame_delta)
        {
            zoom_level -= 0.01f * frame_delta;
            pitch -= 0.01f * frame_delta;
            return;
        }

        void decrease_orbit(float frame_delta)
        {
            orbit_angle -= 0.1f * frame_delta;
            return;
        }

        void increase_orbit(float frame_delta)
        {
            orbit_angle += 0.1f * frame_delta;
            return;
        }

        // calculate current camera front
        void get_front_face(void)
        {
            glm::vec3 fr;
            fr.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
            fr.y = sin(glm::radians(rotation.x));
            fr.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
            fr = glm::normalize(fr);

            camera_front = fr;
        }

        // rotation
        void rotate(glm::vec3 delta, float frame_delta)
        {
            float speed_limit = 0.06f;
            float to_apply_x = (delta.x * frame_delta * speed_limit);
            float to_apply_y = (delta.y * frame_delta * speed_limit);

            float upcoming_z = 0.0f;

            float upcoming_x = rotation.x + to_apply_x;
            float upcoming_y = rotation.y + to_apply_y;

            if (fabs(upcoming_x) >= 89.99f)
            {
                bool is_looking_up = (rotation.x > 0) ? true : false;
                if (is_looking_up)
                {
                    to_apply_x = (89.99f - rotation.x);
                    upcoming_x = rotation.x + to_apply_x;
                }
                else
                {
                    to_apply_x = -(rotation.x + 89.99f);
                    upcoming_x = rotation.x + to_apply_x;
                }
            }

            rotation.x = upcoming_x;
            rotation.y = upcoming_y;
            rotation.z = upcoming_z;
        }

        // vertical movement
        void move_up(float frame_delta)
        {
            position.y += MOVESPEED * frame_delta;
            return;
        }
        void move_down(float frame_delta)
        {
            position.y -= MOVESPEED * frame_delta;
            return;
        }

        // lateral movement
        void move_left(float frame_delta)
        {
            get_front_face();
            position -= glm::normalize(glm::cross(camera_front, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED * frame_delta;
        }
        void move_right(float frame_delta)
        {
            get_front_face();
            position += glm::normalize(glm::cross(camera_front, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED * frame_delta;
        }
        void move_forward(float frame_delta)
        {
            get_front_face();
            position += camera_front * MOVESPEED * frame_delta;
        }
        void move_backward(float frame_delta)
        {
            get_front_face();
            position -= camera_front * MOVESPEED * frame_delta;
        }

        glm::vec3 get_position(void)
        {
            return position;
        }

        void set_position(glm::vec3 new_position)
        {
            this->position = new_position;
            return;
        }

        void set_perspective(float fv, float aspect, float nz, float fz)
        {
            fov = fv;
            nearz = nz;
            farz = fz;
            matrices.perspective = glm::perspective(glm::radians(fov), aspect, nearz, farz);
            matrices.perspective[1][1] *= -1.0f;
            return;
        }

        glm::mat4 get_view(void)
        {
            return matrices.view;
        }

        glm::mat4 get_projection(void)
        {
            return matrices.perspective;
        }

        glm::vec3 get_default_up_direction(void)
        {
            return DEFAULT_UP_VECTOR;
        }
        glm::vec3 get_default_down_direction(void)
        {
            return DEFAULT_UP_VECTOR;
        }
    };
};

#endif