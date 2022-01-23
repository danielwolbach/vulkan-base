#define GLFW_INCLUDE_VULKAN

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

int main() {

    const bool ENABLE_VALIDATION = true;
    const uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    const char* const validation_layer_names[] = { "VK_LAYER_KHRONOS_validation" };
    uint32_t validation_layer_count = sizeof validation_layer_names / sizeof * validation_layer_names;

    // Create a window (using GLFW).

    GLFWwindow* window = NULL;

    {
        if (!glfwInit()) {
            fprintf(stderr, "error (glfw): Failed to initialize.\n");
            return 1;
        }

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(1280, 720, "Vulkan Base", NULL, NULL);

        if (window == NULL) {
            fprintf(stderr, "error: (glfw): Failed to create a window.\n");
            return 1;
        }
    }

    // Create an instance.

    VkInstance instance = VK_NULL_HANDLE;

    {
        // Select layers and extensions.

        uint32_t enabled_extension_count = 0;
        const char* const* enabled_extension_names = glfwGetRequiredInstanceExtensions(&enabled_extension_count);

        uint32_t enabled_layer_count = ENABLE_VALIDATION ? validation_layer_count : 0;
        const char* const* enabled_layer_names = validation_layer_names;

        // Check layer support.

        {
            uint32_t instance_layer_count = 0;
            vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL);
            VkLayerProperties* instance_layer_properties = malloc(instance_layer_count * sizeof * instance_layer_properties);
            vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layer_properties);

            if (instance_layer_properties == NULL) {
                fprintf(stderr, "error (vulkan): Failed to fetch instance layer properties.\n");
                return 1;
            }

            for (uint32_t i = 0; i < enabled_layer_count; i++) {
                bool layer_available = false;

                for (uint32_t j = 0; j < instance_layer_count; j++) {
                    if (strcmp(enabled_layer_names[i], instance_layer_properties[j].layerName) == 0) {
                        layer_available = true;
                        break;
                    }
                }

                if (!layer_available) {
                    fprintf(stderr, "error (vulkan): Requested instance layer (name: \"%s\") is not available.\n", enabled_layer_names[i]);
                    return 1;
                }
            }

            free(instance_layer_properties);
        }

        // Check extension support.

        {
            uint32_t instance_extension_count = 0;
            vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, NULL);
            VkExtensionProperties* instance_extension_properties = malloc(instance_extension_count * sizeof * instance_extension_properties);
            vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, instance_extension_properties);

            if (instance_extension_properties == NULL) {
                fprintf(stderr, "error (vulkan): Failed to fetch instance layer properties.\n");
                return 1;
            }

            for (uint32_t i = 0; i < enabled_extension_count; i++) {
                bool extension_available = false;

                for (uint32_t j = 0; j < instance_extension_count; j++) {
                    if (strcmp(enabled_extension_names[i], instance_extension_properties[j].extensionName) == 0) {
                        extension_available = true;
                        break;
                    }
                }

                if (!extension_available) {
                    fprintf(stderr, "error (vulkan): Requested instance extension (name: \"%s\") is not available.\n", enabled_extension_names[i]);
                    return 1;
                }
            }

            free(instance_extension_properties);
        }

        // Configure the instance.

        const VkApplicationInfo application_info = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = NULL,
            .pApplicationName = "Vulkan Application",
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .pEngineName = "Vulkan Engine",
            .engineVersion = VK_MAKE_VERSION(0, 1, 0),
            .apiVersion = VK_API_VERSION_1_0,
        };

        const VkInstanceCreateInfo instance_create_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .pApplicationInfo = &application_info,
            .enabledLayerCount = validation_layer_count,
            .ppEnabledLayerNames = enabled_layer_names,
            .enabledExtensionCount = enabled_extension_count,
            .ppEnabledExtensionNames = enabled_extension_names,
        };

        // Create the instance.

        const VkResult result = vkCreateInstance(&instance_create_info, NULL, &instance);

        if (result != VK_SUCCESS) {
            fprintf(stderr, "error (vulkan): Failed to create an instance.\n");
            return 1;
        }
    }

    // Create a surface.

    VkSurfaceKHR surface = VK_NULL_HANDLE;

    {
        const VkResult result = glfwCreateWindowSurface(instance, window, NULL, &surface);

        if (result != VK_SUCCESS) {
            fprintf(stderr, "error (vulkan): Failed to create a surface.\n");
            return 1;
        }
    }

    // Choose a physical device.

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;

    {
        // Fetch (all) physical devices.

        uint32_t physical_device_count = 0;
        vkEnumeratePhysicalDevices(instance, &physical_device_count, NULL);

        if (physical_device_count < 1) {
            fprintf(stderr, "error (vulkan): No physical devices are available.\n");
            return 1;
        }

        VkPhysicalDevice* physical_devices = malloc(physical_device_count * sizeof * physical_devices);
        const VkResult result = vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices);

        if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || physical_devices == NULL) {
            fprintf(stderr, "error (vulkan): Failed to fetch physical devices.\n");
            free(physical_devices);
            return 1;
        }

        // Find the most suitable physical device.

        physical_device = physical_devices[0];

        for (uint32_t i = 0; i < physical_device_count; i++) {
            VkPhysicalDeviceProperties physical_device_properties;
            vkGetPhysicalDeviceProperties(physical_devices[i], &physical_device_properties);

            if (physical_device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physical_device = physical_devices[i];
                break;
            }
        }

        // Clean up.

        free(physical_devices);
    }

    // Find queue families.

    uint32_t graphics_queue_family_index = 0;

    {
        // Fetch the properties of all queue families.

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);

        if (queue_family_count < 1) {
            fprintf(stderr, "error (vulkan): No queue families are available.\n");
            return 1;
        }

        VkQueueFamilyProperties* queue_family_properties = malloc(queue_family_count * sizeof * queue_family_properties);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_family_properties);

        if (queue_family_properties == NULL) {
            fprintf(stderr, "error (vulkan): Failed to fetch queue family properties.\n");
            return 1;
        }

        // Find all queue families that are needed.

        bool graphics_queue_family_found = false;

        for (uint32_t i = 0; i < queue_family_count; i++) {
            VkBool32 queue_family_supports_presentation = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &queue_family_supports_presentation);

            if (!graphics_queue_family_found && queue_family_supports_presentation && queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_queue_family_index = i;
                graphics_queue_family_found = true;
            }

            if (graphics_queue_family_found) {
                break;
            }
        }

        if (!graphics_queue_family_found) {
            fprintf(stderr, "error (vulkan): Failed to find a queue family that supports graphics computation.\n");
            return 1;
        }

        // Clean up.

        free(queue_family_properties);
    }

    // Create a logical device.

    VkDevice device = VK_NULL_HANDLE;

    {
        // Configure the queues.

        float graphics_queue_priorities[] = { 1.0f };

        const VkDeviceQueueCreateInfo graphics_queue_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .queueFamilyIndex = graphics_queue_family_index,
            .queueCount = 1,
            .pQueuePriorities = graphics_queue_priorities,
        };

        // Select physical device features.

        const VkPhysicalDeviceFeatures physical_device_features = { 0 };

        //  Select layers and extensions.

        // TODO Check device extension/layer support.

        const char* const device_extension_names[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        const uint32_t device_extension_count = sizeof device_extension_names / sizeof * device_extension_names;

        const char* const* enabled_layer_names = validation_layer_names;
        const uint32_t enabled_layer_count = ENABLE_VALIDATION ? validation_layer_count : 0;

        // Configure the device.

        const VkDeviceCreateInfo device_create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &graphics_queue_create_info,
            .enabledLayerCount = enabled_layer_count,
            .ppEnabledLayerNames = enabled_layer_names,
            .enabledExtensionCount = device_extension_count,
            .ppEnabledExtensionNames = device_extension_names,
            .pEnabledFeatures = &physical_device_features,
        };

        const VkResult result = vkCreateDevice(physical_device, &device_create_info, NULL, &device);

        if (result != VK_SUCCESS) {
            fprintf(stderr, "error (vulkan): Failed to create a device.\n");
            return 1;
        }
    }

    // Get queues from the device.

    VkQueue graphics_queue = VK_NULL_HANDLE;

    {
        vkGetDeviceQueue(device, graphics_queue_family_index, 0, &graphics_queue);
    }

    // Create a swapchain.

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkSurfaceFormatKHR surface_format;
    VkExtent2D image_extent;

    {
        // Get the capabilities of the surface.

        VkSurfaceCapabilitiesKHR surface_capabilities;

        {
            const VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);

            if (result != VK_SUCCESS) {
                fprintf(stderr, "error (vulkan): Failed to fetch the surface capabilities.\n");
                return 1;
            }
        }

        // Find the best surface format.

        {
            // Get all available surface formats.

            uint32_t surface_format_count = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, NULL);

            if (surface_format_count < 1) {
                fprintf(stderr, "error (vulkan): No surface formats are available.\n");
                return 1;
            }

            VkSurfaceFormatKHR *surface_formats = malloc(surface_format_count * sizeof *surface_formats);
            const VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, surface_formats);

            if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || surface_formats == NULL) {
                fprintf(stderr, "error (vulkan): Failed to fetch surface formats.\n");
                free(surface_formats);
                return 1;
            }

            // Find a surface format.

            surface_format = surface_formats[0];

            for (uint32_t i = 0; i < surface_format_count; i++) {

                // Find an SRGB surface.

                if (surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
                    switch (surface_formats[i].format) {
                        case VK_FORMAT_B8G8R8A8_SRGB:
                        case VK_FORMAT_R8G8B8A8_SRGB:
                        {
                            surface_format = surface_formats[i];
                            break;
                        }
                    };
                }
            }

            // Clean up.

            free(surface_formats);
        }

        // Find the best presentation mode.

        VkPresentModeKHR present_mode;

        {
            // Get all available present modes.

            uint32_t present_mode_count = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL);

            if (present_mode_count < 1) {
                fprintf(stderr, "error (vulkan): No presentation modes are available.\n");
                return 1;
            }

            VkPresentModeKHR *present_modes = malloc(present_mode_count * sizeof *present_modes);
            const VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes);

            if ((result != VK_SUCCESS && result != VK_INCOMPLETE) || present_modes == NULL) {
                fprintf(stderr, "error (vulkan): Failed to fetch presentation modes.\n");
                free(present_modes);
                return 1;
            }

            // Find a present mode.

            present_mode = VK_PRESENT_MODE_FIFO_KHR; // FIXME Check if this is guaranteed to work.

            /*
            for (uint32_t i = 0; i < present_mode_count; i++) {
                if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                    present_mode = present_modes[i];
                    break;
                }
            }
            */

            // Clean up.

            free(present_modes);
        }

        // Find a suitable image extent.

        {
            image_extent = surface_capabilities.currentExtent;;

            // Check if it is allowed to differ the swapchain resolution from the window resolution.

            if (image_extent.width == UINT32_MAX) {
                int32_t framebuffer_width = 0, framebuffer_height = 0;
                glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

                image_extent.width = framebuffer_width;
                image_extent.height = framebuffer_height;
            }

            // Clamp the extent between the minimum and maximum extent.

            if (image_extent.width < surface_capabilities.minImageExtent.width) {
                image_extent.width = surface_capabilities.minImageExtent.width;
            } else if (image_extent.width > surface_capabilities.maxImageExtent.width) {
                image_extent.width = surface_capabilities.maxImageExtent.width;
            }

            if (image_extent.height < surface_capabilities.minImageExtent.height) {
                image_extent.height = surface_capabilities.minImageExtent.height;
            } else if (image_extent.height > surface_capabilities.maxImageExtent.height) {
                image_extent.height = surface_capabilities.maxImageExtent.height;
            }
        }

        // Select an image count.

        uint32_t image_count = surface_capabilities.minImageCount + 1;

        {
            // Clamp the image count between the minium and maximum image count.

            if (image_count < surface_capabilities.minImageCount) {
                image_count = surface_capabilities.minImageCount;
            } else if (image_count > surface_capabilities.maxImageCount) {
                image_count = surface_capabilities.maxImageCount;
            }
        }

        // Configure the swapchain.

        const VkSwapchainCreateInfoKHR swapchain_create_info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .surface = surface,
            .minImageCount = image_count,
            .imageFormat = surface_format.format,
            .imageColorSpace = surface_format.colorSpace,
            .imageExtent = image_extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .preTransform = surface_capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = present_mode,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE,
        };

        // Create the swapchain.

        const VkResult result = vkCreateSwapchainKHR(device, &swapchain_create_info, NULL, &swapchain);

        if (result != VK_SUCCESS) {
            fprintf(stderr, "error (vulkan): Failed to create a swapchain.\n");
            return 1;
        }
    }

    // Create the image views.

    uint32_t image_view_count = 0;
    VkImageView *image_views = NULL;

    {
        // Get the swapchain images.

        uint32_t image_count = 0;
        VkImage *images = NULL;

        {
            vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL);

            if (image_count < 1) {
                fprintf(stderr, "error (vulkan): No swapchain images are available.\n");
                return 1;
            }

            images = malloc(image_count * sizeof *images);
            const VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &image_count, images);

            if (result != VK_SUCCESS || images == NULL) {
                fprintf(stderr, "error (vulkan): Failed to fetch the swapchain images.\n");
                free(images);
                return 1;
            }
        }

        // Configure and create the image views.

        image_view_count = image_count;
        image_views = malloc(image_count * sizeof *image_views);

        for (uint32_t i = 0; i < image_count; i++) {
            const VkImageViewCreateInfo image_view_create_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .image = images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = surface_format.format,
                .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };

            const VkResult result = vkCreateImageView(device, &image_view_create_info, NULL, &image_views[i]);

            if (result != VK_SUCCESS) {
                fprintf(stderr, "error (vulkan): Failed to create an image view.\n");
                free(image_views);
                free(images);
                return 1;
            }
        }

        // Clean up.

        free(images);
    }

    // Create the render pass.

    VkRenderPass graphics_render_pass = VK_NULL_HANDLE;

    {
        // Configure the color attachment (for the framebuffers).

        const VkAttachmentDescription color_attachment_description = {
            .flags = 0,
            .format = surface_format.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        };

        const VkAttachmentReference color_attachment_reference = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        const VkSubpassDescription subpass_description = {
            .flags = 0,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .pInputAttachments = NULL,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment_reference,
            .pResolveAttachments = NULL,
            .pDepthStencilAttachment = NULL,
            .preserveAttachmentCount = VK_FALSE,
            .pPreserveAttachments = NULL,
        };

        const VkSubpassDependency subpass_dependency = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0,
        };

        const VkRenderPassCreateInfo render_pass_create_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .attachmentCount = 1,
            .pAttachments = &color_attachment_description,
            .subpassCount = 1,
            .pSubpasses = &subpass_description,
            .dependencyCount = 1,
            .pDependencies = &subpass_dependency,
        };

        // Create the render pass.

        const VkResult result = vkCreateRenderPass(device, &render_pass_create_info, NULL, &graphics_render_pass);

        if (result != VK_SUCCESS) {
            fprintf(stderr, "error (vulkan): Failed to create the render pass.\n");
            return 1;
        }
    }

    // Create the shader modules.

    VkShaderModule vertex_shader_module = VK_NULL_HANDLE;
    VkShaderModule fragment_shader_module = VK_NULL_HANDLE;

    {
        // Create the vertex shader module.

        {
            // Open the vertex shader module file and read the bytes.

            FILE *vertex_shader_module_file = fopen("vertex.spv", "rb");

            if (vertex_shader_module_file == NULL) {
                fprintf(stderr, "error (io): Failed to open vertex shader file.\n");
                return 1;
            }

            fseek(vertex_shader_module_file, 0L, SEEK_END);
            const uint64_t vertex_shader_module_file_size = ftell(vertex_shader_module_file);
            fseek(vertex_shader_module_file, 0L, SEEK_SET);
            char *vertex_shader_module_file_buffer = malloc(vertex_shader_module_file_size * (sizeof *vertex_shader_module_file_buffer));
            fread(vertex_shader_module_file_buffer, vertex_shader_module_file_size, 1, vertex_shader_module_file);

            // Create the vertex shader module.

            const VkShaderModuleCreateInfo vertex_shader_module_create_info = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .codeSize = vertex_shader_module_file_size,
                .pCode = (uint32_t *)vertex_shader_module_file_buffer,
            };

            const VkResult result = vkCreateShaderModule(device, &vertex_shader_module_create_info, NULL, &vertex_shader_module);

            if (result != VK_SUCCESS) {
                fprintf(stderr, "error (vulkan): Failed to create vertex shader module.\n");
                fclose(vertex_shader_module_file);
                free(vertex_shader_module_file_buffer);
                return 1;
            }

            // Clean up.

            fclose(vertex_shader_module_file);
            free(vertex_shader_module_file_buffer);
        }

        // Create the fragment shader module

        {
            // Open the fragment shader module file and read the bytes.

            FILE *fragment_shader_module_file = fopen("fragment.spv", "rb");

            if (fragment_shader_module_file == NULL) {
                fprintf(stderr, "error (io): Failed to open fragment shader file.\n");
                return 1;
            }

            fseek(fragment_shader_module_file, 0L, SEEK_END);
            const uint64_t fragment_shader_module_file_size = ftell(fragment_shader_module_file);
            fseek(fragment_shader_module_file, 0L, SEEK_SET);
            char *fragment_shader_module_file_buffer = malloc(fragment_shader_module_file_size * (sizeof *fragment_shader_module_file_buffer));
            fread(fragment_shader_module_file_buffer, fragment_shader_module_file_size, 1, fragment_shader_module_file);

            // Create the fragment shader module.

            const VkShaderModuleCreateInfo fragment_shader_module_create_info = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .codeSize = fragment_shader_module_file_size,
                .pCode = (uint32_t *)fragment_shader_module_file_buffer,
            };

            const VkResult result = vkCreateShaderModule(device, &fragment_shader_module_create_info, NULL, &fragment_shader_module);

            if (result != VK_SUCCESS) {
                fprintf(stderr, "error (vulkan): Failed to create fragment shader module.\n");
                fclose(fragment_shader_module_file);
                free(fragment_shader_module_file_buffer);
                return 1;
            }

            // Clean up.

            fclose(fragment_shader_module_file);
            free(fragment_shader_module_file_buffer);
        }
    }

    // Create the graphics pipeline.

    VkPipelineLayout graphics_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;

    {
        // Create the pipeline layout.

        {
            const VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .setLayoutCount = 0,
                .pSetLayouts = NULL,
                .pushConstantRangeCount = 0,
                .pPushConstantRanges = NULL,
            };

            const VkResult result = vkCreatePipelineLayout(device, &pipeline_layout_create_info, NULL, &graphics_pipeline_layout);

            if (result != VK_SUCCESS) {
                fprintf(stderr, "error (vulkan): Failed to crete a graphics pipeline layout.");
                return 1;
            }
        }

        // Configure the fixed function stages.

        const VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = NULL,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = NULL,
        };

        const VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        const VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)image_extent.width,
            .height = (float)image_extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        const VkRect2D scissor = {
            .offset = {
                .x = 0,
                .y = 0,
            },
            .extent = image_extent,
        };

        const VkPipelineViewportStateCreateInfo viewport_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor,
        };

        const VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f,
        };

        const VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0f,
            .pSampleMask = NULL,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        };

        const VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };

        const VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &color_blend_attachment_state,
            .blendConstants[0] = 0.0f,
            .blendConstants[1] = 0.0f,
            .blendConstants[2] = 0.0f,
            .blendConstants[3] = 0.0f,
        };

        // Configure the shader stages.

        const VkPipelineShaderStageCreateInfo vertex_shader_stage_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader_module,
            .pName = "main",
            .pSpecializationInfo = NULL,
        };

        const VkPipelineShaderStageCreateInfo fragment_shader_stage_create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_shader_module,
            .pName = "main",
            .pSpecializationInfo = NULL,
        };

        VkPipelineShaderStageCreateInfo shader_stage_create_infos[] = {vertex_shader_stage_create_info, fragment_shader_stage_create_info};

        // Create the graphics pipeline.

        const VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stageCount = 2,
            .pStages = shader_stage_create_infos,
            .pVertexInputState = &vertex_input_state_create_info,
            .pInputAssemblyState = &input_assembly_state_create_info,
            .pTessellationState = NULL,
            .pViewportState = &viewport_state_create_info,
            .pRasterizationState = &rasterization_state_create_info,
            .pMultisampleState = &multisample_state_create_info,
            .pDepthStencilState = NULL,
            .pColorBlendState = &color_blend_state_create_info,
            .pDynamicState = NULL,
            .layout = graphics_pipeline_layout,
            .renderPass = graphics_render_pass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1,
        };

        const VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, NULL, &graphics_pipeline);

        if (result != VK_SUCCESS) {
            fprintf(stderr, "error (vulkan): Failed to create the graphics pipeline.\n");
            return 1;
        }

        // Clean up.

        vkDestroyShaderModule(device, fragment_shader_module, NULL);
        vkDestroyShaderModule(device, vertex_shader_module, NULL);
    }

    // Create the framebuffers.

    VkFramebuffer *framebuffers = NULL;

    {
        framebuffers = malloc(image_view_count * sizeof *framebuffers);

        for (uint32_t i = 0; i < image_view_count; i++) {
            VkImageView attachments[] = {
                image_views[i]
            };

            const VkFramebufferCreateInfo framebuffer_create_info = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .renderPass = graphics_render_pass,
                .attachmentCount = 1,
                .pAttachments = attachments,
                .width = image_extent.width,
                .height = image_extent.height,
                .layers = 1,
            };

            const VkResult result = vkCreateFramebuffer(device, &framebuffer_create_info, NULL, &framebuffers[i]);

            if (result != VK_SUCCESS) {
                fprintf(stderr, "error (vulkan): Failed to create a framebuffer.");
                free(framebuffers);
                return 1;
            }
        }
    }

    // Create a command pool.

    VkCommandPool command_pool = VK_NULL_HANDLE;

    {
        const VkCommandPoolCreateInfo command_pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .queueFamilyIndex = graphics_queue_family_index,
        };

        const VkResult result = vkCreateCommandPool(device, &command_pool_create_info, NULL,  &command_pool);

        if (result != VK_SUCCESS) {
            fprintf(stderr, "error (vulkan): Failed to create the command pool.\n");
            return 1;
        }
    }

    // Allocate and record the command buffers.

    VkCommandBuffer *command_buffers = NULL;

    {
        // Allocate the command buffers

        {
            command_buffers = malloc(image_view_count * sizeof *command_buffers);

            const VkCommandBufferAllocateInfo command_buffer_allocate_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = NULL,
                .commandPool = command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = image_view_count,
            };

            const VkResult result = vkAllocateCommandBuffers(device, &command_buffer_allocate_info, command_buffers);

            if (result != VK_SUCCESS || command_buffers == NULL) {
                fprintf(stderr, "error (vulkan): Failed to allocate the command buffers.\n");
                free(command_buffers);
                return 1;
            }
        }

        // Record the command buffers for drawing.

        {
            const VkCommandBufferBeginInfo command_buffer_begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = NULL,
                .flags = 0,
                .pInheritanceInfo = VK_NULL_HANDLE,
            };

            for (uint32_t i = 0; i < image_view_count; i++) {

                // Set up.

                {
                    const VkResult result = vkBeginCommandBuffer(command_buffers[i], &command_buffer_begin_info);

                    if (result != VK_SUCCESS) {
                        fprintf(stderr, "error (vulkan): Failed to start command buffer recording.\n");
                        return 1;
                    }

                    const VkClearValue clear_value = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

                    const VkRenderPassBeginInfo render_pass_begin_info = {
                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                        .pNext = NULL,
                        .renderPass = graphics_render_pass,
                        .framebuffer = framebuffers[i],
                        .renderArea = {
                            .offset = {
                                .x = 0,
                                .y = 0,
                            },
                            .extent = image_extent,
                        },
                        .clearValueCount = 1,
                        .pClearValues = &clear_value,
                    };

                    vkCmdBeginRenderPass(command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
                }

                // Record draw commands.

                {
                    vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
                    vkCmdDraw(command_buffers[i], 3, 1, 0, 0);
                }

                // Finish recording.

                {
                    vkCmdEndRenderPass(command_buffers[i]);

                    const VkResult result = vkEndCommandBuffer(command_buffers[i]);

                    if (result != VK_SUCCESS) {
                        fprintf(stderr, "error (vulkan): Failed to finish command buffer recording.\n");
                        return 1;
                    }
                }
            }
        }
    }

    // Create semaphores and fences for synchronization.

    VkSemaphore *image_available_semaphores = NULL;
    VkSemaphore *image_finished_semaphores = NULL;
    VkFence *in_flight_fences = NULL;
    VkFence *in_flight_image_fences = NULL;

    {
        image_available_semaphores = malloc(MAX_FRAMES_IN_FLIGHT * sizeof *image_available_semaphores);
        image_finished_semaphores = malloc(MAX_FRAMES_IN_FLIGHT * sizeof *image_finished_semaphores);
        in_flight_fences = malloc(MAX_FRAMES_IN_FLIGHT * sizeof *in_flight_fences);
        in_flight_image_fences = malloc(image_view_count * sizeof *in_flight_image_fences);

        const VkSemaphoreCreateInfo semaphore_create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        };

        const VkFenceCreateInfo fence_create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            const VkResult result = vkCreateSemaphore(device, &semaphore_create_info, NULL, &image_available_semaphores[i])
                & vkCreateSemaphore(device, &semaphore_create_info, NULL, &image_finished_semaphores[i])
                & vkCreateFence(device, &fence_create_info, NULL, &in_flight_fences[i]);

            if (result != VK_SUCCESS) {
                fprintf(stderr, "error (vulkan): Failed to create synchronisation objects.\n");
                return 1;
            }
        }

        for (uint32_t i = 0; i < image_view_count; i++) {
            in_flight_image_fences[i] = VK_NULL_HANDLE;
        }
    }

    // Draw and poll events.

    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        uint32_t current_frame = 0;

        {
            vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

            uint32_t image_index = 0;
            VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);

            if (result != VK_SUCCESS) {
                fprintf(stderr, "error (vulkan): Failed acquire the next image.\n");
                return 1;
            }

            if (in_flight_image_fences[image_index] != VK_NULL_HANDLE) {
                vkWaitForFences(device, 1, &in_flight_image_fences[image_index], VK_TRUE, UINT64_MAX);
            }

            in_flight_image_fences[image_index] = in_flight_fences[current_frame];

            const VkSemaphore wait_semaphores[] = {image_available_semaphores[current_frame]};
            const VkSemaphore signal_semaphores[] = {image_finished_semaphores[current_frame]};
            const VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

            const VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = NULL,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = wait_semaphores,
                .pWaitDstStageMask = wait_stages,
                .commandBufferCount = 1,
                .pCommandBuffers = &command_buffers[image_index],
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = signal_semaphores,
            };

            vkResetFences(device, 1, &in_flight_fences[current_frame]);

            result = vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences[current_frame]);

            if (result != VK_SUCCESS) {
                fprintf(stderr, "error (vulkan): Failed to submit command buffers to the graphics queue.\n");
                return 1;
            }

            const VkPresentInfoKHR present_info = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .pNext = NULL,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = signal_semaphores,
                .swapchainCount = 1,
                .pSwapchains = &swapchain,
                .pImageIndices = &image_index,
                .pResults = NULL,
            };

            vkQueuePresentKHR(graphics_queue, &present_info);
            current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
        }
    }

    vkDeviceWaitIdle(device);

    // Clean up.

    {
        {
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroySemaphore(device, image_finished_semaphores[i], NULL);
                vkDestroySemaphore(device, image_available_semaphores[i], NULL);
                vkDestroyFence(device, in_flight_fences[i], NULL);
            }

            free(image_finished_semaphores);
            free(image_available_semaphores);
            free(in_flight_fences);
            free(in_flight_image_fences);
        }

        vkDestroyCommandPool(device, command_pool, NULL);
        free(command_buffers);

        {
            for (uint32_t i = 0; i < image_view_count; i++) {
                vkDestroyFramebuffer(device, framebuffers[i], NULL);
            }

            free(framebuffers);
        }

        vkDestroyPipeline(device, graphics_pipeline, NULL);
        vkDestroyPipelineLayout(device, graphics_pipeline_layout, NULL);
        vkDestroyRenderPass(device, graphics_render_pass, NULL);

        {
            for (uint32_t i = 0; i < image_view_count; i++) {
                vkDestroyImageView(device, image_views[i], NULL);
            }

            free(image_views);
        }

        vkDestroySwapchainKHR(device, swapchain, NULL);

        vkDestroyDevice(device, NULL);
        vkDestroySurfaceKHR(instance, surface, NULL);
        vkDestroyInstance(instance, NULL);

        glfwDestroyWindow(window);
        glfwTerminate();
    }
}
