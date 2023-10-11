#include "shadowmap_render.h"
#include "../../utils/input_definitions.h"

#include <geom/vk_mesh.h>
#include <vk_pipeline.h>
#include <vk_buffers.h>

SimpleShadowmapRender::SimpleShadowmapRender(uint32_t a_width, uint32_t a_height) : m_width(a_width), m_height(a_height)
{
#ifdef NDEBUG
  m_enableValidation = false;
#else
  m_enableValidation = true;
#endif
}

void SimpleShadowmapRender::SetupDeviceFeatures()
{
  // m_enabledDeviceFeatures.fillModeNonSolid = VK_TRUE;
}

void SimpleShadowmapRender::SetupDeviceExtensions()
{
  m_deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

void SimpleShadowmapRender::SetupValidationLayers()
{
  m_validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  m_validationLayers.push_back("VK_LAYER_LUNARG_monitor");
}

void SimpleShadowmapRender::InitVulkan(const char** a_instanceExtensions, uint32_t a_instanceExtensionsCount, uint32_t a_deviceId)
{
  for(size_t i = 0; i < a_instanceExtensionsCount; ++i)
  {
    m_instanceExtensions.push_back(a_instanceExtensions[i]);
  }

  SetupValidationLayers();
  VK_CHECK_RESULT(volkInitialize());
  CreateInstance();
  volkLoadInstance(m_instance);

  CreateDevice(a_deviceId);
  volkLoadDevice(m_device);

  m_commandPool = vk_utils::createCommandPool(m_device, m_queueFamilyIDXs.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  m_cmdBuffersDrawMain.reserve(m_framesInFlight);
  m_cmdBuffersDrawMain = vk_utils::createCommandBuffers(m_device, m_commandPool, m_framesInFlight);

  m_frameFences.resize(m_framesInFlight);
  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (size_t i = 0; i < m_framesInFlight; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(m_device, &fenceInfo, nullptr, &m_frameFences[i]));
  }

  m_pScnMgr = std::make_shared<SceneManager>(m_device, m_physicalDevice, m_queueFamilyIDXs.transfer, m_queueFamilyIDXs.graphics, false);
}

void SimpleShadowmapRender::InitPresentation(VkSurfaceKHR &a_surface, bool initGUI)
{
  m_surface = a_surface;

  m_presentationResources.queue = m_swapchain.CreateSwapChain(m_physicalDevice, m_device, m_surface,
                                                              m_width, m_height, m_framesInFlight, m_vsync);
  m_presentationResources.currentFrame = 0;

  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_presentationResources.imageAvailable));
  VK_CHECK_RESULT(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_presentationResources.renderingFinished));

  std::vector<VkFormat> depthFormats = {
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_D16_UNORM_S8_UINT,
    VK_FORMAT_D16_UNORM
  };
  vk_utils::getSupportedDepthFormat(m_physicalDevice, depthFormats, &m_depthBuffer.format);
  m_screenRenderPass = vk_utils::createDefaultRenderPass(m_device, m_swapchain.GetFormat(), m_depthBuffer.format);
  m_depthBuffer  = vk_utils::createDepthTexture(m_device, m_physicalDevice, m_width, m_height, m_depthBuffer.format);
  m_frameBuffers = vk_utils::createFrameBuffers(m_device, m_swapchain, m_screenRenderPass, m_depthBuffer.view);
  
  // create full screen quad for debug purposes
  // 
  m_pFSQuad = std::make_shared<vk_utils::QuadRenderer>(0,0, 512, 512);
  m_pFSQuad->Create(m_device, "../resources/shaders/quad3_vert.vert.spv", "../resources/shaders/quad.frag.spv", 
                    vk_utils::RenderTargetInfo2D{ VkExtent2D{ m_width, m_height }, m_swapchain.GetFormat(),                                        // this is debug full scree quad
                                                  VK_ATTACHMENT_LOAD_OP_LOAD, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR }); // seems we need LOAD_OP_LOAD if we want to draw quad to part of screen

  // create shadow map
  //
  m_pShadowMap2 = std::make_shared<vk_utils::RenderTarget>(m_device, VkExtent2D{2048, 2048});

  vk_utils::AttachmentInfo infoDepth;
  infoDepth.format           = VK_FORMAT_D16_UNORM;
  infoDepth.usage            = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  infoDepth.imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
  m_shadowMapId              = m_pShadowMap2->CreateAttachment(infoDepth);
  auto memReq                = m_pShadowMap2->GetMemoryRequirements()[0]; // we know that we have only one texture
  
  // memory for all shadowmaps (well, if you have them more than 1 ...)
  {
    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext           = nullptr;
    allocateInfo.allocationSize  = memReq.size;
    allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_physicalDevice);

    VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, NULL, &m_memShadowMap));
  }

  m_pShadowMap2->CreateViewAndBindMemory(m_memShadowMap, {0});
  m_pShadowMap2->CreateDefaultSampler();
  m_pShadowMap2->CreateDefaultRenderPass();

  if (initGUI)
  {
    m_pGUIRender = std::make_shared<ImGuiRender>(m_instance, m_device, m_physicalDevice, m_queueFamilyIDXs.graphics, m_graphicsQueue, m_swapchain);
  }
}

