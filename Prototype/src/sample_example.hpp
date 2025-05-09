/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


#pragma once
#include <random>
#include "hdr_sampling.hpp"
#include "nvvk/gizmos_vk.hpp"
#include "renderer.h"

#include "json.hpp"
using json = nlohmann::json;
/*

 Structure of the application

    +--------------------------------------------+
    |             SampleExample                  |
    +--------+-----------------------------------+
    |  Pick  |    RtxPipeline   | other   ? ...  |
    +--------+---------+-------------------------+
    |       TLAS       |                         |
    +------------------+     Offscreen           |
    |      Scene       |                         |
    +------------------+-------------------------+

*/


// #define ALLOC_DMA  <--- This is in the CMakeLists.txt
#include "nvvk/resourceallocator_vk.hpp"
#if defined(ALLOC_DMA)
#include <nvvk/memallocator_dma_vk.hpp>
typedef nvvk::ResourceAllocatorDma Allocator;
#elif defined(ALLOC_VMA)
#include <nvvk/memallocator_vma_vk.hpp>
typedef nvvk::ResourceAllocatorVma Allocator;
#else
typedef nvvk::ResourceAllocatorDedicated Allocator;
#endif

#define CPP  // For sun_and_sky

#include "nvh/gltfscene.hpp"
#include "nvvkhl/appbase_vk.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/profiler_vk.hpp"
#include "nvvk/raytraceKHR_vk.hpp"
#include "nvvk/raypicker_vk.hpp"

#include "accelstruct.hpp"
#include "render_output.hpp"
#include "scene.hpp"
#include "shaders/host_device.h"

#include "imgui_internal.h"
#include "queue.hpp"
#include "nvvk/stagingmemorymanager_vk.hpp"
#include "sorting_grid.hpp"

class SampleGUI;




//--------------------------------------------------------------------------------------------------
// Simple rasterizer of OBJ objects
// - Each OBJ loaded are stored in an `ObjModel` and referenced by a `ObjInstance`
// - It is possible to have many `ObjInstance` referencing the same `ObjModel`
// - Rendering is done in an offscreen framebuffer
// - The image of the framebuffer is displayed in post-process in a full-screen quad
//
class SampleExample : public nvvkhl::AppBaseVk
{
  friend SampleGUI;

public:
  enum RndMethod
  {
    eRtxPipeline,
    eRayQuery,
    eNone,
  };

  enum Queues
  {
    eGCT0,
    eGCT1,
    eCompute,
    eTransfer
  };


