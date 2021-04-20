#ifndef HEADERS_MVCAMERA_H_
#define HEADERS_MVCAMERA_H_

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

namespace mv
{
    class Camera
    {
    public:
        Camera(void);
        ~Camera(void);

    private:
        Camera(const Camera &) = delete;
        Camera &operator=(const Camera &) = delete;

    public:
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
        }

    protected:
        glm::vec3 rotation = glm::vec3();
        glm::vec3 position = glm::vec3();
        glm::vec4 viewPos = glm::vec4();

    protected:
    private:
        float fov;
        float farz;
        float nearz;
    };
};

#endif