Base structure of Vulkan application.

This handles the majority of boiler plate of creating...
instance, debugging callback, logical device, swapchain(images + views), render pass,
framebuffer, command buffers & sync objects.

Whatever application that extends this will need to create + initialize buffers along with
loading any assets/vertices & then allocating a descriptor pool, layout, set and finally
fill in the command buffers.

Tie it all together with a pipeline & the app is ready to go