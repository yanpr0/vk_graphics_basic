#ifndef SIMPLE_SHADOWMAP_RENDER_H
#define SIMPLE_SHADOWMAP_RENDER_H

#define VK_NO_PROTOTYPES
#include "../../render/scene_mgr.h"
#include "../../render/render_common.h"
#include "../../../resources/shaders/common.h"
#include <geom/vk_mesh.h>
#include <vk_descriptor_sets.h>
#include <vk_fbuf_attachment.h>
#include <vk_images.h>
#include <vk_swapchain.h>
#include <vk_quad.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <iostream>

class SimpleShadowmapRender : public IRender
{
public:
  SimpleShadowmapRender(uint32_t a_width, uint32_t a_height);
  ~SimpleShadowmapRender()  { Cleanup(); };

  inline uint32_t     GetWidth()      const override { return m_width; }
  inline uint32_t     GetHeight()     const override { return m_height; }
  inline VkInstance   GetVkInstance() const override { return m_instance; }
  void InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId) override;

  void InitPresentation(VkSurfaceKHR &a_surface, bool initGUI) override;

  void ProcessInput(const AppInput& input) override;
  void UpdateCamera(const Camera* cams, uint32_t a_camsNumber) override;
  Camera GetCurrentCamera() override {return m_cam;}
  void UpdateView();

  void LoadScene(const char *path, bool transpose_inst_matrices) override;
  void DrawFrame(float a_time, DrawMode a_mode) override;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // debugging utils
  //
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackFn(
    VkDebugReportFlagsEXT                       flags,
    VkDebugReportObjectTypeEXT                  objectType,
    uint64_t                                    object,
    size_t                                      location,
    int32_t                                     messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData)
  {
    (void)flags;
    (void)objectType;
    (void)object;
    (void)location;
    (void)messageCode;
    (void)pUserData;
    std::cout << pLayerPrefix << ": " << pMessage << std::endl;
    return VK_FALSE;
  }

  VkDebugReportCallbackEXT m_debugReportCallback = nullptr;
