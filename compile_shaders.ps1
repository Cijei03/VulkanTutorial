cd shaders
glslangValidator --target-env vulkan1.3 -e main -o gbuffer_generation_pass_vert.spv sources/gbuffer_generation_pass.vert
glslangValidator --target-env vulkan1.3 -e main -o gbuffer_generation_pass_frag.spv sources/gbuffer_generation_pass.frag
glslangValidator --target-env vulkan1.3 -e main -o shadow_map_generation_vert.spv sources/shadow_map_generation.vert
glslangValidator --target-env vulkan1.3 -e main -o shadow_map_generation_frag.spv sources/shadow_map_generation.frag
glslangValidator -R --target-env vulkan1.3 -e main -o deferred_shading_pass_vert.spv sources/deferred_shading_pass.vert
glslangValidator --target-env vulkan1.3 -e main -o deferred_shading_pass_frag.spv sources/deferred_shading_pass.frag
cd ..