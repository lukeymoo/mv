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
#include "mvCollection.h"

namespace mv
{
    struct camera_init_struct
    {
        // ptr to view & projection matrices
        mv::Collection::uniform_object *view_uniform_object = nullptr;
        mv::Collection::uniform_object *projection_uniform_object = nullptr;

        float fov = -1;
        float aspect = -1;
        float nearz = -1;
        float farz = -1;

        int camera_type = 0;
        mv::Object *target = nullptr;

        glm::vec3 position = glm::vec3(1.0);
    };

    /*
        ------------------------------
        --      camera class        --
        ------------------------------
    */

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

        mv::Collection::uniform_object *view_uniform_object = nullptr;
        mv::Collection::uniform_object *projection_uniform_object = nullptr;

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
        mv::Object *target = nullptr;
        float pitch = 50.0f;
        float orbit_angle = 0.0f;
        float zoom_level = 10.0f;
        bool has_init = false;
        enum camera_type
        {
            free_look = 0,
            first_person,
            third_person
        };
        // default to free_look as first_person incomplete
        // & third_person requires user specified target
        camera_type type = free_look;

        // should orbit steps be a multiple of some kind of window size ratio?
        static constexpr float orbit_step = 0.125f;

        static constexpr float min_orbit = 0.0f;
        static constexpr float max_orbit = 359.9f;

        // camera zoom
        float zoom_accel = 0.0f;
        static constexpr float zoom_step = 0.25f;
        static constexpr float zoom_friction = zoom_step * 0.125f;

        static constexpr float max_zoom_level = 20.0f;
        static constexpr float min_zoom_level = 3.4f;

    public:
        // init object variables and configure the projection matrix & initial view matrix
        Camera(struct camera_init_struct &init_params)
        {
            fov = init_params.fov;
            aspect = init_params.aspect;
            nearz = init_params.nearz;
            farz = init_params.farz;
            position = init_params.position;
            view_uniform_object = init_params.view_uniform_object;
            projection_uniform_object = init_params.projection_uniform_object;

            type = (camera_type)init_params.camera_type;

            // ensure matrices specified
            if (init_params.view_uniform_object == nullptr)
            {
                throw std::runtime_error("No view matrix uniform object specified");
            }
            if (init_params.projection_uniform_object == nullptr)
            {
                throw std::runtime_error("No projection matrix uniform object specified");
            }

            // ensure target is given if type is third person
            if (type == camera_type::third_person)
            {
                if (init_params.target == nullptr)
                {
                    throw std::runtime_error("Camera type is specified as third person yet no target specified in initialization structure");
                }
                target = init_params.target;
            }

            rotation = glm::vec3(0.01f, 0.01f, 0.01f);
            camera_front = glm::vec3(0.0f, 0.0f, -1.0f);

            update();

            projection_uniform_object->matrix = glm::perspective(glm::radians(fov), aspect, nearz, farz);
            projection_uniform_object->matrix[1][1] *= -1.0f;

            // TODO
            // add non host visible/coherent update support
            // update projection matrix buffer
            memcpy(projection_uniform_object->mv_buffer.mapped,
                   &projection_uniform_object->matrix,
                   sizeof(projection_uniform_object->matrix));
        }
        ~Camera() {}

    private:
        Camera(const Camera &) = delete;
        Camera &operator=(const Camera &) = delete;

    public:
        // Updates view matrix
        inline void update(void)
        {
            // process camera zoom acceleration
            if (zoom_accel != 0.0f)
            {
                if (check_zoom(zoom_accel))
                {
                    zoom_level += zoom_accel;
                }
                else
                {
                    if (zoom_accel < 0)
                    {
                        zoom_level = min_zoom_level;
                    }
                    else
                    {
                        zoom_level = max_zoom_level;
                    }
                    zoom_accel = 0.0f;
                }
                // friction implementation
                if (zoom_accel > 0.0f)
                {
                    zoom_accel -= zoom_friction;
                }
                else if (zoom_accel < 0.0f)
                {
                    zoom_accel += zoom_friction;
                }
            }

            if (type == camera_type::free_look)
            {
                glm::mat4 rotation_matrix = glm::mat4(1.0f);
                glm::mat4 translation_matrix = glm::mat4(1.0f);

                // rotate about each axis
                rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
                rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
                rotation_matrix = glm::rotate(rotation_matrix, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

                translation_matrix = glm::translate(glm::mat4(1.0), position);

                view_uniform_object->matrix = rotation_matrix * translation_matrix;
            }
            else if (type == camera_type::first_person)
            {
                throw std::runtime_error("first person camera mode currently unsupported");
            }
            else if (type == camera_type::third_person)
            {
                // set camera origin to target's position
                set_position(glm::vec3(target->position.x,
                                       target->position.y - 0.5f,
                                       target->position.z));

                // angle around player
                glm::mat4 y_mat = glm::rotate(glm::mat4(1.0f),
                                              glm::radians(orbit_angle + target->rotation.y),
                                              glm::vec3(0.0f, 1.0f, 0.0f));

                // camera pitch angle
                glm::mat4 x_mat = glm::rotate(glm::mat4(1.0f), glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));

                glm::mat4 rotation_matrix = y_mat * x_mat;

                glm::mat4 target_mat = glm::translate(glm::mat4(1.0f), glm::vec3(position.x, position.y, position.z));
                glm::mat4 distance_mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, zoom_level));