private:
  static constexpr std::size_t m_instCount = 5000;//0;

  VkInstance       m_instance           = VK_NULL_HANDLE;
  VkCommandPool    m_commandPool        = VK_NULL_HANDLE;
  VkCommandPool    m_computeCommandPool = VK_NULL_HANDLE;
  VkPhysicalDevice m_physicalDevice     = VK_NULL_HANDLE;
  VkDevice         m_device             = VK_NULL_HANDLE;
  VkQueue          m_graphicsQueue      = VK_NULL_HANDLE;
  //VkQueue          m_transferQueue      = VK_NULL_HANDLE;
  //VkQueue          m_computeQueue       = VK_NULL_HANDLE;

  vk_utils::QueueFID_T m_queueFamilyIDXs {UINT32_MAX, UINT32_MAX, UINT32_MAX};

  struct
  {
    uint32_t    currentFrame      = 0u;
    VkQueue     queue             = VK_NULL_HANDLE;
    VkSemaphore imageAvailable    = VK_NULL_HANDLE;
    VkSemaphore renderingFinished = VK_NULL_HANDLE;
  } m_presentationResources;

  std::vector<VkFence> m_frameFences;
  std::vector<VkCommandBuffer> m_cmdBuffersDrawMain;

  struct
  {
    float4x4 projView;
    float4x4 model;
  } pushConst2M;

  struct
  {
    float4x4 projView;
    LiteMath::Box4f bbox;
    std::uint32_t instCnt = m_instCount; // could be put to bbox.*.w
  } m_pushConstCompute;

  float4x4 m_worldViewProj;
  float4x4 m_lightMatrix;

  UniformParams m_uniforms {};
  VkBuffer m_ubo = VK_NULL_HANDLE;
  VkDeviceMemory m_uboAlloc = VK_NULL_HANDLE;
  void* m_uboMappedMem = nullptr;

  VkBuffer m_instTransform = VK_NULL_HANDLE;
  VkDeviceMemory m_instTransformAlloc = VK_NULL_HANDLE;
  void* m_instTransformMappedMem = nullptr;

  VkBuffer m_visInstIds = VK_NULL_HANDLE;
  VkDeviceMemory m_visInstIdsAlloc = VK_NULL_HANDLE;
  void* m_visInstIdsMappedMem = nullptr;

  VkBuffer m_indirectInfo = VK_NULL_HANDLE;
  VkDeviceMemory m_indirectInfoAlloc = VK_NULL_HANDLE;
  void* m_indirectInfoMappedMem = nullptr;

  pipeline_data_t m_computePipeline {};
  pipeline_data_t m_basicForwardPipeline {};
  pipeline_data_t m_shadowPipeline {};

  VkDescriptorSet m_computeDS = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_computeDSLayout = VK_NULL_HANDLE;

  VkDescriptorSet m_dSet = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_dSetLayout = VK_NULL_HANDLE;

  VkRenderPass m_screenRenderPass = VK_NULL_HANDLE; // main renderpass

  std::shared_ptr<vk_utils::DescriptorMaker> m_pBindings = nullptr;

  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
  VulkanSwapChain m_swapchain;
  std::vector<VkFramebuffer> m_frameBuffers;
  vk_utils::VulkanImageMem m_depthBuffer{}; // screen depthbuffer

  Camera   m_cam;
  uint32_t m_width  = 1024u;
  uint32_t m_height = 1024u;
  uint32_t m_framesInFlight = 2u;
  bool m_vsync = false;

  VkPhysicalDeviceFeatures m_enabledDeviceFeatures = {};
  std::vector<const char*> m_deviceExtensions      = {};
  std::vector<const char*> m_instanceExtensions    = {};

  bool m_enableValidation;
  std::vector<const char*> m_validationLayers;

  std::shared_ptr<SceneManager>     m_pScnMgr;

  // objects and data for shadow map
  //
  std::shared_ptr<vk_utils::IQuad>               m_pFSQuad;
  //std::shared_ptr<vk_utils::RenderableTexture2D> m_pShadowMap;
  std::shared_ptr<vk_utils::RenderTarget>        m_pShadowMap2;
  uint32_t                                       m_shadowMapId = 0;

  VkDeviceMemory        m_memShadowMap = VK_NULL_HANDLE;
  VkDescriptorSet       m_quadDS;
  VkDescriptorSetLayout m_quadDSLayout = nullptr;

  struct InputControlMouseEtc
  {
    bool drawFSQuad = false;
  } m_input;

  /**
  \brief basic parameters that you usually need for shadow mapping
  */
  struct ShadowMapCam
  {
    ShadowMapCam()
    {
      cam.pos    = float3(4.0f, 4.0f, 4.0f);
      cam.lookAt = float3(0, 0, 0);
      cam.up     = float3(0, 1, 0);

      radius          = 5.0f;
      lightTargetDist = 20.0f;
      usePerspectiveM = true;
    }

    float  radius;           ///!< ignored when usePerspectiveM == true
    float  lightTargetDist;  ///!< identify depth range
    Camera cam;              ///!< user control for light to later get light worldViewProj matrix
    bool   usePerspectiveM;  ///!< use perspective matrix if true and ortographics otherwise

  } m_light;

  void DrawFrameSimple();

  void CreateInstance();
  void CreateDevice(uint32_t a_deviceId);

  void BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkFramebuffer a_frameBuff,
                                VkImageView a_targetImageView, VkPipeline a_pipeline);

  void DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp);

  void SetupSimplePipeline();
  void CleanupPipelineAndSwapchain();
  void RecreateSwapChain();

  void CreateBuffers();
  void UpdateUniformBuffer(float a_time);

  void Cleanup();

  void SetupDeviceFeatures();
  void SetupDeviceExtensions();
  void SetupValidationLayers();
};


#endif //CHIMERA_SIMPLE_RENDER_H
