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
        std::shared_ptr<std::vector<mv::Model>> models;
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
            // initialize pointer to models
            models = std::make_shared<std::vector<mv::Model>>();
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

            // exceptions inside of constructor fail to call destructor
            // request handle to active descriptor pool
            try
            {
                mv::Allocator::Container *pool = descriptor_allocator->get(); // get descriptor pool

                VkDescriptorSetLayoutBinding bind_info = {};
                bind_info.binding = 0;
                bind_info.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                bind_info.descriptorCount = 1;
                bind_info.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

                VkDescriptorSetLayoutCreateInfo layout_info = {};
                layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                layout_info.bindingCount = 1;
                layout_info.pBindings = &bind_info;

                // create uniform buffer layout
                descriptor_allocator->create_layout("uniform_layout", layout_info);
                VkDescriptorSetLayout uniform_layout = descriptor_allocator->get_layout("uniform_layout");

                // allocate & update descriptor set for view uniform buffer
                descriptor_allocator->allocate_set(pool, uniform_layout, view_uniform.descriptor);
                descriptor_allocator->update_set(pool, view_uniform.mv_buffer.descriptor, view_uniform.descriptor, 0);

                // allocate & update descriptor set for projection uniform buffer
                descriptor_allocator->allocate_set(pool, uniform_layout, projection_uniform.descriptor);
                descriptor_allocator->update_set(pool, projection_uniform.mv_buffer.descriptor, projection_uniform.descriptor, 0);
            }
            catch (std::exception &e)
            {
                // cleanup buffers
                cleanup();
                throw std::runtime_error(e.what());
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
                models->push_back(mv::Model());
                // call model routine _load
                models->back()._load(device, descriptor_allocator, filename);

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
            for (const auto &model : *(models.get()))
            {
                if (model.model_name == model_name)
                {
                    model_exist = true;
                    model_index = (&model - &models.get()->at(0));
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
            models->at(model_index).objects->push_back(mv::Object());

            // create uniform buffer for object
            device->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                  &models->at(model_index).objects->back().uniform_buffer,
                                  sizeof(mv::Object::matrices));
            if (models->at(model_index).objects->back().uniform_buffer.map() != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create uniform buffer for new object");
            }

            VkDescriptorSetLayout uniform_layout = descriptor_allocator->get_layout("uniform_layout");

            // allocate descriptor set for object's model uniform
            descriptor_allocator->allocate_set(descriptor_allocator->get(),
                                               uniform_layout,
                                               models->at(model_index).objects->back().model_descriptor);
            descriptor_allocator->update_set(descriptor_allocator->get(),
                                             models->at(model_index).objects->back().uniform_buffer.descriptor,
                                             models->at(model_index).objects->back().model_descriptor,
                                             0);
            return;
        }

        void update(void)
        {
            for (auto &model : *(models.get()))
            {
                for (auto &object : *(model.objects.get()))
                {
                    object.update();
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
            for (auto &model : *(models.get()))
            {
                for (auto &object : *(model.objects.get()))
                {
                    object.uniform_buffer.destroy();
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