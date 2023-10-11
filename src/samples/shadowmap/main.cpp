#include "shadowmap_render.h"
#include "utils/glfw_window.h"

void initVulkanGLFW(std::shared_ptr<IRender> &app, GLFWwindow* window, int deviceID, bool showGUI)
{
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions;
  glfwExtensions  = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  if(glfwExtensions == nullptr)
  {
    std::cout << "WARNING. Can't connect Vulkan to GLFW window (glfwGetRequiredInstanceExtensions returns NULL)" << std::endl;
  }

  app->InitVulkan(glfwExtensions, glfwExtensionCount, deviceID);

  if(glfwExtensions != nullptr)
  {
    VkSurfaceKHR surface;
    VK_CHECK_RESULT(glfwCreateWindowSurface(app->GetVkInstance(), window, nullptr, &surface));

    if(showGUI)
    {
      setupImGuiContext(window);
    }
    app->InitPresentation(surface, showGUI);
  }
}

int main()
{
  constexpr bool WITH_GUI = true;

  constexpr int WIDTH = 1024;
  constexpr int HEIGHT = 1024;
  constexpr int VULKAN_DEVICE_ID = 0;

  std::system("cd ../resources/shaders && python3 compile_shadowmap_shaders.py");

  std::shared_ptr<IRender> app = std::make_unique<SimpleShadowmapRender>(WIDTH, HEIGHT);
  if(app == nullptr)
  {
    std::cout << "Can't create render of specified type" << std::endl;
    return 1;
  }

  auto* window = initWindow(WIDTH, HEIGHT);

  initVulkanGLFW(app, window, VULKAN_DEVICE_ID, WITH_GUI);

  app->LoadScene("../resources/scenes/043_cornell_normals/statex_00001.xml", false);

  mainLoop(app, window, WITH_GUI);

  return 0;
}
