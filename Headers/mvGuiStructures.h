#pragma once

// clang-format off
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "imgui-1.82/misc/cpp/imgui_stdlib.h"
// clang-format on
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <any>
#include <chrono>
#include <cxxabi.h>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <thread>
#include <unordered_map>

struct Camera;
class MapHandler;
class LogHandler;
enum class LogHandlerMessagePriority;

class GuiHandler;
extern void noShow (GuiHandler *);
extern void showOpenDialog (GuiHandler *);
extern void showSaveDialog (GuiHandler *);
extern void showQuitDialog (GuiHandler *);
extern void selectNewTerrainFile (GuiHandler *);
extern std::vector<std::pair<int, std::string>> splitPath (std::string p_Path);

template <typename T>
constexpr auto
type_name () noexcept
{
  std::string_view name = "Error: unsupported compiler", prefix, suffix;
#ifdef __clang__
  name = __PRETTY_FUNCTION__;
  prefix = "auto type_name() [T = ";
  suffix = "]";
#elif defined(__GNUC__)
  name = __PRETTY_FUNCTION__;
  prefix = "constexpr auto type_name() [with T = ";
  suffix = "]";
#elif defined(_MSC_VER)
  name = __FUNCSIG__;
  prefix = "auto __cdecl type_name<";
  suffix = ">(void) noexcept";
#endif
  name.remove_prefix (prefix.size ());
  name.remove_suffix (suffix.size ());
  return name;
}

namespace gui
{
  enum ModalType
  {
    eFileMenu = 0,
    eDebug,
    eMap,
    eCamera
  };

  // Used only for template, all modals must inherit
  struct BaseModal
  {
  public:
    std::string type;
    BaseModal () = default;
    ~BaseModal () = default;
  };

  struct FileMenu : public BaseModal
  {
  public:
    std::string type = typeid (*this).name ();
    struct OpenModal
    {
      bool isOpen = false;
      ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove;
    } openModal;

    struct SaveAsModal
    {
      bool isOpen = false;
      ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                               | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    } saveAsModal;

    struct QuitModal
    {
      bool isOpen = false;
      ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                               | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    } quitModal;
  };

  struct CameraModal : public BaseModal
  {
    std::string type = typeid (*this).name ();
    const int width = 300;
    const int height = 150;
    std::string xInputBuffer;
    std::string yInputBuffer;
    std::string zInputBuffer;
    void *userData;
    ImGuiInputTextFlags inputFlags
        = ImGuiInputTextFlags_NoHorizontalScroll | ImGuiInputTextFlags_EnterReturnsTrue
          | ImGuiInputTextFlags_CallbackCharFilter | ImGuiColorEditFlags_NoLabel;
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                                   | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar;
  };

  struct DebugModal : public BaseModal
  {
    std::string type = typeid (*this).name ();
    const int width = 800;
    const int height = 600;
    bool isOpen = true;
    bool autoScroll = true;
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
  };

  struct MapModal : public BaseModal
  {
    std::string type = typeid (*this).name ();
    struct
    {
      bool isSelected = false;
      std::string filename = "None";
      std::string fullPath = "None";
      ImGuiSelectableFlags flags = ImGuiSelectableFlags_AllowDoubleClick;
    } terrainItem; // Selectable flags

    struct CameraItem
    {
      enum Type
      {
        eUndefined,
        eFreeLook,
        eFirstPerson,
        eThirdPerson,
        eIsometric,
      };
      // { TYPE, { ITEM TEXT, SELECTED? } }
      std::unordered_map<CameraItem::Type, std::pair<std::string, bool>> typeMap = {
        { eUndefined, { "Undefined", false } },      { eFreeLook, { "Free Look", false } },
        { eFirstPerson, { "First Person", false } }, { eThirdPerson, { "Third Person", false } },
        { eIsometric, { "Isometric", false } },
      };
      Type selectedType = eUndefined;

      inline std::string
      getTypeName (Type p_EnumType)
      {
        return typeMap.at (p_EnumType).first;
      }