void SimpleShadowmapRender::CreateInstance()
{
  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pNext = nullptr;
  appInfo.pApplicationName = "VkRender";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.pEngineName = "ShadowMap";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
  appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 0);

  m_instance = vk_utils::createInstance(m_enableValidation, m_validationLayers, m_instanceExtensions, &appInfo);

  if (m_enableValidation)
    vk_utils::initDebugReportCallback(m_instance, &debugReportCallbackFn, &m_debugReportCallback);
}

void SimpleShadowmapRender::CreateDevice(uint32_t a_deviceId)
{
  SetupDeviceExtensions();
  m_physicalDevice = vk_utils::findPhysicalDevice(m_instance, true, a_deviceId, m_deviceExtensions);

  SetupDeviceFeatures();
  m_device = vk_utils::createLogicalDevice(m_physicalDevice, m_validationLayers, m_deviceExtensions,
                                           m_enabledDeviceFeatures, m_queueFamilyIDXs,
                                           VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT);

  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.graphics, 0, &m_graphicsQueue);
  vkGetDeviceQueue(m_device, m_queueFamilyIDXs.transfer, 0, &m_transferQueue);
}


void SimpleShadowmapRender::SetupSimplePipeline()
{
  std::vector<std::pair<VkDescriptorType, uint32_t> > dtypes = {
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             2},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     2}
  };

  m_pBindings = std::make_shared<vk_utils::DescriptorMaker>(m_device, dtypes, 3);

  auto shadowMap = m_pShadowMap2->m_attachments[m_shadowMapId];

  m_pBindings->BindBegin(VK_SHADER_STAGE_VERTEX_BIT);
  m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  m_pBindings->BindEnd(&m_dSet, &m_dSetLayout);

  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindBuffer(0, m_ubo, VK_NULL_HANDLE, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  m_pBindings->BindImage (1, shadowMap.view, m_pShadowMap2->m_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindEnd(&m_dSet, &m_dSetLayout);

  //m_pBindings->BindImage(0, m_GBufTarget->m_attachments[m_GBuf_idx[GBUF_ATTACHMENT::POS_Z]].view, m_GBufTarget->m_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  m_pBindings->BindBegin(VK_SHADER_STAGE_FRAGMENT_BIT);
  m_pBindings->BindImage(0, shadowMap.view, m_pShadowMap2->m_sampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  m_pBindings->BindEnd(&m_quadDS, &m_quadDSLayout);

  // if we are recreating pipeline (for example, to reload shaders)
  // we need to cleanup old pipeline
  if(m_basicForwardPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_basicForwardPipeline.layout, nullptr);
    m_basicForwardPipeline.layout = VK_NULL_HANDLE;
  }
  if(m_basicForwardPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_basicForwardPipeline.pipeline, nullptr);
    m_basicForwardPipeline.pipeline = VK_NULL_HANDLE;
  }

  if(m_shadowPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_shadowPipeline.pipeline, nullptr);
    m_shadowPipeline.pipeline = VK_NULL_HANDLE;
  }

  vk_utils::GraphicsPipelineMaker maker;
  
  // pipeline for drawing objects
  //
  std::unordered_map<VkShaderStageFlagBits, std::string> shader_paths;
  {
    shader_paths[VK_SHADER_STAGE_FRAGMENT_BIT] = "../resources/shaders/simple_shadow.frag.spv";
    shader_paths[VK_SHADER_STAGE_VERTEX_BIT]   = "../resources/shaders/simple.vert.spv";
  }
  maker.LoadShaders(m_device, shader_paths);

  m_basicForwardPipeline.layout = maker.MakeLayout(m_device, {m_dSetLayout}, sizeof(pushConst2M));
  maker.SetDefaultState(m_width, m_height);

  m_basicForwardPipeline.pipeline = maker.MakePipeline(m_device, m_pScnMgr->GetPipelineVertexInputStateCreateInfo(),
                                                       m_screenRenderPass);
                                                       //, {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR}
  
  // pipeline for rendering objects to shadowmap
  //
  // maker.SetDefaultState(m_width, m_height);
  shader_paths.clear();
  shader_paths[VK_SHADER_STAGE_VERTEX_BIT] = "../resources/shaders/simple.vert.spv";
  maker.LoadShaders(m_device, shader_paths);

  maker.viewport.width  = float(m_pShadowMap2->m_resolution.width);
  maker.viewport.height = float(m_pShadowMap2->m_resolution.height);
  maker.scissor.extent  = VkExtent2D{ uint32_t(m_pShadowMap2->m_resolution.width), uint32_t(m_pShadowMap2->m_resolution.height) };

  m_shadowPipeline.layout   = m_basicForwardPipeline.layout;
  m_shadowPipeline.pipeline = maker.MakePipeline(m_device, m_pScnMgr->GetPipelineVertexInputStateCreateInfo(), 
                                                 m_pShadowMap2->m_renderPass);                                                       
}

