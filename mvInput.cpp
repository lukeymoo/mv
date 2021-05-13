#include "mvInput.h"

/*
  KEYBOARD METHODS
*/
template <typename T> void mv::Keyboard::trimBuffer(std::queue<T> &p_Buffer, uint32_t p_MaxSize)
{
    while (p_Buffer.size() > max_size)
    {
        p_Buffer.pop();
    }
    return;
}

mv::Keyboard::Event mv::Keyboard::read(void)
{
    Keyboard::Event e(Keyboard::Event::Type::eInvalid, GLFW_KEY_UNKNOWN);
    if (!keyBuffer.empty())
    {
        e = keyBuffer.front();
        keyBuffer.pop();
    }
    return e;
}

void mv::Keyboard::onKeyPress(int p_Code)
{
    keyStates[p_Code] = true;
    keyBuffer.push(Keyboard::Event(Keyboard::Event::Type::ePress, p_Code));
    trimBuffer(keyBuffer, mv::Keyboard::maxBufferSize);
    return;
}

void mv::Keyboard::onKeyRelease(int p_Code)
{
    keyStates[p_Code] = false;
    keyBuffer.push(Keyboard::Event(Keyboard::Event::Type::eRelease, p_Code));
    trimBuffer(keyBuffer, mv::Keyboard::maxBufferSize);
    return;
}

void mv::Keyboard::clearState(void)
{
    keyStates.reset();
    return;
}

bool mv::Keyboard::isKeyState(int p_Code)
{
    // sanity check
    if (p_Code < 0)
    {
        return false;
    }
    return keyStates[p_Code];
}

bool mv::Keyboard::isKey(GLFWwindow *p_GLFWwindow, int p_Code)
{
    // sanity check
    if (p_Code < 0)
    {
        return false;
    }
    return glfwGetKey(p_GLFWwindow, p_Code);
}

/*
  MOUSE METHODS
*/
template <typename T> void mv::Mouse::trimBuffer(std::queue<T> &p_Buffer, uint32_t p_MaxSize)
{
    while (p_Buffer.size() > p_MaxSize)
    {
        p_Buffer.pop();
    }
    return;
}

void mv::Mouse::update(int p_NewX, int p_NewY) noexcept
{
    lastX = currentX;
    lastY = currentY;
    currentX = p_NewX;
    currentY = p_NewY;
    calculateDelta();
    return;
}

struct mv::Mouse::Event mv::Mouse::read(void) noexcept
{
    Mouse::Event e(Mouse::Event::Type::eInvalid, 0, 0, false, false, false);
    if (!mouseBuffer.empty())
    {
        e = mouseBuffer.front();
        mouseBuffer.pop();
    }
    return e;
}

void mv::Mouse::onLeftPress(void) noexcept
{
    isLeftPressed = true;
    mouseBuffer.push(Event{
        .type = Event::Type::eLeftDown,
        .x = currentX,
        .y = currentY,
        .isLeftPressed = isLeftPressed,
        .isMiddlePressed = isMiddlePressed,
        .isRightPressed = isRightPressed,
    });
    trimBuffer(mouseBuffer, maxBufferSize);
    return;
}
void mv::Mouse::onLeftRelease(void) noexcept
{
    isLeftPressed = false;
    mouseBuffer.push(Event{
        .type = Event::Type::eLeftRelease,
        .x = currentX,
        .y = currentY,
        .isLeftPressed = isLeftPressed,
        .isMiddlePressed = isMiddlePressed,
        .isRightPressed = isRightPressed,
    });
    trimBuffer(mouseBuffer, maxBufferSize);
    return;
}

void mv::Mouse::onRightPress(void) noexcept
{
    isRightPressed = true;
    mouseBuffer.push(Event{
        .type = Event::Type::eRightDown,
        .x = currentX,
        .y = currentY,
        .isLeftPressed = isLeftPressed,
        .isMiddlePressed = isMiddlePressed,
        .isRightPressed = isRightPressed,
    });
    trimBuffer(mouseBuffer, maxBufferSize);
    return;
}
void mv::Mouse::onRightRelease(void) noexcept
{
    isRightPressed = false;
    mouseBuffer.push(Event{
        .type = Event::Type::eRightRelease,
        .x = currentX,
        .y = currentY,
        .isLeftPressed = isLeftPressed,
        .isMiddlePressed = isMiddlePressed,
        .isRightPressed = isRightPressed,
    });
    trimBuffer(mouseBuffer, maxBufferSize);
    return;
}

