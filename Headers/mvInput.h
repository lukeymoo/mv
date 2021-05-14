#pragma once

// clang-format off
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
// clang-format on

// #define WHEEL_DELTA 120
#define WHEEL_DELTA 1

#include <bitset>
#include <queue>

// Keyboard
namespace mv
{
    class Keyboard
    {
      public:
        struct Event
        {
            enum Type
            {
                eInvalid = 0,
                ePress,
                eRelease,
            };
            Event(Type p_Type, int p_Code) noexcept
            {
                this->type = p_Type;
                this->code = p_Code;
            }
            ~Event()
            {
            }
            Type type = Event::Type::eInvalid;
            int code = GLFW_KEY_UNKNOWN;

            inline bool isPress(void) const noexcept
            {
                return type == Type::ePress;
            }
            inline bool isRelease(void) const noexcept
            {
                return type == Type::eRelease;
            }
            inline bool isValid(void) const noexcept
            {
                return type != Type::eInvalid;
            }
        }; // end event structure

        const uint32_t maxBufferSize = 24;

        std::bitset<350> keyStates;
        std::queue<struct mv::Keyboard::Event> keyBuffer;

        template <typename T> void trimBuffer(std::queue<T> &p_Buffer, uint32_t p_MaxSize);

        struct mv::Keyboard::Event read(void);

        void onKeyPress(int p_Code);
        void onKeyRelease(int p_Code);
        void clearState(void);
        bool isKeyState(int p_Code);
        bool isKey(GLFWwindow *p_GLFWwindow, int p_Code);

    }; // namespace keyboard
};     // namespace mv

// Mouse
namespace mv
{
    class Mouse
    {
      public:
        struct Event
        {
            enum Type
            {
                eInvalid = 0,
                eLeftDown,
                eLeftRelease,
                eRightDown,
                eRightRelease,
                eMiddleDown,
                eMiddleRelease,
                eWheelUp,
                eWheelDown,
                eMove,
                eEnter,
                eLeave,
            };

            // Event(Type p_EventType, int p_CursorX, int p_CursorY, bool p_LeftButtonStatus, bool p_MiddleButtonStatus,
            //       bool p_RightButtonStatus) noexcept
            // {
            //     this->type = p_EventType;
            //     this->x = p_CursorX;
            //     this->y = p_CursorY;
            //     this->isLeftPressed = p_LeftButtonStatus;
            //     this->isMiddlePressed = p_MiddleButtonStatus;
            //     this->isRightPressed = p_RightButtonStatus;
            //     return;
            // }
            // ~Event()
            // {
            // }

            Type type = Type::eInvalid;
            int x = 0;
            int y = 0;

            bool isLeftPressed = false;
            bool isMiddlePressed = false;
            bool isRightPressed = false;

            inline bool isValid(void) const noexcept
            {
                return (type != Type::eInvalid);
            }
        }; // end event

        template <typename T> void trimBuffer(std::queue<T> &p_Buffer, uint32_t p_MaxSize);

        const uint32_t maxBufferSize = 24;
        std::queue<struct mv::Mouse::Event> mouseBuffer;

        enum DeltaStyles
        {
            eFromCenter = 0,
            eFromLastPosition
        };
        DeltaStyles deltaStyle = DeltaStyles::eFromCenter;

        int lastX = 0;
        int lastY = 0;
        int currentX = 0;
        int currentY = 0;
        int deltaX = 0;
        int deltaY = 0;
        int centerX = 0;
        int centerY = 0;

        bool isLeftPressed = false;
        bool isMiddlePressed = false;
        bool isRightPressed = false;

        bool isDragging = false;
        int dragStartx = 0;
        int dragStarty = 0;
        int dragDeltaX = 0;
        int dragDeltaY = 0;

        float storedOrbit = 0.0f;
        float storedPitch = 0.0f;

        struct Mouse::Event read(void) noexcept;
        void update(int p_NewX, int p_NewY) noexcept;

        void onLeftPress(void) noexcept;
        void onLeftRelease(void) noexcept;
        void onRightPress(void) noexcept;
        void onRightRelease(void) noexcept;
        void onMiddlePress(void) noexcept;
        void onMiddleRelease(void) noexcept;

        void onWheelUp(int p_NewX, int p_NewY) noexcept;
        void onWheelDown(int p_NewX, int p_NewY) noexcept;
        void calculateDelta(void) noexcept;
        void startDrag(void) noexcept;
        void startDrag(float p_StartOrbit, float p_StartPitch) noexcept;
        void endDrag(void) noexcept;
        void clear(void) noexcept;
    }; // namespace mouse
};     // namespace mv