      inline bool
      getIsSelected (Type p_EnumType)
      {
        return typeMap.at (p_EnumType).second;
      }

      inline bool *
      getIsSelectedRef (Type p_EnumType)
      {
        return &typeMap.at (p_EnumType).second;
      }
      inline void
      select (Type p_EnumType)
      {
        deselectAllBut (p_EnumType);
        for (auto &pair : typeMap)
          {
            if (pair.first == p_EnumType)
              pair.second.second = true;
          }
        return;
      }
      inline void
      deselectAll (void)
      {
        for (auto &pair : typeMap)
          {
            pair.second.second = false;
          }
        return;
      }
      inline void
      deselectAllBut (Type p_EnumType)
      {
        for (auto &pair : typeMap)
          {
            if (pair.first == p_EnumType)
              continue;
            pair.second.second = false;
          }
      }
    } cameraItem;

    struct
    {
      std::string type = typeid (*this).name ();
      bool isOpen;
      std::filesystem::path currentDirectory = std::filesystem::current_path ();
      ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar
                                     | ImGuiWindowFlags_NoScrollWithMouse
                                     | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    } selectTerrainModal;

    const int width = 300;
    const int height = 150;
    // Parent modal
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                                   | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar;
  };

  // Not a modal
  // Holds current directory
  struct FileSystem
  {
    enum FileType
    {
      eFile = 0,
      eDirectory
    };
    std::filesystem::path currentDirectory = std::filesystem::current_path ();

    std::vector<std::pair<FileType, std::string>> listDirectory (std::filesystem::path p_Path);
  };

  struct ModalFunction
  {
    void (*toExec) (GuiHandler *p_This) = noShow;

  public:
    // Display no modal
    void
    operator() (GuiHandler *p_This)
    {
      toExec (p_This);
      return;
    }

    // For testing which child is `in use.`
    template <typename P>
    bool
    operator== (P *ptr) const
    {
      if (toExec == ptr)
        {
          return true;
        }
      return false;
    }

    // Allow copy assignment
    template <typename P>
    ModalFunction operator= (P *ptr);
  };

  // For output std::filesystem data as  ImGui::Selectable
  enum ListType
  {
    eHeightMaps = 0,
    eMapFiles,
  };

  /*
      Methods for GuiModal map
  */
  template <class T, class U>
  concept Derived = std::is_base_of<U, T>::value;

  template <Derived<BaseModal> T>
  class ModalEntry : public T
  {
  public:
    ModalEntry () = default;
    ~ModalEntry () = default;
  };

  using UMAP = std::unordered_map<ModalType, std::any> &;

  // Returns reference to modal object
  template <typename T>
  auto &
  getModal (UMAP umap)
  {
    std::string modal_param_mangled = typeid (ModalEntry<T>).name ();
    std::string modal_param_demangled;
    modal_param_demangled.resize (modal_param_mangled.size () * 2);
    size_t modal_param_size = modal_param_demangled.size ();

    int statusp = 0;
    __cxxabiv1::__cxa_demangle (
        modal_param_mangled.c_str (), modal_param_demangled.data (), &modal_param_size, &statusp);
    if (statusp != 0)
      throw std::runtime_error ("Demangling error, requested param");

    for (auto &p : umap)
      {
        std::string modal_entry_mangled = p.second.type ().name ();
        std::string modal_entry_demangled;
        modal_entry_demangled.resize (modal_entry_mangled.size () * 2);
        size_t modal_entry_size = modal_entry_demangled.size ();
        int status = 0;

        __cxxabiv1::__cxa_demangle (modal_entry_mangled.c_str (),
                                    modal_entry_demangled.data (),
                                    &modal_entry_size,
                                    &status);
        if (status != 0)
          throw std::runtime_error ("Demangling error, modal entry");

        if (modal_entry_demangled.compare (modal_param_demangled) == 0)
          {
            return std::any_cast<ModalEntry<T> &> (p.second);
          }
      }
    throw std::runtime_error ("Requested invalid type of ModalEntry<T> : T = "
                              + std::string (type_name<T> ()));
  }
}; // namespace gui