#ifndef HEADERS_MVCOLLECTION_H_
#define HEADERS_MVCOLLECTION_H_

#include <ostream>

#include "mvDevice.h"
#include "mvAllocator.h"
#include "mvModel.h"

namespace mv
{
    struct GlobalUniforms
    {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 projection;

        mv::Buffer ubo_view;
        mv::Buffer ubo_projection;
        VkDescriptorSet view_descriptor_set;
        VkDescriptorSet proj_descriptor_set;
    };

    struct Collection
    {
        mv::Device *device = nullptr;
        std::vector<mv::Model> models;
        std::vector<std::string> model_names;
        std::shared_ptr<mv::Allocator> descriptor_allocator;

        struct uniform_object
        {
            alignas(16) glm::mat4 matrix;
            mv::Buffer mv_buffer;
            VkDescriptorSet descriptor;
        } view_uniform, projection_uniform;

        Collection(mv::Device *device, std::shared_ptr<mv::Allocator> &descriptor_allocator)
        {
            this->device = device;
            this->descriptor_allocator = descriptor_allocator;

            // create uniform buffer for view matrix
            device->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  &view_uniform.mv_buffer,
                                  sizeof(GlobalUniforms::view));
            if (view_uniform.mv_buffer.map() != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to map view matrix buffer");
            }

            // create uniform buffer for projection matrix
            device->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                  &projection_uniform.mv_buffer,
                                  sizeof(GlobalUniforms::projection));
            if (projection_uniform.mv_buffer.map() != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to map projection matrix buffer");
            }
        }
        ~Collection(void) {}

        // load model
        void load_model(const char *filename)
        {
            bool already_loaded = false;
            for (const auto &name : model_names)
            {
                if (name.compare(filename) == 0) // if same
                {
                    already_loaded = true;
                    break;
                }
            }

            if (!already_loaded)
            {
                // make space for new model
                models.push_back(mv::Model());
                // call model routine _load
                models.back()._load(device, descriptor_allocator, filename);

                // add filename to model_names container
                model_names.push_back(filename);
            }
            return;
        }

        // creates object with specified model data
        void create_object(std::string model_name)
        {
            // ensure model exists
            bool model_exist = false;
            bool model_index = 0;
            for (const auto &model : models)
            {
                if (model.model_name == model_name)
                {
                    model_exist = true;
                    model_index = (&model - &models[0]);
                    break;
                }
            }
            if (!model_exist)
            {
                std::ostringstream oss;
                oss << "[!] Requested creation of object of model type => " << model_name
                    << " failed as that model has never been loaded" << std::endl;
                throw std::runtime_error(oss.str().c_str());
            }

            std::cout << "[+] Creating object of model type => " << model_name << std::endl;

            // create new object element in specified model
            std::unique_ptr<mv::Object> tmp = std::make_unique<mv::Object>();
            models[model_index].objects.push_back(std::move(tmp));

            // create uniform buffer for object
            device->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  &models[model_index].objects.back()->uniform_buffer,
                                  sizeof(mv::Object::matrices));
            if (models[model_index].objects.back()->uniform_buffer.map() != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create uniform buffer for new object");
            }

            VkDescriptorSetLayout uniform_layout = descriptor_allocator->get_layout("uniform_layout");

            // allocate descriptor set for object's model uniform
            descriptor_allocator->allocate_set(descriptor_allocator->get(),
                                               uniform_layout,
                                               models[model_index].objects.back()->model_descriptor);
            descriptor_allocator->update_set(descriptor_allocator->get(),
                                             models[model_index].objects.back()->uniform_buffer.descriptor,
                                             models[model_index].objects.back()->model_descriptor,
                                             0);
            return;
        }

        void update(void)
        {
            for (auto &model : models)
            {
                for (auto &object : model.objects)
                {
                    object->update();
                }
            }
            return;
        }

        void cleanup(void)
        {
            vkDeviceWaitIdle(device->device);

            // cleanup descriptor_allocator
            descriptor_allocator->cleanup();

            // cleanup models & objects buffers
            for (auto &model : models)
            {
                for (auto &object : model.objects)
                {
                    object->uniform_buffer.destroy();
                }
                for (auto &mesh : model._meshes)
                {
                    mesh.cleanup(device);
                }
            }

            view_uniform.mv_buffer.destroy();
            projection_uniform.mv_buffer.destroy();
            return;
        }
    };
};

#endif