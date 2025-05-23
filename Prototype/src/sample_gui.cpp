/*
 * Copyright (c) 2021-2023, NVIDIA CORPORATION.  All rights reserved.
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
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


/*
 *  This implements all graphical user interface of SampleExample.
 */


#include <bitset>  // std::bitset
#include <iomanip>
#include <sstream>
#include <implot.h>
#include <fmt/format.h>

#include "imgui/imgui_camera_widget.h"
#include "imgui/imgui_helper.h"
#include "imgui/imgui_orient.h"
#include "rtx_pipeline.hpp"
#include "sample_example.hpp"
#include "sample_gui.hpp"
#include "tools.hpp"
#include "iostream"
#include <algorithm>

#include "nvml_monitor.hpp"
#ifdef _WIN32
#include <commdlg.h>
#endif  // _WIN32


using GuiH = ImGuiH::Control;

#if defined(NVP_SUPPORTS_NVML)
extern NvmlMonitor g_nvml;  // GPU load and memory
#endif

//--------------------------------------------------------------------------------------------------
// Main rendering function for all
//
void SampleGUI::render(nvvk::ProfilerVK& profiler)
{
  // Show UI panel window.
  float panelAlpha = 1.0f;
  if(_se->showGui())
  {
    ImGuiH::Control::style.ctrlPerc = 0.55f;
    ImGuiH::Panel::Begin(ImGuiH::Panel::Side::Right, panelAlpha);

    using Gui = ImGuiH::Control;
    bool changed{false};

    if(ImGui::CollapsingHeader("Camera" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
      changed |= guiCamera();
    if(ImGui::CollapsingHeader("Ray Tracing" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
      changed |= guiRayTracing();
    if(ImGui::CollapsingHeader("SortingGrid Learning" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
      changed |= guiSortingGrid();
    if(ImGui::CollapsingHeader("Tonemapper" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
      changed |= guiTonemapper();
    if(ImGui::CollapsingHeader("Environment" /*, ImGuiTreeNodeFlags_DefaultOpen*/))
      changed |= guiEnvironment();
    if(ImGui::CollapsingHeader("Stats"))
    {
      Gui::Group<bool>("Scene Info", false, [&] { return guiStatistics(); });
      Gui::Group<bool>("Profiler", true, [&] { return guiProfiler(profiler); });
      Gui::Group<bool>("Plot", false, [&] { return guiGpuMeasures(); });
    }
    ImGui::TextWrapped("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                       ImGui::GetIO().Framerate);

    if(changed)
    {
      _se->resetFrame();
    }

    ImGui::End();  // ImGui::Panel::end()
  }

  // Rendering region is different if the side panel is visible
  if(panelAlpha >= 1.0f && _se->showGui())
  {
    ImVec2 pos, size;
    ImGuiH::Panel::CentralDimension(pos, size);
    _se->setRenderRegion(VkRect2D{VkOffset2D{static_cast<int32_t>(pos.x), static_cast<int32_t>(pos.y)},
                                  VkExtent2D{static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y)}});
  }
  else
  {
    _se->setRenderRegion(VkRect2D{{}, _se->getSize()});
  }

  if(_se->activateParametertesting || _se->performAutomaticTraining)
  {
    _se->doCycle();
  }
  
}

//--------------------------------------------------------------------------------------------------
//
//
bool SampleGUI::guiCamera()
{
  bool changed{false};
  changed |= ImGuiH::CameraWidget();
  auto& cam = _se->m_scene.getCamera();
  changed |= GuiH::Slider("Aperture", "", &cam.aperture, nullptr, ImGuiH::Control::Flags::Normal, 0.0f, 0.5f);

  return changed;
}

//--------------------------------------------------------------------------------------------------
//
//
bool SampleGUI::guiRayTracing()
{
  auto  Normal = ImGuiH::Control::Flags::Normal;
  bool  changed{false};
  bool changedGrid{false};
  auto& rtxState(_se->m_rtxState);

  changed |= GuiH::Slider("Max Ray Depth", "", &rtxState.maxDepth, nullptr, Normal, 1, 40);
  changed |= GuiH::Slider("Samples Per Frame", "", &rtxState.maxSamples, nullptr, Normal, 1, 10);
  changed |= GuiH::Slider("Max Iteration ", "", &_se->m_maxFrames, nullptr, Normal, 1, 1000);
  changed |= GuiH::Slider("De-scaling ",
                          "Reduce resolution while navigating.\n"
                          "Speeding up rendering while camera moves.\n"
                          "Value of 1, will not de-scale",
                          &_se->m_descalingLevel, nullptr, Normal, 1, 8);

  changed |= GuiH::Selection("Pbr Mode", "PBR material model", &rtxState.pbrMode, nullptr, Normal, {"Disney", "Gltf"});

  static bool bAnyHit = true;
  static bool bProfiling = false;
  if(_se->m_rndMethod == SampleExample::RndMethod::eRtxPipeline)
  {
    auto rtx = dynamic_cast<RtxPipeline*>(_se->m_pRender[_se->m_rndMethod]);
    if(GuiH::Checkbox("Enable AnyHit", "AnyHit is used for double sided, cutout opacity, but can be slower when all objects are opaque",
                      &bAnyHit, nullptr))
    {
      auto rtx = dynamic_cast<RtxPipeline*>(_se->m_pRender[_se->m_rndMethod]);
      vkDeviceWaitIdle(_se->m_device);  // cannot run while changing this
      rtx->useAnyHit(bAnyHit);
      changed = true;
    }   
    if(GuiH::Checkbox("Enable Profiling", "",
                      &bProfiling, nullptr))
    {
      auto rtx = dynamic_cast<RtxPipeline*>(_se->m_pRender[_se->m_rndMethod]);
      vkDeviceWaitIdle(_se->m_device);  // cannot run while changing this
      rtx->enableProfiling(bProfiling);
      changed = true;
    }

    ImGui::RadioButton("Manual",&manualSorting, 1);
    if(manualSorting > 0)
    {
      if(GuiH::Selection("Sorting Mode", "Display unique values of material", rtx->getSortingMode(), nullptr, Normal,{
                                   "No Sorting",
                                   "HitObject",
                                   "Sort by Origin",
                                   "Sort by Origin&Direction",
                                   "Sort by Origin&Direction reversed",
                                   "Sort by Origin&Direction interleaved",
                                   "Twopoint sorting",
                                   "Endpoint Estimation",
                                   "Adaptive Endpoint Estimation",
                                   "Infer Sorting Key",}))
      {
        vkDeviceWaitIdle(_se->m_device);  // cannot run while changing this
        _se->reloadRender();
        changed = true;
      }
    }

    //changed |= GuiH::Slider("Number Coherence Bits", "", &_se->m_SERParameters.numCoherenceBitsTotal, nullptr, Normal, 0u, 64u);
    GuiH::Slider("Number Coherence Bits", "", &rtx->m_SERParameters.numCoherenceBitsTotal, nullptr, Normal, 0u, 64u);
  }
  
  GuiH::Group<bool>("Profiling", false, [&] {

    ImGui::RadioButton("Shading Time",&m_pMode,eShade);
    ImGui::SameLine();
    ImGui::RadioButton("Sorting Time",&m_pMode,eSort);
    
    ImGui::RadioButton("Ray Traversal Time",&m_pMode,eRayTraversal);
    ImGui::SameLine();
    ImGui::RadioButton("Ray Tracing Time",&m_pMode,eTracing);


    GuiH::Checkbox("show Histogram","",&showHistogram);
    
    
  
    if(showHistogram)
    {
      ImGui::RadioButton("Standard",&histogramFlags,ImPlotHistogramFlags_None);
      ImGui::SameLine();
      ImGui::RadioButton("Density",&histogramFlags,ImPlotHistogramFlags_Density);
      ImGui::SameLine();
      ImGui::RadioButton("Cumulative",&histogramFlags,ImPlotHistogramFlags_Cumulative);

      if(ImPlot::BeginPlot("First Plot"))
      {
      ImPlot::SetupAxes("Time","#Threads");
      std::vector<float> rttime;
      std::vector<ProfilingStats> stats = _se->profilingStats[*(dynamic_cast<RtxPipeline*>(_se->m_pRender[_se->m_rndMethod])->getSortingMode())];
      for (ProfilingStats timing : stats)
      {
        if(m_pMode == eShade)
        {
          rttime.emplace_back((float)timing.shadeTiming.avg_time);
        }
        if(m_pMode == eSort)
        {
          rttime.emplace_back((float)timing.sortTiming.avg_time);
        }
        if(m_pMode == eRayTraversal)
        {
          rttime.emplace_back((float)timing.rtTiming.avg_time);
        }
        if(m_pMode == eTracing)
        {
          rttime.emplace_back((float)timing.traceTiming.avg_time);
        }
      }
      ImPlot::PlotHistogram("first histogram",rttime.data(),rttime.size(),ImPlotBin_Sqrt,(1.0),ImPlotRange(),histogramFlags);
      ImPlot::EndPlot();
      }
    }
    return false;
  }); 
  

  GuiH::Group<bool>("Debugging", false, [&] {
    changed |= GuiH::Selection("Debug Mode", "Display unique values of material", &rtxState.debugging_mode, nullptr, Normal,
                               {
                                   "No Debug",
                                   "BaseColor",
                                   "Normal",
                                   "Metallic",
                                   "Emissive",
                                   "Alpha",
                                   "Roughness",
                                   "TexCoord",
                                   "Tangent",
                                   "Radiance",
                                   "Weight",
                                   "RayDir",
                                   "HeatMap",
                               });

    if(rtxState.debugging_mode == eHeatmap)
    {
      changed |= GuiH::Drag("Min Heat map", "Minimum timing value, below this value it will be blue",
                            &rtxState.minHeatmap, nullptr, Normal, 0, 1'000'000, 100);
      changed |= GuiH::Drag("Max Heat map", "Maximum timing value, above this value it will be red",
                            &rtxState.maxHeatmap, nullptr, Normal, 0, 1'000'000, 100);
    }

    return changed;
  });

  /* DO not need Ray Query here
  if(_se->m_supportRayQuery)
  {
    SampleExample::RndMethod method = _se->m_rndMethod;
    if(GuiH::Selection<int>("Rendering Pipeline", "Choose the type of rendering", (int*)&method, nullptr,
                            GuiH::Control::Flags::Normal, {"Ray Tracing Pipeline", "Ray Query"}))
    {
      _se->createRender(method);
      changed = true;
    }
  }
  */
  if(GuiH::button("Reload Shaders","",""))
  {
    _se->reloadRender();
    changed = true;
  }

  GuiH::Info("Frame", "", std::to_string(rtxState.frame), GuiH::Flags::Disabled);
  return changed;
}


bool SampleGUI::guiSortingGrid()

{
  bool changed{false};
  auto  Normal = ImGuiH::Control::Flags::Normal;

  if(GuiH::Slider("Grid X", "", &gridX, nullptr, Normal, 1, 10) || GuiH::Slider("Grid Y", "", &gridY, nullptr, Normal, 1, 10) || GuiH::Slider("Grid Z", "", &gridZ, nullptr, Normal, 1, _se->MAXGRIDSIZE))
  {
    if(!_se->performAutomaticTraining)
    {
      _se->grid_x = gridX;
      _se->grid_y = gridY;
      _se->grid_z = gridZ;
      _se->buildSortingGrid();
      changed = true;
    }
    else {
      gridX = _se->grid_x;
      gridY = _se->grid_y;
      gridZ = _se->grid_z;

    }

  }
  ImGui::Text(("Current Grid Position [x,y,z]: ("+  std::to_string(_se->currentGridSpace.x) + "," +  std::to_string(_se->currentGridSpace.y)  + "," +  std::to_string(_se->currentGridSpace.z) + ")").c_str());

  GuiH::Checkbox("Use Constant Grid Learning Speed","",&_se->useConstantGridLearning);
  if(_se->useConstantGridLearning)
  {
    GuiH::Slider("Constant Learning Speed","",&_se->constantGridlearningSpeed,nullptr,Normal,0.01f,1.0f,nullptr);
  } else 
  {
    float currentAdaptiveLearningRate = _se->grid.gridSpaces[_se->currentGridSpace.z][_se->currentGridSpace.y][_se->currentGridSpace.x].adaptiveGridLearningRate;
  
    ImGui::Text(("Current Grid Cell learning Rate: "+ std::to_string(currentAdaptiveLearningRate)).c_str());
  }
  auto rtx = dynamic_cast<RtxPipeline*>(_se->m_pRender[_se->m_rndMethod]);

  if(GuiH::Checkbox("Activate Async Pipeline Creation","",&rtx->useAsyncPipelineCreation))
  {
    if(rtx->useAsyncPipelineCreation)
    {
      rtx->activateAsyncPipelineCreation();
    } else {
      //rtx->destroyAsyncPipelineBuffer();
    }
  }
  //printf("Current Grid Position [x,y]: (%d , %d)\n", _se->currentGridSpace.x,_se->currentGridSpace.y);

  if(GuiH::button("save SortingGrid to File","save",""))
  {
    _se->SaveSortingGrid();
  }
  if(GuiH::button("NewAsyncPipeline","useNewPipeline",""))
  {
    vkDeviceWaitIdle(_se->m_device);
    rtx->setNewPipeline();
    //rtx->setNewPipeline_WithoutDestroying();
  }
  ImGui::Text("%d",_se->currentLookDirection);
  if(GuiH::Checkbox("Visualize Sorting method","",&VisualizeSortingGrid))
  {
    if(VisualizeSortingGrid)
    {
      _se->m_rtxState.VisualizeSortingGrid = 1;
    } else {
      _se->m_rtxState.VisualizeSortingGrid = 0;
    }
    
    changed = true;
  }

  if(VisualizeSortingGrid)
  {
    if(GuiH::Slider("size of display cubes","",&_se->m_rtxState.DisplayCubeSize,nullptr,Normal,0.1f,2.0f,nullptr))
    {
      changed = true;
    }
    
  }

  if(GuiH::button("randomize Parameters","new parameters",""))
    {
      rtx->m_SERParameters= _se->createSortingParameters();
      _se->reloadRender();
    }
  //Display current Sorting parameters
    ImGui::Text("Sorting Parameter");
    ImGui::Text(("NumCoherenceBitsTotal: "+ std::to_string(rtx->m_SERParameters.numCoherenceBitsTotal)).c_str());
    ImGui::Text(("sortAfterASTraversal: "+ std::to_string(rtx->m_SERParameters.sortAfterASTraversal)).c_str());
    ImGui::Text(("No Sorting: "+ std::to_string(rtx->m_SERParameters.noSort)).c_str());
    ImGui::Text(("hitObject: "+ std::to_string(rtx->m_SERParameters.hitObject)).c_str());
    ImGui::Text(("rayOrigin: "+ std::to_string(rtx->m_SERParameters.rayOrigin)).c_str());
    ImGui::Text(("rayDirection: "+ std::to_string(rtx->m_SERParameters.rayDirection)).c_str());
    ImGui::Text(("estimatedEndpoint: "+ std::to_string(rtx->m_SERParameters.estimatedEndpoint)).c_str());
    ImGui::Text(("realEndpoint: "+ std::to_string(rtx->m_SERParameters.realEndpoint)).c_str());
    ImGui::Text(("isFinished: "+ std::to_string(rtx->m_SERParameters.isFinished)).c_str());


  if( GuiH::Checkbox("perform automatic training","",&_se->performAutomaticTraining))
  {
    if(_se->performAutomaticTraining)
    {_se->beginSortingGridTraining();
    } else {
      
    }
  }

 
  if(!_se->performAutomaticTraining)
  {
    GuiH::Checkbox("activate Inference","",&(_se->activateParametertesting));
  }
  if(!(_se->activateParametertesting ||_se->performAutomaticTraining))
  {
    GuiH::Checkbox("always use best Parameters found","",&(_se->useBestParameters));
  }
  
  return changed;
}

bool SampleGUI::guiTonemapper()
{
  static Tonemapper default_tm{
      .brightness     = 1.0f,
      .contrast       = 1.0f,
      .saturation     = 1.0f,
      .vignette       = 0.0f,
      .avgLum         = 1.0f,
      .zoom           = 1.0f,
      .renderingRatio = {1.0f, 1.0f},
      .autoExposure   = 0,
      .Ywhite         = 0.5f,
      .key            = 0.5f,
      .dither         = 1,
  };

  auto&          tm = _se->m_offscreen.m_tonemapper;
  bool           changed{false};
  std::bitset<8> b(tm.autoExposure);

  bool autoExposure = b.test(0);

  changed |= GuiH::Checkbox("Auto Exposure", "Adjust exposure", (bool*)&autoExposure);
  changed |= GuiH::Slider("Exposure", "Scene Exposure", &tm.avgLum, &default_tm.avgLum, GuiH::Flags::Normal, 0.001f, 5.00f);
  changed |= GuiH::Slider("Brightness", "", &tm.brightness, &default_tm.brightness, GuiH::Flags::Normal, 0.0f, 2.0f);
  changed |= GuiH::Slider("Contrast", "", &tm.contrast, &default_tm.contrast, GuiH::Flags::Normal, 0.0f, 2.0f);
  changed |= GuiH::Slider("Saturation", "", &tm.saturation, &default_tm.saturation, GuiH::Flags::Normal, 0.0f, 5.0f);
  changed |= GuiH::Slider("Vignette", "", &tm.vignette, &default_tm.vignette, GuiH::Flags::Normal, 0.0f, 2.0f);
  changed |= GuiH::Checkbox("Dither", "Help hiding banding artifacts", (bool*)&tm.dither, (bool*)&default_tm.dither);


  if(autoExposure)
  {
    bool localExposure = b.test(1);
    GuiH::Group<bool>("Auto Settings", true, [&] {
      changed |= GuiH::Checkbox("Local", "", &localExposure);
      changed |= GuiH::Slider("Burning White", "", &tm.Ywhite, &default_tm.Ywhite, GuiH::Flags::Normal, 0.0f, 1.0f);
      changed |= GuiH::Slider("Brightness", "", &tm.key, &default_tm.key, GuiH::Flags::Normal, 0.0f, 1.0f);
      b.set(1, localExposure);
      return changed;
    });
  }
  b.set(0, autoExposure);
  tm.autoExposure = b.to_ulong();

  return false;  // no need to restart the renderer
}

//--------------------------------------------------------------------------------------------------
//
//
bool SampleGUI::guiEnvironment()
{
  static SunAndSky dss{
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

  bool  changed{false};
  auto& sunAndSky(_se->m_sunAndSky);

  changed |= ImGui::Checkbox("Use Sun & Sky", (bool*)&sunAndSky.in_use);
  changed |= GuiH::Slider("Exposure", "Intensity of the environment", &_se->m_rtxState.hdrMultiplier, nullptr,
                          GuiH::Flags::Normal, 0.f, 5.f);

  // Adjusting the up with the camera
  glm::vec3 eye, center, up;
  CameraManip.getLookat(eye, center, up);
  sunAndSky.y_is_up = (up.y == 1);

  if(sunAndSky.in_use)
  {
    GuiH::Group<bool>("Sun", true, [&] {
      changed |= GuiH::Custom("Direction", "Sun Direction", [&] {
        float indent = ImGui::GetCursorPos().x;
        changed |= ImGui::DirectionGizmo("", &sunAndSky.sun_direction.x, true);
        ImGui::NewLine();
        ImGui::SameLine(indent);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        changed |= ImGui::InputFloat3("##IG", &sunAndSky.sun_direction.x);
        return changed;
      });
      changed |= GuiH::Slider("Disk Scale", "", &sunAndSky.sun_disk_scale, &dss.sun_disk_scale, GuiH::Flags::Normal, 0.f, 100.f);
      changed |= GuiH::Slider("Glow Intensity", "", &sunAndSky.sun_glow_intensity, &dss.sun_glow_intensity,
                              GuiH::Flags::Normal, 0.f, 5.f);
      changed |= GuiH::Slider("Disk Intensity", "", &sunAndSky.sun_disk_intensity, &dss.sun_disk_intensity,
                              GuiH::Flags::Normal, 0.f, 5.f);
      changed |= GuiH::Color("Night Color", "", &sunAndSky.night_color.x, &dss.night_color.x, GuiH::Flags::Normal);
      return changed;
    });

    GuiH::Group<bool>("Ground", true, [&] {
      changed |= GuiH::Slider("Horizon Height", "", &sunAndSky.horizon_height, &dss.horizon_height, GuiH::Flags::Normal, -1.f, 1.f);
      changed |= GuiH::Slider("Horizon Blur", "", &sunAndSky.horizon_blur, &dss.horizon_blur, GuiH::Flags::Normal, 0.f, 1.f);
      changed |= GuiH::Color("Ground Color", "", &sunAndSky.ground_color.x, &dss.ground_color.x, GuiH::Flags::Normal);
      changed |= GuiH::Slider("Haze", "", &sunAndSky.haze, &dss.haze, GuiH::Flags::Normal, 0.f, 15.f);
      return changed;
    });

    GuiH::Group<bool>("Other", false, [&] {
      changed |= GuiH::Drag("Multiplier", "", &sunAndSky.multiplier, &dss.multiplier, GuiH::Flags::Normal, 0.f,
                            std::numeric_limits<float>::max(), 2, "%5.5f");
      changed |= GuiH::Slider("Saturation", "", &sunAndSky.saturation, &dss.saturation, GuiH::Flags::Normal, 0.f, 1.f);
      changed |= GuiH::Slider("Red Blue Shift", "", &sunAndSky.redblueshift, &dss.redblueshift, GuiH::Flags::Normal, -1.f, 1.f);
      changed |= GuiH::Color("RGB Conversion", "", &sunAndSky.rgb_unit_conversion.x, &dss.rgb_unit_conversion.x, GuiH::Flags::Normal);

      glm::vec3 eye, center, up;
      CameraManip.getLookat(eye, center, up);
      sunAndSky.y_is_up = up.y == 1;
      changed |= GuiH::Checkbox("Y is Up", "", (bool*)&sunAndSky.y_is_up, nullptr, GuiH::Flags::Disabled);
      return changed;
    });
  }

  return changed;
}

//--------------------------------------------------------------------------------------------------
//
//
bool SampleGUI::guiStatistics()
{
  ImGuiStyle& style    = ImGui::GetStyle();
  auto        pushItem = style.ItemSpacing;
  style.ItemSpacing.y  = -4;  // making the lines more dense

  auto& stats = _se->m_scene.getStat();

  if(stats.nbCameras > 0)
    GuiH::Info("Cameras", "", FormatNumbers(stats.nbCameras));
  if(stats.nbImages > 0)
    GuiH::Info("Images", "", FormatNumbers(stats.nbImages) + " (" + FormatNumbers(stats.imageMem) + ")");
  if(stats.nbTextures > 0)
    GuiH::Info("Textures", "", FormatNumbers(stats.nbTextures));
  if(stats.nbMaterials > 0)
    GuiH::Info("Material", "", FormatNumbers(stats.nbMaterials));
  if(stats.nbSamplers > 0)
    GuiH::Info("Samplers", "", FormatNumbers(stats.nbSamplers));
  if(stats.nbNodes > 0)
    GuiH::Info("Nodes", "", FormatNumbers(stats.nbNodes));
  if(stats.nbMeshes > 0)
    GuiH::Info("Meshes", "", FormatNumbers(stats.nbMeshes));
  if(stats.nbLights > 0)
    GuiH::Info("Lights", "", FormatNumbers(stats.nbLights));
  if(stats.nbTriangles > 0)
    GuiH::Info("Triangles", "", FormatNumbers(stats.nbTriangles));
  if(stats.nbUniqueTriangles > 0)
    GuiH::Info("Unique Tri", "", FormatNumbers(stats.nbUniqueTriangles));
  GuiH::Info("Resolution", "", std::to_string(_se->m_size.width) + "x" + std::to_string(_se->m_size.height));

  style.ItemSpacing = pushItem;

  return false;
}


void SampleGUI::prepareTimeData(TimingData data)
{

}
//--------------------------------------------------------------------------------------------------
//
//
bool SampleGUI::guiProfiler(nvvk::ProfilerVK& profiler)
{
  struct Info
  {
    vec2  statRender{0.0f, 0.0f};
    vec2  statTone{0.0f, 0.0f};
    vec2  statRenderEnd{0.0f, 0.0f};
    float frameTime{0.0f};
  };
  static Info  display;
  static Info  collect;
  static float mipmapGen{0.f};

  // Collecting data
  static float dirtyCnt = 0.0f;
  {
    dirtyCnt++;
    nvh::Profiler::TimerInfo info;
    nvh::Profiler::TimerInfo end_info;
    profiler.getTimerInfo("Render", info);
    int current_index = _se->m_rtxState.frame % 5;
    stored_frames[current_index] = _se->m_rtxState.frame;
    stored_timers[current_index] = info;
    collect.statRender.x += float(info.gpu.average / 1000.0f);
    collect.statRender.y += float(info.cpu.average / 1000.0f);

    profiler.getTimerInfo("Render Section", end_info);
    /*
    profiler.getTimerInfo("Render End", end_info);
    collect.statRenderEnd.x += float(info.gpu.average / 1000.0f) - float(end_info.gpu.average / 1000.0f);
    collect.statRenderEnd.y += float(info.cpu.average / 1000.0f) - float(end_info.cpu.average / 1000.0f);
    */
    profiler.getTimerInfo("Tonemap", info);
    collect.statTone.x += float(info.gpu.average / 1000.0f);
    collect.statTone.y += float(info.cpu.average / 1000.0f);
    collect.frameTime += 1000.0f / ImGui::GetIO().Framerate;


    if(_se->m_offscreen.m_tonemapper.autoExposure == 1)
    {
      profiler.getTimerInfo("Mipmap", info);
      mipmapGen = float(info.gpu.average / 1000.0f);
      //LOGI("Mipmap Generation: %.2fms\n", info.gpu.average / 1000.0f);
    }
  }

  // Averaging display of the data every 0.5 seconds
  static float dirtyTimer = 1.0f;
  dirtyTimer += ImGui::GetIO().DeltaTime;
  if(dirtyTimer >= 0.5f)
  {
    display.statRender = collect.statRender / dirtyCnt;
    //display.statRenderEnd = collect.statRenderEnd / dirtyCnt;
    display.statTone   = collect.statTone / dirtyCnt;
    display.frameTime  = collect.frameTime / dirtyCnt;
    dirtyTimer         = 0;
    dirtyCnt           = 0;
    collect            = Info{};
  }
  


  ImGui::Text("Frame     [ms]: %2.3f", display.frameTime);
  ImGui::Text("Render GPU/CPU [ms]: %2.3f  /  %2.3f", display.statRender.x, display.statRender.y);

  uint64_t one = 1;
  ImGui::Text("gpu time   [ms]: %2.3f", (double) (_se->latest_timeData.full_time/glm::max(_se->latest_timeData.full_time_threads, one)));

  ImGui::Text("frame time   [ms]: %2.3f", (double) (_se->latest_timeData.frameTime/glm::max(_se->latest_timeData.frameTimeThreads, one)));
  ImGui::Text("noSort time   [ms]: %2.3f", (double) (_se->latest_timeData.noSortTime/glm::max(_se->latest_timeData.noSortThreads, one)));
  ImGui::Text("hitobject time   [ms]: %2.3f", (double) (_se->latest_timeData.hitObjectTime/glm::max(_se->latest_timeData.hitObjectThreads, one)));
  ImGui::Text("origin time   [ms]: %2.3f", (double) (_se->latest_timeData.originTime/glm::max(_se->latest_timeData.originThreads, one)));
  ImGui::Text("reis time   [ms]: %2.3f", (double) (_se->latest_timeData.reisTime/glm::max(_se->latest_timeData.reisThreads, one)));
  ImGui::Text("costa time   [ms]: %2.3f", (double) (_se->latest_timeData.costaTime/glm::max(_se->latest_timeData.costaThreads, one)));
  ImGui::Text("aila time   [ms]: %2.3f", (double) (_se->latest_timeData.ailaTime/glm::max(_se->latest_timeData.ailaThreads, one)));
  ImGui::Text("twopoint time   [ms]: %2.3f", (double) (_se->latest_timeData.twoPointTime/glm::max(_se->latest_timeData.twoPointThreads, one)));
  ImGui::Text("endpoint time   [ms]: %2.3f", (double) (_se->latest_timeData.endPointEstTime/glm::max(_se->latest_timeData.endPointEstThreads, one)));
  ImGui::Text("adaptive time   [ms]: %2.3f", (double) (_se->latest_timeData.endEstAdaptiveTime/glm::max(_se->latest_timeData.endEstAdaptiveThreads, one)));
  

  ImGui::Text("Frame     : %1d", glm::max(stored_frames[(_se->m_rtxState.frame-4) % 5],0));
  ImGui::Text("Frame gpu : %1d", _se->latest_timeData.frame);
 // ImGui::Text("Render_End GPU/CPU [ms]: %2.3f  /  %2.3f", display.statRenderEnd.x, display.statRenderEnd.y);
  ImGui::Text("Tone+UI GPU/CPU [ms]: %2.3f  /  %2.3f", display.statTone.x, display.statTone.y);
  if(_se->m_offscreen.m_tonemapper.autoExposure == 1)
    ImGui::Text("Mipmap Gen: %2.3fms", mipmapGen);
  ImGui::ProgressBar(display.statRender.x / display.frameTime);


  return false;
}

//--------------------------------------------------------------------------------------------------
//
//


//-----------------------------------------------------------------------------
int metricFormatter(double value, char* buff, int size, void* data)
{
  const char*        unit       = (const char*)data;
  static double      s_value[]  = {1000000000, 1000000, 1000, 1, 0.001, 0.000001, 0.000000001};
  static const char* s_prefix[] = {"G", "M", "k", "", "m", "u", "n"};
  if(value == 0)
  {
    return snprintf(buff, size, "0 %s", unit);
  }
  for(int i = 0; i < 7; ++i)
  {
    if(fabs(value) >= s_value[i])
    {
      return snprintf(buff, size, "%g %s%s", value / s_value[i], s_prefix[i], unit);
    }
  }
  return snprintf(buff, size, "%g %s%s", value / s_value[6], s_prefix[6], unit);
}


void imguiGraphLines(uint32_t gpuIndex)
{
#if defined(NVP_SUPPORTS_NVML)
  const int offset = g_nvml.getOffset();

  // Display Graphs
  const NvmlMonitor::SysInfo& cpuMeasure = g_nvml.getSysInfo();
  const NvmlMonitor::GpuInfo& info       = g_nvml.getInfo(gpuIndex);
  const std::vector<float>&   gpuLoad    = g_nvml.getMeasures(gpuIndex).load;
  const std::vector<float>&   gpuMem     = g_nvml.getMeasures(gpuIndex).memory;

  float       memUsage   = static_cast<float>(gpuMem[offset]) / info.max_mem * 100.f;
  std::string lineString = fmt::format("Load: {}%", gpuLoad[offset]);
  std::string memString  = fmt::format("Memory: {:.0f}%", memUsage);

  static ImPlotFlags     s_plotFlags = ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText | ImPlotFlags_Crosshairs;
  static ImPlotAxisFlags s_axesFlags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoLabel;
  static ImColor         s_lineColor = ImColor(0.07f, 0.9f, 0.06f, 1.0f);
  static ImColor         s_memColor  = ImColor(0.06f, 0.6f, 0.97f, 1.0f);
  static ImColor         s_cpuColor  = ImColor(0.96f, 0.96f, 0.0f, 1.0f);

#define SAMPLING_NUM 100  // Show 100 measurements

  if(ImPlot::BeginPlot(info.name.c_str(), ImVec2(-1, 0), s_plotFlags))
  {
    ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_NoButtons);
    ImPlot::SetupAxes(nullptr, "Load", s_axesFlags | ImPlotAxisFlags_NoDecorations, s_axesFlags);
    ImPlot::SetupAxis(ImAxis_Y2, "Mem", ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_Opposite);
    ImPlot::SetupAxesLimits(0, SAMPLING_NUM, 0, 100);
    ImPlot::SetupAxisLimits(ImAxis_Y2, 0, float(info.max_mem));
    ImPlot::SetupAxisFormat(ImAxis_Y2, metricFormatter, (void*)"iB");
    ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);

    // Load
    ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
    ImPlot::SetNextFillStyle(s_lineColor);
    ImPlot::PlotShaded(lineString.c_str(), gpuLoad.data(), (int)gpuLoad.size(), -INFINITY, 1.0, 0.0, 0, offset + 1);
    ImPlot::SetNextLineStyle(s_lineColor);
    ImPlot::PlotLine(lineString.c_str(), gpuLoad.data(), (int)gpuLoad.size(), 1.0, 0.0, 0, offset + 1);
    // Memory
    ImPlot::SetAxes(ImAxis_X1, ImAxis_Y2);
    ImPlot::SetNextFillStyle(s_memColor);
    ImPlot::PlotShaded(memString.c_str(), gpuMem.data(), (int)gpuMem.size(), -INFINITY, 1.0, 0.0, 0, offset + 1);
    ImPlot::SetNextLineStyle(s_memColor);
    ImPlot::PlotLine(memString.c_str(), gpuMem.data(), (int)gpuMem.size(), 1.0, 0.0, 0, offset + 1);
    ImPlot::PopStyleVar();
    // CPU
    ImPlot::SetAxes(ImAxis_X1, ImAxis_Y1);
    ImPlot::SetNextLineStyle(s_cpuColor);
    ImPlot::PlotLine("CPU", cpuMeasure.cpu.data(), (int)cpuMeasure.cpu.size(), 1.0, 0.0, 0, offset + 1);

    if(ImPlot::IsPlotHovered())
    {
      ImPlotPoint mouse     = ImPlot::GetPlotMousePos();
      int         gpuOffset = (int(mouse.x) + offset) % (int)gpuLoad.size();
      int         cpuOffset = (int(mouse.x) + offset) % (int)cpuMeasure.cpu.size();

      char buff[32];
      metricFormatter(static_cast<double>(gpuMem[gpuOffset]), buff, 32, (void*)"iB");

      ImGui::BeginTooltip();
      ImGui::Text("Load: %3.0f%%", gpuLoad[gpuOffset]);
      ImGui::Text("Memory: %s", buff);
      ImGui::Text("Cpu: %3.0f%%", cpuMeasure.cpu[cpuOffset]);
      ImGui::EndTooltip();
    }

    ImPlot::EndPlot();
  }
#endif
}

bool SampleGUI::guiGpuMeasures()
{
#if defined(NVP_SUPPORTS_NVML)

  for(uint32_t g = 0; g < g_nvml.nbGpu(); g++)  // Number of gpu
  {
    imguiGraphLines(g);
  }
#else
  ImGui::Text("NVML wasn't loaded");
#endif
  return false;
}


//--------------------------------------------------------------------------------------------------
// This is displaying information in the titlebar
//
void SampleGUI::titleBar()
{
  static float dirtyTimer = 0.0f;

  dirtyTimer += ImGui::GetIO().DeltaTime;
  if(dirtyTimer > 1)
  {
    std::stringstream o;
    o << "VK glTF Viewer";
    o << " | " << _se->m_scene.getSceneName();                                                   // Scene name
    o << " | " << _se->m_renderRegion.extent.width << "x" << _se->m_renderRegion.extent.height;  // resolution
    o << " | " << static_cast<int>(ImGui::GetIO().Framerate)                                     // FPS / ms
      << " FPS / " << std::setprecision(3) << 1000.F / ImGui::GetIO().Framerate << "ms";
#if defined(NVP_SUPPORTS_NVML)
    if(g_nvml.isValid())  // Graphic card, driver
    {
      const auto& i = g_nvml.getInfo(0);
      o << " | " << i.name;
      o << " | " << g_nvml.getSysInfo().driverVersion;
    }
#endif
    if(_se->m_rndMethod != SampleExample::eNone && _se->m_pRender[_se->m_rndMethod] != nullptr)
      o << " | " << _se->m_pRender[_se->m_rndMethod]->name();
    glfwSetWindowTitle(_se->m_window, o.str().c_str());
    dirtyTimer = 0;
  }
}

//--------------------------------------------------------------------------------------------------
//
//
void SampleGUI::menuBar()
{
  auto openFilename = [](const char* filter) {
#ifdef _WIN32
    char          filename[MAX_PATH] = {0};
    OPENFILENAMEA ofn;
    ZeroMemory(&filename, sizeof(filename));
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nullptr;  // If you have a window to center over, put its HANDLE here
    ofn.lpstrFilter = filter;
    ofn.lpstrFile   = filename;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrTitle  = "Select a File";
    ofn.Flags       = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

    if(GetOpenFileNameA(&ofn))
    {
      return std::string(filename);
    }
#endif

    return std::string("");
  };


  // Menu Bar
  if(ImGui::BeginMainMenuBar())
  {
    if(ImGui::BeginMenu("File"))
    {
      if(ImGui::MenuItem("Open GLTF Scene"))
        _se->loadAssets(openFilename("GLTF Files\0*.gltf;*.glb\0\0").c_str());
      if(ImGui::MenuItem("Open HDR Environment"))
        _se->loadAssets(openFilename("HDR Files\0*.hdr\0\0").c_str());
      if(ImGui::MenuItem("Load Sorting Grid"))
        _se->loadAssets(openFilename("Json Files\0*.json\0\0").c_str());
      ImGui::Separator();
      if(ImGui::MenuItem("Quit", "ESC"))
        glfwSetWindowShouldClose(_se->m_window, 1);
      ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Tools"))
    {
      ImGui::MenuItem("Settings", "F10", &_se->m_show_gui);
      ImGui::MenuItem("Axis", nullptr, &_se->m_showAxis);
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }
}


//--------------------------------------------------------------------------------------------------
// Display a static window when loading assets
//
void SampleGUI::showBusyWindow()
{
  static int   nb_dots   = 0;
  static float deltaTime = 0;
  bool         show      = true;
  size_t       width     = 270;
  size_t       height    = 60;

  deltaTime += ImGui::GetIO().DeltaTime;
  if(deltaTime > .25)
  {
    deltaTime = 0;
    nb_dots   = ++nb_dots % 10;
  }

  ImGui::SetNextWindowSize(ImVec2(float(width), float(height)));
  ImGui::SetNextWindowPos(ImVec2(float(_se->m_size.width - width) * 0.5f, float(_se->m_size.height - height) * 0.5f));

  ImGui::SetNextWindowBgAlpha(0.75f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 15.0);
  if(ImGui::Begin("##notitle", &show,
                  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
                      | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMouseInputs))
  {
    ImVec2 available = ImGui::GetContentRegionAvail();

    ImVec2 text_size = ImGui::CalcTextSize(_se->m_busyReasonText.c_str(), nullptr, false, available.x);

    ImVec2 pos = ImGui::GetCursorPos();
    pos.x += (available.x - text_size.x) * 0.5f;
    pos.y += (available.y - text_size.y) * 0.5f;

    ImGui::SetCursorPos(pos);
    ImGui::TextWrapped("%s", (_se->m_busyReasonText + std::string(nb_dots, '.')).c_str());
  }
  ImGui::PopStyleVar();
  ImGui::End();
}
