// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#ifdef OSPRAY_USE_TBB
#  include <tbb/parallel_for.h>
#elif defined(OSPRAY_USE_CILK)
#  include <cilk/cilk.h>
#endif

namespace ospray {

// NOTE(jda) - This abstraction wraps "fork-join" parallelism, with an implied
//             synchronizsation after all of the tasks have run.
template<typename Task>
inline void parallel_for(int nTasks, const Task& fcn)
{
#ifdef OSPRAY_USE_TBB
  tbb::parallel_for(0, nTasks, 1, fcn);
#elif defined(OSPRAY_USE_CILK)
  cilk_for (int taskIndex = 0; taskIndex < nTasks; ++taskIndex) {
    fcn(taskIndex);
  }
#elif defined(OSPRAY_USE_OMP)
# pragma omp parallel for schedule(dynamic)
  for (int taskIndex = 0; taskIndex < nTasks; ++taskIndex) {
    fcn(taskIndex);
  }
#else // Debug (no tasking system)
  for (int taskIndex = 0; taskIndex < nTasks; ++taskIndex) {
    fcn(taskIndex);
  }
#endif
}

}//namespace ospray
