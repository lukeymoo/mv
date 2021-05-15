Most of the boilerplate for this project was established at <a href='https://github.com/lukeymoo/mv/tree/440b7d80a350951a198a85682adc2c3e45cc5e1e'>about this time.</a>

Still have no clue what type of game this may end up being used in or if I'll even finish it but it's great learning experience.
Lastest in app `footage` if you can even call it that :D

Character model is a rip from Old school runescape I found online; The room looking mesh is a model from https://vulkan-tutorial.com/ -- great starter place for newbies; Also, <a href='https://github.com/SaschaWillems/'>Sascha Willems</a> has great examples that help digest the many parts of Vulkan api.

The rooms are dynamically created with the space bar using an allocator I made( just creates pools when needed & tracks them for deletion at program end, will do more in future :] ).

The room's mesh is loaded once & objects are spawned at random locations in a given range using std::random_device;

- Handler that loads models and creates buffers for them is inefficient, creating a separate buffer for each mesh(some as small as 64 bytes & there can be dozens of meshes in a model)..to be fixed later
- Basically every buffer except vertex, index & textures are managed with host visible, host coherent memory(another inefficiency)..to be fixed
- Rendering meshes is not instanced & each mesh in a model currently requires a draw call(using osrs model at 1200 objects there are an absurd amount of draw calls ~10k if im not mistaken)..to be fixed

lots more to do...

![snapshot](https://raw.githubusercontent.com/lukeymoo/mv/development/snapshots/Screenshot%20from%202021-05-04%2002-36-47.png)

Turns out I've had a bug in my aspect ratio for the entire duration of this project; Aspect ratio was 1.0 due to using `swapchain.height/swapchain.height`
instead of the appropriate `swapchain.width/swapchain.height`

![Lastest snapshot](https://raw.githubusercontent.com/lukeymoo/mv/main/snapshots/Screenshot%20from%202021-05-15%2001-08-24.png)
