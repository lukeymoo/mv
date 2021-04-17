#ifndef HEADERS_MVENGINE_H_
#define HEADERS_MVENGINE_H_

#include "mvWindow.h"

namespace mv
{
    class Engine : public mv::Window
    {
    public:
        Engine(int w, int h, const char *title);
        ~Engine();

        void go(void);
        void recordCommandBuffer(uint32_t imageIndex);
    private:
        void createDescriptorPool(void);
        void createDescriptorLayout(void);
        void createDescriptorSets(void);
    };
};

#endif