void SimpleShadowmapRender::CreateUniformBuffer()
{
  VkMemoryRequirements memReq;
  m_ubo = vk_utils::createBuffer(m_device, sizeof(UniformParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &memReq);

  VkMemoryAllocateInfo allocateInfo = {};
  allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocateInfo.pNext = nullptr;
  allocateInfo.allocationSize = memReq.size;
  allocateInfo.memoryTypeIndex = vk_utils::findMemoryType(memReq.memoryTypeBits,
                                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                          m_physicalDevice);
  VK_CHECK_RESULT(vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_uboAlloc));
  VK_CHECK_RESULT(vkBindBufferMemory(m_device, m_ubo, m_uboAlloc, 0));

  vkMapMemory(m_device, m_uboAlloc, 0, sizeof(m_uniforms), 0, &m_uboMappedMem);

  m_uniforms.invRes      = 1.0f / m_resolution;

  UpdateUniformBuffer(0.0f);

  pushConst2M.minHeight = 0.0;
  pushConst2M.maxHeight = 1.0;
}

void SimpleShadowmapRender::UpdateUniformBuffer(float a_time)
{
  m_uniforms.lightMatrix = m_lightMatrix;
  m_uniforms.lightPos    = m_light.cam.pos; //LiteMath::float3(sinf(a_time), 1.0f, cosf(a_time));
  m_uniforms.time        = a_time;

  m_uniforms.baseColor = LiteMath::float3(0.9f, 0.92f, 1.0f);
  memcpy(m_uboMappedMem, &m_uniforms, sizeof(m_uniforms));
}