void mv::Mouse::onMiddlePress(void) noexcept
{
    isMiddlePressed = true;
    mouseBuffer.push(Event{
        .type = Event::Type::eMiddleDown,
        .x = currentX,
        .y = currentY,
        .isLeftPressed = isLeftPressed,
        .isMiddlePressed = isMiddlePressed,
        .isRightPressed = isRightPressed,
    });
    trimBuffer(mouseBuffer, maxBufferSize);
    return;
}
void mv::Mouse::onMiddleRelease(void) noexcept
{
    isMiddlePressed = false;
    mouseBuffer.push(Event{
        .type = Event::Type::eMiddleRelease,
        .x = currentX,
        .y = currentY,
        .isLeftPressed = isLeftPressed,
        .isMiddlePressed = isMiddlePressed,
        .isRightPressed = isRightPressed,
    });
    trimBuffer(mouseBuffer, maxBufferSize);
    return;
}

// used internally -- use onDelta
void mv::Mouse::onWheelUp(int p_NewX, int p_NewY) noexcept
{
    lastX = currentX;
    lastY = currentY;
    currentX = p_NewX;
    currentY = p_NewY;
    calculateDelta();
    mouseBuffer.push(Event{
        .type = Event::Type::eWheelUp,
        .x = currentX,
        .y = currentY,
        .isLeftPressed = isLeftPressed,
        .isMiddlePressed = isMiddlePressed,
        .isRightPressed = isRightPressed,
    });
    trimBuffer(mouseBuffer, maxBufferSize);
    return;
}
// used internally -- use onDelta
void mv::Mouse::onWheelDown(int p_NewX, int p_NewY) noexcept
{
    lastX = currentX;
    lastY = currentY;
    currentX = p_NewX;
    currentY = p_NewY;
    calculateDelta();
    mouseBuffer.push(Event{
        .type = Event::Type::eWheelDown,
        .x = currentX,
        .y = currentY,
        .isLeftPressed = isLeftPressed,
        .isMiddlePressed = isMiddlePressed,
        .isRightPressed = isRightPressed,
    });
    trimBuffer(mouseBuffer, maxBufferSize);
    return;
}

void mv::Mouse::calculateDelta(void) noexcept
{
    // if dragging calculate drag delta
    if (isDragging)
    {
        dragDeltaX = currentX - lastX;
        dragDeltaY = currentY - lastY;
    }
    if (deltaStyle == DeltaStyles::eFromCenter)
    {
        deltaX = currentX - centerX;
        deltaY = currentY - centerY;
    }
    else if (deltaStyle == DeltaStyles::eFromLastPosition)
    {
        deltaX = currentX - lastX;
        deltaY = currentY - lastY;
    }
    return;
}

void mv::Mouse::startDrag(void) noexcept
{
    dragStartx = currentX;
    dragStarty = currentY;
    isDragging = true;
    return;
}

void mv::Mouse::startDrag(float p_StartOrbit, float p_StartPitch) noexcept
{
    storedOrbit = p_StartOrbit;
    storedPitch = p_StartPitch;
    dragStartx = currentX;
    dragStarty = currentY;
    isDragging = true;
    return;
}

void mv::Mouse::endDrag(void) noexcept
{
    storedOrbit = 0.0f;
    storedPitch = 0.0f;
    dragStartx = 0;
    dragStarty = 0;
    dragDeltaX = 0;
    dragDeltaY = 0;
    isDragging = false;
    return;
}

void mv::Mouse::clear(void) noexcept
{
    deltaX = 0;
    deltaY = 0;
    dragDeltaX = 0;
    dragDeltaY = 0;
    return;
}