  void setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, const std::vector<nvvk::Queue>& queues);

  bool isBusy() { return m_busy; }
  void createDescriptorSetLayout();
  void createUniformBuffer();
  void createUniformBufferProfiling();
  void destroyResources();
  void loadAssets(const char* filename);
  void loadEnvironmentHdr(const std::string& hdrFilename);
  void loadScene(const std::string& filename);
  void onFileDrop(const char* filename) override;
  void onKeyboard(int key, int scancode, int action, int mods) override;
  void onMouseButton(int button, int action, int mods) override;
  void onMouseMotion(int x, int y) override;
  void onResize(int /*w*/, int /*h*/) override;
  void renderGui(nvvk::ProfilerVK& profiler);
  void createRender(RndMethod method);
  void rebuildRender();//own contribution
  void reloadRender();//own contribution
  void resetFrame();
  void screenPicking();
  void updateFrame();
  void updateHdrDescriptors();
  void updateUniformBuffer(const VkCommandBuffer& cmdBuf);
  void prepareProfilingData(VkCommandBuffer cmdBuf);

  void beginSortingGridTraining();
  void iterateTrainingPosition();
  void doCycle();

  Scene              m_scene;
  AccelStructure     m_accelStruct;
  RenderOutput       m_offscreen;
  HdrSampling        m_skydome;
  nvvk::AxisVK       m_axis;
  nvvk::RayPickerKHR m_picker;

  // It is possible that ray query isn't supported (ex. Titan)
  void supportRayQuery(bool support) { m_supportRayQuery = support; }
  bool m_supportRayQuery{true};

  // All renderers
  std::array<Renderer*, eNone> m_pRender{nullptr, nullptr};
  RndMethod                    m_rndMethod{eNone};

  enum CubeSide
{
  CubeUp,
  CubeDown,
  CubeLeft,
  CubeRight,
  CubeFront,
  CubeBack
};
  bool useBestParameters = false;;
  CubeSide currentLookDirection;
  nvvk::Buffer m_sunAndSkyBuffer;
  nvvk::Buffer m_profilingBuffer;
  nvvk::Buffer m_sortingParametersBuffer; //UniformBuffers that contains the parameters chosen by User or the Classificator for SER
  nvvk::Buffer m_GridSortingKeyBuffer;
  const int MAXGRIDSIZE = 10;

  GridCube bestKeys[10*10*10];

  int bestSortMode = eNoSorting;
  int DELAY_FRAMES = 4;
  uint recoveredFrame[5] = {0,0,0,0,0};
  uint64_t recovered_time = 0;
  double avg_full_time = 0.0;
  TimingData latest_timeData;



  std::vector<std::vector<ProfilingStats>> profilingStats;

  // Graphic pipeline
  VkDescriptorPool            m_descPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout       m_descSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet             m_descSet{VK_NULL_HANDLE};
  nvvk::DescriptorSetBindings m_bind;

  Allocator       m_alloc;  // Allocator for buffer, images, acceleration structures
  nvvk::StagingMemoryManager m_staging;
  nvvk::DebugUtil m_debug;  // Utility to name objects


  VkRect2D m_renderRegion{};
  void     setRenderRegion(const VkRect2D& size);

  // #Post
  void createOffscreenRender();
  void drawPost(VkCommandBuffer cmdBuf);

  // #VKRay
  void renderScene(const VkCommandBuffer& cmdBuf, nvvk::ProfilerVK& profiler);

  //creates a SortingParameters Struct
  void createStorageBuffer();
  void updateStorageBuffer(const VkCommandBuffer& cmdBuf);
  SortingParameters createSortingParameters();


  RtxState m_rtxState{
      0,       // frame;
      10,      // maxDepth;
      1,       // maxSamples;
      1,       // fireflyClampThreshold;
      1,       // hdrMultiplier;
      0,       // debugging_mode;
      0,       // pbrMode;
      0,       // _pad0;
      {0, 0},  // size;
      0,       // minHeatmap;
      65000,    // maxHeatmap;
      {0.0,0.0,0.0}, // Maximum of Scene BoundingBox
      0,             // Sorting Grid X dimension
      {0.0,0.0,0.0}, // Minimum of Scene BoundingBox
      0,             // Sorting Grid Y Dimension 
      {0.0,0.0,0.0}, // Center of Scene Bounding Box
      0,             // Sorting Grid Z Dimension
      1.0,           // Size of the sortingCubes for display
      0,         // Visualization Mode
      32, //NumHintBits
      0, //SortAfterASTraversal
      0, //noSort;
      1, //HitObject;
      0, //rayOrigin;
      0,  //rayDirection;
      0,  // estimatedEndpoint;
      0,  // realEndpoint;
      0  // isFinished;
            
  };

  SunAndSky m_sunAndSky{
      {1, 1, 1},            // rgb_unit_conversion;
      0.0000101320f,        // multiplier;
      0.0f,                 // haze;
      0.0f,                 // redblueshift;
      1.0f,                 // saturation;
      0.0f,                 // horizon_height;
      {0.4f, 0.4f, 0.4f},   // ground_color;
      0.1f,                 // horizon_blur;
      {0.0, 0.0, 0.01f},    // night_color;
      0.8f,                 // sun_disk_intensity;
      {0.00, 0.78, 0.62f},  // sun_direction;
      5.0f,                 // sun_disk_scale;
      1.0f,                 // sun_glow_intensity;
      1,                    // y_is_up;
      1,                    // physically_scaled_sun;
      0,                    // in_use;
  };



  int         m_maxFrames{100000};
  bool        m_showAxis{true};
  bool        m_descaling{false};
  int         m_descalingLevel{1};
  bool        m_busy{false};
  std::string m_busyReasonText;


  std::random_device dev;
  std::mt19937 rng;

  std::shared_ptr<SampleGUI> m_gui;


  const float timePerCycle = 200.0;
  float timePerCubeSide = 1000.0f;

  float timeRemaining = timePerCycle;
  uint framesThisCycle = 0;

  bool activateParametertesting = false;



  std::vector<TimingObject> inferenceMeasurements;

float constantGridlearningSpeed = 0.2f;
bool useConstantGridLearning = true;

bool performAutomaticTraining{false};
bool GridWhite = false;
Grid grid;
void buildSortingGrid();

int trainingDirectionIndex = 0;
glm::vec3 trainingPosition = glm::vec3(0,0,0);

glm::vec3 lookDirections[6]{glm::vec3(0,1,0.1),glm::vec3(0,-1,-0.1),glm::vec3(-1,0,0),glm::vec3(1,0,0),glm::vec3(0,0,1),glm::vec3(0,0,-1)};

int grid_x = 2;
int grid_y = 2;
int grid_z = 2;

glm::ivec3 currentGridSpace;




std::vector<std::vector<std::vector<GridSpace>>> sortingGrid;

json fillJsonWithBestResult(json j);
json fillJsonWithAllResults(json j);
void SaveSortingGrid();



bool waitingOnPipeline = false;

CubeSideStorage* getCubeSideElements(CubeSide side,GridSpace* currentGrid);
CubeSideStorage* getCubeSideStorage(GridSpace* currentGrid);

GridCube determineBestTimesCube(GridSpace* currentGrid);

int getCubeSideHash(vec3 CubeCoords, CubeSide side);

glm::vec3 calculateGridSpaceCenter(glm::vec3 gridSpace);

void loadSortingGrid(const std::string& jsonFilename);
};