                view_uniform_object->matrix = target_mat * rotation_matrix * distance_mat;
                view_uniform_object->matrix = glm::inverse(view_uniform_object->matrix);
            }

            // TODO
            // add support for non host coherent/host visible buffers
            // end of update() copy to view matrix ubo
            memcpy(view_uniform_object->mv_buffer.mapped, &view_uniform_object->matrix, sizeof(view_uniform_object->matrix));
            return;
        }

        // third person methods
        inline void adjust_zoom(float delta)
        {
            zoom_accel += delta;
            return;
        }
        inline void adjust_orbit(float delta, float start_orbit)
        {
            orbit_angle = start_orbit + (orbit_step * delta);
            return;
        }
        inline void realign_orbit(void)
        {
            if (orbit_angle > 359.9f)
            {
                orbit_angle = abs(orbit_angle) - 359.9f;
            }
            else if (orbit_angle < 0.0f)
            {
                orbit_angle = 359.9f - abs(orbit_angle);
            }
            return;
        }

        inline bool check_zoom(float delta)
        {
            if (delta > 0) // zooming out
            {
                if ((zoom_level + delta) < max_zoom_level)
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
                if ((zoom_level + delta) > min_zoom_level)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
        }

        inline void increase_pitch(void)
        {
            constexpr float speed = 0.35f;
            float f_zoom = zoom_level + speed;
            if (f_zoom < 20.0f)
            {
                zoom_level += speed;
                pitch += speed;
            }
            return;
        }
        inline void increase_pitch(float delta)
        {
            constexpr float speed = 0.35f;
            float f_zoom = zoom_level + (speed * delta);
            if (f_zoom < 20.0f)
            {
                zoom_level += speed;
                pitch += speed;
            }
            return;
        }

        inline void decrease_pitch(void)
        {
            constexpr float speed = 0.35f;
            float f_zoom = zoom_level - speed;
            if (f_zoom > 3.4f)
            {
                zoom_level -= speed;
                pitch -= speed;
            }
            return;
        }
        inline void decrease_pitch(float delta)
        {
            constexpr float speed = 0.35f;
            float f_zoom = zoom_level - (speed * delta);
            if (f_zoom > 3.4f)
            {
                zoom_level -= speed;
                pitch -= speed;
            }
            return;
        }

        inline void increase_orbit(void)
        {
            constexpr float speed = 2.0f;
            float f_orbit = orbit_angle + speed;
            if (f_orbit >= 360.0f)
            {
                f_orbit = 0.0f;
            }
            orbit_angle = f_orbit;
            return;
        }
        inline void increase_orbit(float mouse_delta)
        {
            constexpr float speed = 0.25f;
            float f_orbit = orbit_angle + (speed * mouse_delta);
            if (f_orbit > 359.9f)
            {
                f_orbit = 0.0f;
            }
            orbit_angle = f_orbit;
            return;
        }

        inline void decrease_orbit(void)
        {
            constexpr float speed = 2.0f;
            float f_orbit = orbit_angle - speed;
            if (f_orbit < 0.0f)
            {
                f_orbit = 359.9f;
            }
            orbit_angle = f_orbit;
            return;
        }
        inline void decrease_orbit(float mouse_delta)
        {
            constexpr float speed = 0.25f;
            float f_orbit = orbit_angle - (speed * mouse_delta);
            if (f_orbit < 0.0f)
            {
                f_orbit = 359.9f;
            }
            orbit_angle = f_orbit;
            return;
        }

        // free look methods
        inline void get_front_face(void)
        {
            glm::vec3 fr;
            fr.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
            fr.y = sin(glm::radians(rotation.x));
            fr.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
            fr = glm::normalize(fr);

            camera_front = fr;
        }
        inline void rotate(glm::vec3 delta, float frame_delta)
        {
            float speed_limit = 0.10f;
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
            return;
        }
        inline void move_up(float frame_delta)
        {
            position.y += MOVESPEED * frame_delta;
            return;
        }
        inline void move_down(float frame_delta)
        {
            position.y -= MOVESPEED * frame_delta;
            return;
        }
        inline void move_left(float frame_delta)
        {
            get_front_face();
            position -= glm::normalize(glm::cross(camera_front, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED * frame_delta;
        }
        inline void move_right(float frame_delta)
        {
            get_front_face();
            position += glm::normalize(glm::cross(camera_front, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED * frame_delta;
        }
        inline void move_forward(float frame_delta)
        {
            get_front_face();
            position += camera_front * MOVESPEED * frame_delta;
        }
        inline void move_backward(float frame_delta)
        {
            get_front_face();
            position -= camera_front * MOVESPEED * frame_delta;
        }

        inline glm::vec3 get_position(void)
        {
            return position;
        }

        inline void set_position(glm::vec3 new_position)
        {
            this->position = new_position;
            return;
        }

        void set_projection(float fv, float aspct, float nz, float fz)
        {
            this->fov = fv;
            this->nearz = nz;
            this->farz = fz;
            this->aspect = aspct;
            projection_uniform_object->matrix = glm::perspective(glm::radians(fv), aspct, nz, fz);
            projection_uniform_object->matrix[1][1] *= -1.0f;

            memcpy(projection_uniform_object->mv_buffer.mapped,
                   &projection_uniform_object->matrix,
                   sizeof(projection_uniform_object->matrix));
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
    };
};

#endif