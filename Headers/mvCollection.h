#ifndef HEADERS_MVCOLLECTION_H_
#define HEADERS_MVCOLLECTION_H_

#include <ostream>

#include "mvAllocator.h"
#include "mvDevice.h"
#include "mvModel.h"

namespace mv {
  struct uniform_object {
    alignas(16) glm::mat4 matrix = glm::mat4(1.0);
    mv::Buffer mv_buffer;
    vk::DescriptorSet descriptor;
  };

  struct Collection {
    // owns
    std::unique_ptr<std::vector<mv::Model>> models;

    // infos
    std::vector<std::string> model_names;

    // view & projection matrix objects
    std::unique_ptr<mv::uniform_object> view_uniform;
    std::unique_ptr<mv::uniform_object> projection_uniform;

    Collection(const mv::Device &m_dvc) {

      // initialize containers
      view_uniform = std::make_unique<mv::uniform_object>();
      projection_uniform = std::make_unique<mv::uniform_object>();
      models = std::make_unique<std::vector<mv::Model>>();

      // create uniform buffer for view matrix
      m_dvc.create_buffer(vk::BufferUsageFlagBits::eUniformBuffer,
                          vk::MemoryPropertyFlagBits::eHostVisible |
                              vk::MemoryPropertyFlagBits::eHostCoherent,
                          &view_uniform->mv_buffer, sizeof(uniform_object));
      view_uniform->mv_buffer.map(m_dvc);

      // create uniform buffer for projection matrix
      m_dvc.create_buffer(vk::BufferUsageFlagBits::eUniformBuffer,
                          vk::MemoryPropertyFlagBits::eHostVisible |
                              vk::MemoryPropertyFlagBits::eHostCoherent,
                          &projection_uniform->mv_buffer, sizeof(uniform_object));
      projection_uniform->mv_buffer.map(m_dvc);
    }
    ~Collection(void) {
    }

    // load model
    void load_model(const mv::Device &m_dvc, mv::Allocator &m_alloc, const char *filename) {
      bool already_loaded = false;
      for (const auto &name : model_names) {
        if (name.compare(filename) == 0) // if same
        {
          std::cout << "[-] Skipping already loaded model name => " << filename << "\n";
          already_loaded = true;
          break;
        }
      }

      if (!already_loaded) {
        std::cout << "[+] Loading model => " << filename << "\n";
        // make space for new model
        models->push_back(mv::Model());
        // call model routine _load
        models->back()._load(m_dvc, m_alloc, filename);

        // add filename to model_names container
        model_names.push_back(filename);
      }
      return;
    }

    // creates object with specified model data
    void create_object(const mv::Device &m_dvc, mv::Allocator &m_alloc, std::string model_name) {

      if (!models)
        throw std::runtime_error("Requested to create object but models container never "
                                 "initialized :: collection handler");

      // ensure model exists
      bool model_exist = false;
      bool model_index = 0;

      for (auto &model : *models) {
        if (model.model_name == model_name) {
          model_exist = true;
          model_index = (&model - &(*models)[0]);
        }
      }

      if (!model_exist) {
        std::ostringstream oss;
        oss << "[!] Requested creation of object of model type => " << model_name
            << " failed as that model has never been loaded" << std::endl;
        throw std::runtime_error(oss.str().c_str());
      }

      std::cout << "[+] Creating object of model type => " << model_name << std::endl;

      // create new object element in specified model
      models->at(model_index).objects->push_back(mv::Object());

      std::cout << "\t -- Model type object count is now => "
                << models->at(model_index).objects->size() << "\n";

      // create uniform buffer for object
      m_dvc.create_buffer(
          vk::BufferUsageFlagBits::eUniformBuffer,
          vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
          &models->at(model_index).objects->back().uniform_buffer, sizeof(mv::Object::matrices));
      models->at(model_index).objects->back().uniform_buffer.map(m_dvc);

      vk::DescriptorSetLayout uniform_layout = m_alloc.get_layout("uniform_layout");

      // allocate descriptor set for object's model uniform
      m_alloc.allocate_set(m_dvc, m_alloc.get(), uniform_layout,
                           models->at(model_index).objects->back().model_descriptor);
      m_alloc.update_set(m_dvc, m_alloc.get(),
                         models->at(model_index).objects->back().uniform_buffer.descriptor,
                         models->at(model_index).objects->back().model_descriptor, 0);
      return;
    }

    void update(void) {
      if (!models)
        throw std::runtime_error("Collection handler was requested to update but models never "
                                 "initialized :: collection handler");
      for (auto &model : *models) {
        if (!model.objects) {
          std::ostringstream oss;
          oss << "In collection handler update of objects => " << model.model_name
              << " object container never initialized\n";
          throw std::runtime_error(oss.str());
        }
        for (auto &object : *model.objects) {
          object.update();
        }
      }
      return;
    }

    void cleanup(const mv::Device &m_dvc, mv::Allocator &m_alloc) {

      m_dvc.logical_device->waitIdle();

      // cleanup descriptor_allocator
      m_alloc.cleanup(m_dvc);

      // cleanup models & objects buffers
      for (auto &model : *models) {
        if (!model.objects) {
          std::ostringstream oss;
          oss << "In collection handler cleanup of objects => " << model.model_name
              << " object container never initialized\n";
          throw std::runtime_error(oss.str());
        }
        if (!model._meshes) {
          std::ostringstream oss;
          oss << "In collection handler cleanup of objects => " << model.model_name
              << " mesh container never initialized\n";
          throw std::runtime_error(oss.str());
        }
        for (auto &object : *model.objects) {
          object.uniform_buffer.destroy(m_dvc);
        }
        for (auto &mesh : *model._meshes) {
          mesh.cleanup(m_dvc);
        }
      }

      view_uniform->mv_buffer.destroy(m_dvc);
      projection_uniform->mv_buffer.destroy(m_dvc);
      return;
    }
  };
}; // namespace mv

#endif