void SimpleShadowmapRender::DrawSceneCmd(VkCommandBuffer a_cmdBuff, const float4x4& a_wvp)
{
  VkShaderStageFlags stageFlags = (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

  VkDeviceSize zero_offset = 0u;
  VkBuffer vertexBuf = m_pScnMgr->GetVertexBuffer();
  VkBuffer indexBuf  = m_pScnMgr->GetIndexBuffer();
  
  vkCmdBindVertexBuffers(a_cmdBuff, 0, 1, &vertexBuf, &zero_offset);
  vkCmdBindIndexBuffer(a_cmdBuff, indexBuf, 0, VK_INDEX_TYPE_UINT32);

  pushConst2M.projView = a_wvp;
  for (uint32_t i = 0; i < m_pScnMgr->InstancesNum(); ++i)
  {
    auto inst         = m_pScnMgr->GetInstanceInfo(i);

    vkCmdPushConstants(a_cmdBuff, m_basicForwardPipeline.layout, stageFlags, 0, sizeof(pushConst2M), &pushConst2M);

    auto mesh_info = m_pScnMgr->GetMeshInfo(inst.mesh_id);
    vkCmdDrawIndexed(a_cmdBuff, mesh_info.m_indNum, 1, mesh_info.m_indexOffset, mesh_info.m_vertexOffset, 0);
  }
}

void SimpleShadowmapRender::BuildCommandBufferSimple(VkCommandBuffer a_cmdBuff, VkFramebuffer a_frameBuff,
                                                     VkImageView a_targetImageView, VkPipeline a_pipeline)
{
  vkResetCommandBuffer(a_cmdBuff, 0);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  VK_CHECK_RESULT(vkBeginCommandBuffer(a_cmdBuff, &beginInfo));

  VkViewport viewport{};
  VkRect2D scissor{};
  VkExtent2D ext;
  ext.height = m_height;
  ext.width  = m_width;
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width  = static_cast<float>(ext.width);
  viewport.height = static_cast<float>(ext.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  scissor.offset = {0, 0};
  scissor.extent = ext;

  std::vector<VkViewport> viewports = {viewport};
  std::vector<VkRect2D> scissors = {scissor};
  vkCmdSetViewport(a_cmdBuff, 0, 1, viewports.data());
  vkCmdSetScissor(a_cmdBuff, 0, 1, scissors.data());

  //// draw scene to shadowmap
  //
  VkClearValue clearDepth = {};
  clearDepth.depthStencil.depth   = 1.0f;
  clearDepth.depthStencil.stencil = 0;
  std::vector<VkClearValue> clear =  {clearDepth};
  VkRenderPassBeginInfo renderToShadowMap = m_pShadowMap2->GetRenderPassBeginInfo(0, clear);
  vkCmdBeginRenderPass(a_cmdBuff, &renderToShadowMap, VK_SUBPASS_CONTENTS_INLINE);
  {
    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.pipeline);
    DrawSceneCmd(a_cmdBuff, m_lightMatrix);
  }
  vkCmdEndRenderPass(a_cmdBuff);

  //// draw final scene to screen
  //
  {
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_screenRenderPass;
    renderPassInfo.framebuffer = a_frameBuff;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchain.GetExtent();

    VkClearValue clearValues[2] = {};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues    = &clearValues[0];

    vkCmdBeginRenderPass(a_cmdBuff, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, a_pipeline);
    vkCmdBindDescriptorSets(a_cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_basicForwardPipeline.layout, 0, 1, &m_dSet, 0, VK_NULL_HANDLE);

    DrawSceneCmd(a_cmdBuff, m_worldViewProj);

    vkCmdEndRenderPass(a_cmdBuff);
  }

  if(m_input.drawFSQuad)
  {
    float scaleAndOffset[4] = {0.5f, 0.5f, -0.5f, +0.5f};
    m_pFSQuad->SetRenderTarget(a_targetImageView);
    m_pFSQuad->DrawCmd(a_cmdBuff, m_quadDS, scaleAndOffset);
  }

  VK_CHECK_RESULT(vkEndCommandBuffer(a_cmdBuff));
}


void SimpleShadowmapRender::CleanupPipelineAndSwapchain()
{
  if (!m_cmdBuffersDrawMain.empty())
  {
    vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_cmdBuffersDrawMain.size()),
                         m_cmdBuffersDrawMain.data());
    m_cmdBuffersDrawMain.clear();
  }

  for (size_t i = 0; i < m_frameFences.size(); i++)
  {
    vkDestroyFence(m_device, m_frameFences[i], nullptr);
  }

  vkDestroyImageView(m_device, m_depthBuffer.view, nullptr);
  vkDestroyImage(m_device, m_depthBuffer.image, nullptr);

  for (size_t i = 0; i < m_frameBuffers.size(); i++)
  {
    vkDestroyFramebuffer(m_device, m_frameBuffers[i], nullptr);
  }

  vkDestroyRenderPass(m_device, m_screenRenderPass, nullptr);

  //m_swapchain.Cleanup();
}

