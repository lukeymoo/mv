#ifndef HEADERS_MVCAMERA_H_
#define HEADERS_MVCAMERA_H_

#include <memory>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

namespace mv
{
    class Camera
    {
    public:
        Camera(float fov, float aspect, float nearz, float farz, glm::vec3 pos)
        {
            this->fov = fov;
            this->aspect = aspect;
            this->nearz = nearz;
            this->farz = farz;
            this->position = pos;

            matrices.perspective = glm::perspective(glm::radians(fov), aspect, nearz, farz);
            //matrices.perspective[1][1] *= -1.0f;

            update_view();
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
            free_look,
            first_person,
            third_person
        };
        Type camera_type = free_look;

        struct
        {
            glm::mat4 view = glm::mat4(1.0);
            glm::mat4 perspective = glm::mat4(1.0);
        } matrices;

        void update_view(void)
        {
            glm::mat4 rotation_matrix = glm::mat4(1.0f);
            glm::mat4 translation_matrix = glm::mat4(1.0f);

            // rotate about each axis
            rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
            rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
            rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

            translation_matrix = glm::translate(glm::mat4(1.0), position);

            // freelook
            matrices.view = translation_matrix * rotation_matrix;

            // TODO
            // add more camera modes
            // first person
            // rotation_matrix * translation_matrix
        }

        // calculate current camera front
        void update(void)
        {
            glm::vec3 fr;
            fr.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
            fr.y = sin(glm::radians(rotation.x));
            fr.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
            fr = glm::normalize(fr);

            camera_front = fr;
        }

        // movement
        void move_left(void)
        {
            update();
            position -= glm::normalize(glm::cross(camera_front, glm::vec3(0.0f, 1.0f, 0.0f))) * 0.2f;
            update_view();
        }
        void move_right(void)
        {
            update();
            position += glm::normalize(glm::cross(camera_front, glm::vec3(0.0f, 1.0f, 0.0f))) * 0.2f;
            update_view();
        }
        void move_forward(void)
        {
            update();
            position += camera_front * 0.2f;
            update_view();
        }
        void move_backward(void)
        {
            update();
            position -= camera_front * 0.2f;
            update_view();
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

        glm::vec3 get_default_up_direction(void)
        {
            return DEFAULT_UP_VECTOR;
        }
        glm::vec3 get_default_down_direction(void)
        {
            return DEFAULT_UP_VECTOR;
        }

    protected:
        glm::vec3 rotation = glm::vec3(1.0);
        glm::vec3 position = glm::vec3(1.0);
        glm::vec4 view_position = glm::vec4(1.0);
        glm::vec3 camera_front = glm::vec3(0.0f, 0.0f, -1.0f);

    protected:
    private:
        float fov = 50.0f;
        float farz = 100.0f;
        float nearz = 0.1f;
        float aspect = 0;

    public:
        const glm::vec3 DEFAULT_UP_VECTOR = {0.0f, 1.0f, 0.0f};
        const glm::vec3 DEFAULT_FORWARD_VECTOR = {0.0f, 0.0f, 1.0f};
        const glm::vec3 DEFAULT_BACKWARD_VECTOR = {0.0f, 0.0f, -1.0f};
        const glm::vec3 DEFAULT_LEFT_VECTOR = {-1.0f, 0.0f, 0.0f};
        const glm::vec3 DEFAULT_RIGHT_VECTOR = {1.0f, 0.0f, 0.0f};
    };
};

#endif