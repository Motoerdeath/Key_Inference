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
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "nvvk/profiler_vk.hpp"


//--------------------------------------------------------------------------------------------------
// This implements all graphical user interface of SampleExample.
class SampleExample;  // Forward declaration

class SampleGUI
{
public:
  SampleGUI(SampleExample* _s)
      : _se(_s)
  {
  }
  void render(nvvk::ProfilerVK& profiler);
  void titleBar();
  void menuBar();
  void showBusyWindow();
  bool VisualizeSortingGrid = false;
  int gridX{2};
  int gridY{2};
  int gridZ{2};
private:
  bool guiCamera();
  bool guiRayTracing();
  bool guiTonemapper();
  bool guiEnvironment();
  bool guiSortingGrid();
  bool guiStatistics();
  bool guiProfiler(nvvk::ProfilerVK& profiler);
  bool guiGpuMeasures();
  void prepareTimeData(TimingData data);

  SampleExample* _se{nullptr};
  std::vector<ProfilingStats> m_stats;
  nvh::Profiler::TimerInfo stored_timers[5];
  int stored_frames[4];
  //each enum stands for the prfilign of a certain time characteristic
  enum ProfilingMode{
    eSort = 0, //time for sorting
    eShade = 1, //time for shading
    eRayTraversal = 2 // time for ray traversal
  };

  int m_pMode{eShade};
  int histogramFlags =0;
  bool inferSortingKey = false;
  //uint NumCoherenceBits = _se->m_SERParameters.numCoherenceBitsTotal;
  
  std::vector<float> rttime;
  bool showHistogram{false};

  int manualSorting{1};



  
  float constantGridlearningSpeed = 0.2f;


  


};