void SimpleShadowmapRender::RecreateSwapChain()
{
  vkDeviceWaitIdle(m_device);

  CleanupPipelineAndSwapchain();
  auto oldImgNum = m_swapchain.GetImageCount();
  m_presentationResources.queue = m_swapchain.CreateSwapChain(m_physicalDevice, m_device, m_surface, m_width, m_height,
         oldImgNum, m_vsync);
  std::vector<VkFormat> depthFormats = {
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM
  };                                                            
  vk_utils::getSupportedDepthFormat(m_physicalDevice, depthFormats, &m_depthBuffer.format);

  m_screenRenderPass = vk_utils::createDefaultRenderPass(m_device, m_swapchain.GetFormat(), m_depthBuffer.format);
  m_depthBuffer      = vk_utils::createDepthTexture(m_device, m_physicalDevice, m_width, m_height, m_depthBuffer.format);
  m_frameBuffers     = vk_utils::createFrameBuffers(m_device, m_swapchain, m_screenRenderPass, m_depthBuffer.view);

  m_frameFences.resize(m_framesInFlight);
  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  for (size_t i = 0; i < m_framesInFlight; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(m_device, &fenceInfo, nullptr, &m_frameFences[i]));
  }

  m_cmdBuffersDrawMain = vk_utils::createCommandBuffers(m_device, m_commandPool, m_framesInFlight);
  for (uint32_t i = 0; i < m_swapchain.GetImageCount(); ++i)
  {
    BuildCommandBufferSimple(m_cmdBuffersDrawMain[i], m_frameBuffers[i],
                             m_swapchain.GetAttachment(i).view, m_basicForwardPipeline.pipeline);
  }

  m_pGUIRender->OnSwapchainChanged(m_swapchain);
}

void SimpleShadowmapRender::Cleanup()
{
  if (m_pGUIRender)
  {
    m_pGUIRender = nullptr;
    ImGui::DestroyContext();
  }

  m_pShadowMap2 = nullptr;
  m_pFSQuad     = nullptr; // smartptr delete it's resources
  
  if(m_memShadowMap != VK_NULL_HANDLE)
  {
    vkFreeMemory(m_device, m_memShadowMap, VK_NULL_HANDLE);
    m_memShadowMap = VK_NULL_HANDLE;
  }

  CleanupPipelineAndSwapchain();

  if (m_basicForwardPipeline.pipeline != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_basicForwardPipeline.pipeline, nullptr);
  }
  if (m_basicForwardPipeline.layout != VK_NULL_HANDLE)
  {
    vkDestroyPipelineLayout(m_device, m_basicForwardPipeline.layout, nullptr);
  }

  if (m_presentationResources.imageAvailable != VK_NULL_HANDLE)
  {
    vkDestroySemaphore(m_device, m_presentationResources.imageAvailable, nullptr);
  }
  if (m_presentationResources.renderingFinished != VK_NULL_HANDLE)
  {
    vkDestroySemaphore(m_device, m_presentationResources.renderingFinished, nullptr);
  }

  if (m_commandPool != VK_NULL_HANDLE)
  {
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
  }
}

