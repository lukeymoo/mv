#ifndef HEADERS_MVCAMERA_H_
#define HEADERS_MVCAMERA_H_

#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

namespace mv
{
    class Camera
    {
    public:
        Camera(float fov, float aspect, float nearz, float farz, glm::vec3 pos) {
            this->fov = fov;
            this->aspect = aspect;
            this->nearz = nearz;
            this->farz = farz;
            this->position = pos;
        }
        ~Camera() {

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

            // TODO
            // if flipping coordinates is desired later, create a temporary vec3 to store position

            // apply stored position as translation
            translation_matrix = glm::translate(glm::mat4(1.0), position);

            // freelook
            matrices.view = translation_matrix * rotation_matrix;

            // TODO
            // add more camera modes
            // first person
            // rotation_matrix * translation_matrix
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

        void translate(glm::vec3 delta)
        {
            this->position += delta;
            return;
        }

    protected:
        glm::vec3 rotation = glm::vec3();
        glm::vec3 position = glm::vec3();
        glm::vec4 view_position = glm::vec4();

    protected:
    private:
        float fov = 50.0f;
        float farz = 100.0f;
        float nearz = 0.1f;
        float aspect = 0;
    };
};

#endif