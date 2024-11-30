# Vulkan

## What's needed to render a triangle?

### 1. Setting up a vulkan instance and selecting a physical device
The application sets up the vulkan API through a `VkInstance`, in which
you specify what API extensions are needed and other things.
Then after creating the instance the application has to select one or more `VkPhysicalDevice`s
to use for operations, maybe depending on it's capabilities (VRAM, dedicated GPU, ...)

### 2. Setup the logical device and queue families
Creation of a `VkDevice` with some `VkPhysicalDeviceFeatures` binded. You need to 
specify which queue families are going to be used.

A queue family is a bunch of `VkQueue`s which can allocate vulan commands
and memory operations asynchronously. Each family type supports a specific
set of operations in its queues (eg. you have a family for graphics, compute and
memory transfer operations).

### 3. Window surface and swap chain
If the application doesn't use offscreen rendering only, a window is needed to render
images to. You can use something like GLFW or SDL.

For rendering a window you need two components; a window surface `VkSurfaceKHR` and a swap
chain `VkSwapchainKHR`. The `KHR` prefix denotes a vulkan extension, these components are
an extension because the vulkan API is platform agnostic, and you need a standarized
window system intefrace (WSI) extension to interact with the window manager.

The surface is a cross-platform abstraction over a window to render something.

The swap chain is a collection of render targets. It's used to ensure that the image
that is currently being rendered is different from the one on screen. This is to assure that
only complete images are shown. Every time a frame is drawn, the swap chain has to provide
a new image to render to. When the frame has ended, the image is returned to the swap
chain to show in the window surface some time later. Depending on the window presentation mode,
the number of render targets is different. That is if you are using double buffering (vsync) or
triple buffering

Additionaly, in some platforms is possible to render directly to the screen framebuffer, avoiding
having to use a window manager, or for implementing one for example.

### 4. Image views and framebuffers
To draw an image acquired from the swap chain, it first has to be wrapped in a `VkImageView` and
a `VkFramebuffer`.

An image view references a specific part of an image that will be used, and a framebuffer
refrences image views that are going to be used for color, depth and stencil targets.

Since there can be many different images in the swap chain, an image view and a framebuffer has to
be created for each one of them and be selected at draw time.

### 5. Render passes
A render pass in Vulkan describes the type of images that are used during rendering operations,
how they will be used, and how their contents are going to be treated.

For example, while rendering a triangle, the application tells Vulkan that it's going to use
a single image as a color target, and it will ve cleared to a solid color before drawing.

A `VkFramebuffer` binds specific images to different slots that are used in a render pass.

### 6. Graphics pipeline
The graphics pipeline in vulkan is set up creating a `VkPipeline` object, which describes
the state of the graphics card, like the viewport size, depth buffer operation and the
programmable state that can be modified using shaders (`VkShaderModule`) that are created
from byte code. The pipeline also needs a render target to be used.

All the configuration of the graphics pipeline needs to be set in advance. If you want to switch
to a different shader or modify your vertex layout, you have to recreate the graphics pipeline.
This means that you have to create a `VkPipeline` for each combination in your render operations
in advance. You can only dynamically change basic configuration, like viewport size and clear
color. All of the state needs to be described explicitly, there is no default color blend state
by default, for example.

### 7. Command pools and command buffers
Many of the operations in Vulkan need to be submited to a queue. These operations first need
to be stored in a `VkCommandBuffer` before they are submitted. These command buffers are
allocated from a `VkCommandPool` that is associated with a specific quque family.

For example, to draw a triangle, the command buffer needs the following operations:
- Begin a render pass
- Bind the graphics pipeline
- Draw three vertices
- End the render pass

Since the image in the framebuffer depends on which specific image the swap chain will give us,
we need to record a command buffer for each possible image and select the right one at draw time.
Otherwise you should have to record the command buffer again every frame (which is dumb)

### 8. Main loop
When you have all the drwaing commands wrapped in a command buffer, the main loop is very 
straightforward. You have to acquire an image from the swap chain with `vikAcquireNextImageKHR`,
select the appropiate command buffer for that image, and submit it to the queue with
`vkQueueSubmit`. Finally, the image is sent to the swap chain again for presentation on the
window surface with `vkQueuePresentKHR`.

All the operations that are submited to the queues are executed asynchronously, so you
have to use synchronization primitives to ensure a correct order of execution. When you
execute the command buffer you first need to wait for the image acquisition to finish, or you
might render to an image that is stil being presented on the screen. So in turn the
`vkQueuePresentKHR` call needs to wait for the rendering to be finished.


### Summary
In short, to draw the first triangle you need to:
- Create a `VkInstance`
- Select a supported graphics card (`VkPhysicalDevice`)
- Create a logical `VkDevice` and a `VkQueue` for drawing and presentation
- Create a window, window surface and swap chain
- Wrap the swap chain images in a `VkImageView`
- Create a render pass that specifies the render targets and how they will be used
- Create some framebuffers for the render pass
- Set up the graphics pipeline
- Allocate a command buffer with draw commands for every single swap chain image
- Draw frames by acquiring images, submitting the appropiate draw command buffers and
returning the images to the swap chain

Additionaly, a real world program needs more steps like allocation vertex buffers, creating
uniform buffers and uploading texture images.