void SimpleShadowmapRender::ProcessInput(const AppInput &input)
{
  // add keyboard controls here
  // camera movement is processed separately
  //
  if(input.keyReleased[GLFW_KEY_Q])
    m_input.drawFSQuad = !m_input.drawFSQuad;

  if(input.keyReleased[GLFW_KEY_P])
    m_light.usePerspectiveM = !m_light.usePerspectiveM;

  // recreate pipeline to reload shaders
  if(input.keyPressed[GLFW_KEY_B])
  {
#ifdef WIN32
    std::system("cd ../resources/shaders && python compile_shadowmap_shaders.py");
#else
    std::system("cd ../resources/shaders && python3 compile_shadowmap_shaders.py");
#endif

    SetupSimplePipeline();

    for (uint32_t i = 0; i < m_framesInFlight; ++i)
    {
      BuildCommandBufferSimple(m_cmdBuffersDrawMain[i], m_frameBuffers[i],
                               m_swapchain.GetAttachment(i).view, m_basicForwardPipeline.pipeline);
    }
  }
}

void SimpleShadowmapRender::UpdateCamera(const Camera* cams, uint32_t a_camsNumber)
{
  m_cam = cams[0];
  if(a_camsNumber >= 2)
    m_light.cam = cams[1];
  UpdateView(); 
}

void SimpleShadowmapRender::UpdateView()
{
  ///// calc camera matrix
  //
  const float aspect = float(m_width) / float(m_height);
  auto mProjFix = OpenglToVulkanProjectionMatrixFix();
  auto mProj = projectionMatrix(m_cam.fov, aspect, 0.1f, 1000.0f);
  auto mLookAt = LiteMath::lookAt(m_cam.pos, m_cam.lookAt, m_cam.up);
  auto mWorldViewProj = mProjFix * mProj * mLookAt;
  
  m_worldViewProj = mWorldViewProj;
  
  ///// calc light matrix
  //
  if(m_light.usePerspectiveM)
    mProj = perspectiveMatrix(m_light.cam.fov, 1.0f, 1.0f, m_light.lightTargetDist*2.0f);
  else
    mProj = ortoMatrix(-m_light.radius, +m_light.radius, -m_light.radius, +m_light.radius, 0.0f, m_light.lightTargetDist);

  if(m_light.usePerspectiveM)  // don't understang why fix is not needed for perspective case for shadowmap ... it works for common rendering  
    mProjFix = LiteMath::float4x4();
  else
    mProjFix = OpenglToVulkanProjectionMatrixFix(); 
  
  mLookAt       = LiteMath::lookAt(m_light.cam.pos, m_light.cam.pos + m_light.cam.forward()*10.0f, m_light.cam.up);
  m_lightMatrix = mProjFix*mProj*mLookAt;
}

void SimpleShadowmapRender::LoadScene(const char* path, bool transpose_inst_matrices)
{
  m_pScnMgr->MakeTesselatedMeshScene(m_resolution);
  //m_pScnMgr->LoadSceneXML(path, transpose_inst_matrices);

  CreateUniformBuffer();
  SetupSimplePipeline();

  auto loadedCam = m_pScnMgr->GetCamera(0);
  m_cam.fov = loadedCam.fov;
  m_cam.pos = float3(loadedCam.pos);
  m_cam.up  = float3(loadedCam.up);
  m_cam.lookAt = float3(loadedCam.lookAt);
  m_cam.tdist  = loadedCam.farPlane;
  UpdateView();

  for (uint32_t i = 0; i < m_framesInFlight; ++i)
  {
    BuildCommandBufferSimple(m_cmdBuffersDrawMain[i], m_frameBuffers[i],
                             m_swapchain.GetAttachment(i).view, m_basicForwardPipeline.pipeline);
  }
}

