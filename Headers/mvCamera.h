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
        // if camera type is third person
        // the camera initalizer must be given an object as a target
        uint32_t target_index = 0;
        std::shared_ptr<std::vector<mv::Object>> objects_list = nullptr;

        glm::vec3 position = glm::vec3(1.0);
    };

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
        uint32_t target_index = 0;
        std::shared_ptr<std::vector<mv::Object>> objects_list;
        float pitch = 45.0f;
        float orbit_angle = 0.0f;
        float zoom_level = 10.0f;

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
                if (init_params.objects_list == nullptr)
                {
                    throw std::runtime_error("Camera type is specified as third person yet no target specified in initialization structure");
                }

                target_index = init_params.target_index;
                objects_list = std::move(init_params.objects_list);
            }

            rotation = glm::vec3(0.01f, 0.01f, 0.01f);
            camera_front = glm::vec3(0.0f, 0.0f, -1.0f);

            update();

            projection_uniform_object->matrix = glm::perspective(glm::radians(fov), aspect, nearz, farz);
            projection_uniform_object->matrix[1][1] *= -1.0f;

            // matrices->projection = glm::perspective(glm::radians(fov), aspect, nearz, farz);
            // matrices->projection[1][1] *= -1.0f;

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

        // Updates view matrix
        void update(void)
        {
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
                set_position(glm::vec3(objects_list->at(target_index).position.x,
                                       objects_list->at(target_index).position.y - 0.5f,
                                       objects_list->at(target_index).position.z));

                // angle around player
                glm::mat4 y_mat = glm::rotate(glm::mat4(1.0f),
                                              glm::radians(orbit_angle + objects_list->at(target_index).rotation.y),
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

        /*
            -----------------------------
            third person methods
            -----------------------------
        */
        void increase_pitch(float *frame_delta)
        {
            float f_zoom = zoom_level - (0.01f * (*frame_delta));
            if (f_zoom < 20.0f)
            {
                zoom_level += 0.01f * (*frame_delta);
                pitch += 0.01f * (*frame_delta);
            }
            return;
        }
        void increase_pitch(float speed_limit, float *frame_delta)
        {
            float f_zoom = zoom_level - (speed_limit * (*frame_delta));
            if (f_zoom < 20.0f)
            {
                zoom_level += speed_limit * (*frame_delta);
                pitch += speed_limit * (*frame_delta);
            }
            return;
        }


        void decrease_pitch(float *frame_delta)
        {
            float f_zoom = zoom_level - (0.01f * (*frame_delta));
            if (f_zoom > 3.4f)
            {
                zoom_level -= 0.01f * (*frame_delta);
                pitch -= 0.01f * (*frame_delta);
            }
            return;
        }
        void decrease_pitch(float speed_limit, float *frame_delta)
        {
            float f_zoom = zoom_level - (speed_limit * (*frame_delta));
            if (f_zoom > 3.4f)
            {
                zoom_level -= speed_limit * (*frame_delta);
                pitch -= speed_limit * (*frame_delta);
            }
            return;
        }

        void increase_orbit(float *frame_delta)
        {
            // keep orbit value in range of 0.0f -> 359.9f
            // larger floats lose accuracy
            float f_orbit = orbit_angle + (0.1f * (*frame_delta));
            if (f_orbit >= 360.0f)
            {
                f_orbit = 0.0f;
            }
            orbit_angle = f_orbit;
            return;
        }
        void increase_orbit(float position_delta, float *frame_delta)
        {
            float f_orbit = orbit_angle + (position_delta * (*frame_delta));
            if (f_orbit >= 360.0f)
            {
                f_orbit = 0.0f;
            }
            orbit_angle = f_orbit;
        }

        void decrease_orbit(float *frame_delta)
        {
            float f_orbit = orbit_angle - (0.1f * (*frame_delta));
            if (f_orbit < 0.0f)
            {
                f_orbit = 359.9f;
            }
            orbit_angle = f_orbit;
            return;
        }

        void decrease_orbit(float position_delta, float *frame_delta)
        {
            float f_orbit = orbit_angle - (position_delta * (*frame_delta));
            if (f_orbit < 0.0f)
            {
                f_orbit = 359.9f;
            }
            orbit_angle = f_orbit;
            return;
        }
        /*
            -----------------------
            end third person methods
            -----------------------
        */

        /*
            -------------------
            free look methods
            -------------------
        */
        void get_front_face(void)
        {
            glm::vec3 fr;
            fr.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
            fr.y = sin(glm::radians(rotation.x));
            fr.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
            fr = glm::normalize(fr);

            camera_front = fr;
        }
        void rotate(glm::vec3 delta, float *frame_delta)
        {
            float speed_limit = 0.10f;
            float to_apply_x = (delta.x * (*frame_delta) * speed_limit);
            float to_apply_y = (delta.y * (*frame_delta) * speed_limit);

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
        void move_up(float *frame_delta)
        {
            position.y += MOVESPEED * (*frame_delta);
            return;
        }
        void move_down(float *frame_delta)
        {
            position.y -= MOVESPEED * (*frame_delta);
            return;
        }
        void move_left(float *frame_delta)
        {
            get_front_face();
            position -= glm::normalize(glm::cross(camera_front, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED * (*frame_delta);
        }
        void move_right(float *frame_delta)
        {
            get_front_face();
            position += glm::normalize(glm::cross(camera_front, glm::vec3(0.0f, 1.0f, 0.0f))) * MOVESPEED * (*frame_delta);
        }
        void move_forward(float *frame_delta)
        {
            get_front_face();
            position += camera_front * MOVESPEED * (*frame_delta);
        }
        void move_backward(float *frame_delta)
        {
            get_front_face();
            position -= camera_front * MOVESPEED * (*frame_delta);
        }
        /*
            -----------------------
            end first person methods
            -----------------------
        */

        glm::vec3 get_position(void)
        {
            return position;
        }

        void set_position(glm::vec3 new_position)
        {
            this->position = new_position;
            return;
        }

        void set_projection(float fv, float aspect, float nz, float fz)
        {
            fov = fv;
            nearz = nz;
            farz = fz;
            projection_uniform_object->matrix = glm::perspective(glm::radians(fov), aspect, nearz, farz);
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