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

    protected:
    private:
        float fov;
        float farz;
        float nearz;
    };
};

#endif