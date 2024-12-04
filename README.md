VulkanTutorial is testing project where I have tried to write something in Vulkan. 

![alt text](https://cijei03.github.io/resources/vulkan_tutorial.png)

As you can see on the screenshot, rendered scene contains plane, elf and Vulkan logo models. It is rendered using deferred shading technique. Lighting of the scene provides directional light withing basic Phong shading model.
More realistic look has been achieved by implementing shadows generation within Variance Shadow Mapping technique.

This has been writed and tested only against AMD Radeon RX 6700XT hardware, so on other hardware it may not work. SPIR-V shaders has been generated from GLSL sources.
