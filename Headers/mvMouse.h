#ifndef HEADERS_MVMOUSE_H_
#define HEADERS_MVMOUSE_H_

#define WHEEL_DELTA 120

#include <queue>

namespace mv
{
    class Mouse
    {
        friend class MWindow;

    public:
        class Event
        {
        public:
            enum class Type
            {
                LDown,
                LRelease,
                RDown,
                RRelease,
                MDown, // middle
                MRelease,
                WheelUp,
                WheelDown,
                Move,  // mouse move
                Leave, // leave client area
                Enter, // enter client area
                Invalid
            };

        public:
            Event() noexcept
                : type(Event::Type::Invalid),
                  leftIsPressed(false),
                  rightIsPressed(false),
                  middleIsPressed(false),
                  x(0), y(0)
            {
            }
            Event(Mouse::Event::Type ty, const Mouse &parent) noexcept
                : type(ty),
                  leftIsPressed(parent.leftIsPressed),
                  rightIsPressed(parent.rightIsPressed),
                  middleIsPressed(parent.middleIsPressed),
                  x(parent.x), y(parent.y)
            {
            }
            bool isValid(void) const noexcept
            {
                return type != Event::Type::Invalid;
            }
            Mouse::Event::Type getType(void) const noexcept
            {
                return type;
            }
            std::pair<int, int> getPos(void) const noexcept
            {
                return {x, y};
            }
            int getPosX(void) const noexcept
            {
                return x;
            }
            int getPosY(void) const noexcept
            {
                return y;
            }
            bool isLeftPressed(void) const noexcept
            {
                return leftIsPressed;
            }
            bool isMiddlePressed(void) const noexcept
            {
                return middleIsPressed;
            }
            bool isRightPressed(void) const noexcept
            {
                return rightIsPressed;
            }

        private:
            Type type;
            bool leftIsPressed;
            bool rightIsPressed;
            bool middleIsPressed;
            int x;
            int y;
        };

    public:
        Mouse() = default;
        Mouse(const Mouse &) = delete;
        Mouse &operator=(const Mouse &) = delete;
        std::pair<int, int> getPos() const noexcept;
        std::pair<int, int> getPosDelta(void) const noexcept;
        int getPosX(void) const noexcept;
        int getPosY(void) const noexcept;
        Mouse::Event read(void) noexcept;
        bool isLeftPressed(void) const noexcept;
        bool isMiddlePressed(void) const noexcept;
        bool isRightPressed(void) const noexcept;
        bool isInWindow(void) const noexcept;
        int deltatest = 0;
        bool isEmpty(void) const noexcept
        {
            return buffer.empty();
        }
        void clearState() noexcept;

    private:
        void onMouseMove(int nx, int ny) noexcept;
        void onLeftPress(int nx, int ny) noexcept;
        void onRightPress(int nx, int ny) noexcept;
        void onLeftRelease(int nx, int ny) noexcept;
        void onRightRelease(int nx, int ny) noexcept;
        void onMiddlePress(int nx, int ny) noexcept;
        void onMiddleRelease(int nx, int ny) noexcept;
        void onWheelUp(int nx, int ny) noexcept;                // used internally -- use onDelta
        void onWheelDown(int nx, int ny) noexcept;              // used internally -- use onDelta
        void onWheelDelta(int nx, int ny, int wDelta) noexcept; // use this one for mouse wheel
        void onMouseEnter(void) noexcept;
        void onMouseLeave(void) noexcept;
        void trimBuffer(void) noexcept;

    private:
        static constexpr unsigned int maxBufferSize = 16u;

        int x = 0;
        int y = 0;
        int last_x = 0;
        int last_y = 0;
        int mouse_x_delta = 0;
        int mouse_y_delta = 0;
        int wheelDeltaCarry = 0;

        bool leftIsPressed = false;
        bool middleIsPressed = false;
        bool rightIsPressed = false;
        bool inWindow = false;

        std::queue<Mouse::Event> buffer;
    };
};

#endif