void SimpleShadowmapRender::DrawFrameSimple()
{
  vkWaitForFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame], VK_TRUE, UINT64_MAX);
  vkResetFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame]);

  uint32_t imageIdx;
  m_swapchain.AcquireNextImage(m_presentationResources.imageAvailable, &imageIdx);

  auto currentCmdBuf = m_cmdBuffersDrawMain[m_presentationResources.currentFrame];

  VkSemaphore waitSemaphores[] = {m_presentationResources.imageAvailable};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  BuildCommandBufferSimple(currentCmdBuf, m_frameBuffers[imageIdx], m_swapchain.GetAttachment(imageIdx).view,
                           m_basicForwardPipeline.pipeline);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &currentCmdBuf;

  VkSemaphore signalSemaphores[] = {m_presentationResources.renderingFinished};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_frameFences[m_presentationResources.currentFrame]));

  VkResult presentRes = m_swapchain.QueuePresent(m_presentationResources.queue, imageIdx,
                                                 m_presentationResources.renderingFinished);

  if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR)
  {
    RecreateSwapChain();
  }
  else if (presentRes != VK_SUCCESS)
  {
    RUN_TIME_ERROR("Failed to present swapchain image");
  }

  m_presentationResources.currentFrame = (m_presentationResources.currentFrame + 1) % m_framesInFlight;

  vkQueueWaitIdle(m_presentationResources.queue);
}

void SimpleShadowmapRender::DrawFrameWithGUI()
{
  vkWaitForFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame], VK_TRUE, UINT64_MAX);
  vkResetFences(m_device, 1, &m_frameFences[m_presentationResources.currentFrame]);

  uint32_t imageIdx;
  auto result = m_swapchain.AcquireNextImage(m_presentationResources.imageAvailable, &imageIdx);
  if (result == VK_ERROR_OUT_OF_DATE_KHR)
  {
    RecreateSwapChain();
    return;
  }
  else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
  {
    RUN_TIME_ERROR("Failed to acquire the next swapchain image!");
  }

  auto currentCmdBuf = m_cmdBuffersDrawMain[m_presentationResources.currentFrame];

  VkSemaphore waitSemaphores[] = {m_presentationResources.imageAvailable};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  BuildCommandBufferSimple(currentCmdBuf, m_frameBuffers[imageIdx], m_swapchain.GetAttachment(imageIdx).view,
    m_basicForwardPipeline.pipeline);

  ImDrawData* pDrawData = ImGui::GetDrawData();
  auto currentGUICmdBuf = m_pGUIRender->BuildGUIRenderCommand(imageIdx, pDrawData);

  std::vector<VkCommandBuffer> submitCmdBufs = {currentCmdBuf, currentGUICmdBuf};

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = (uint32_t)submitCmdBufs.size();
  submitInfo.pCommandBuffers = submitCmdBufs.data();

  VkSemaphore signalSemaphores[] = {m_presentationResources.renderingFinished};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_frameFences[m_presentationResources.currentFrame]));

  VkResult presentRes = m_swapchain.QueuePresent(m_presentationResources.queue, imageIdx,
    m_presentationResources.renderingFinished);

  if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR)
  {
    RecreateSwapChain();
  }
  else if (presentRes != VK_SUCCESS)
  {
    RUN_TIME_ERROR("Failed to present swapchain image");
  }

  m_presentationResources.currentFrame = (m_presentationResources.currentFrame + 1) % m_framesInFlight;

  vkQueueWaitIdle(m_presentationResources.queue);
}

void SimpleShadowmapRender::DrawFrame(float a_time, DrawMode a_mode)
{
  UpdateUniformBuffer(a_time);
  switch (a_mode)
  {
    case DrawMode::WITH_GUI:
      SetupGUIElements();
      DrawFrameWithGUI();
      break;
    case DrawMode::NO_GUI:
      DrawFrameSimple();
      break;
    default:
      DrawFrameSimple();
  }
}

void SimpleShadowmapRender::SetupGUIElements()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
    ImGui::Begin("Shadowmap render settings");

    ImGui::SliderFloat("min height", &pushConst2M.minHeight, 0.f, pushConst2M.maxHeight, NULL, ImGuiSliderFlags_AlwaysClamp);
    ImGui::SliderFloat("max height", &pushConst2M.maxHeight, pushConst2M.minHeight, 10.f, NULL, ImGuiSliderFlags_AlwaysClamp);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::NewLine();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::Text("Changing bindings is not supported.");
    ImGui::Text("Vertex shader path: %s", VERTEX_SHADER_PATH.c_str());
    ImGui::Text("Fragment shader path: %s", FRAGMENT_SHADER_PATH.c_str());
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
}

