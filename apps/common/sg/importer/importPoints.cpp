// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
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

#include "Importer.h"
#include "common/sg/SceneGraph.h"
#include <memory>

/*! \file sg/module/Importer.cpp Defines the interface for writing
    file importers for the ospray::sg */

namespace ospray {
  namespace sg {

    struct ColorMap
    {
      ColorMap(float lo, float hi) 
        : lo(lo), hi(hi) 
      { 
        assert(lo <= hi); 

        // TODO: need a better color map here ...
        // color.push_back(vec3f(0.f,0.f,0.f));
        // color.push_back(vec3f(0.f,0.f,1.f));
        // color.push_back(vec3f(0.f,1.f,0.f));
        // color.push_back(vec3f(1.f,0.f,0.f));
        // color.push_back(vec3f(1.f,1.f,0.f));
        // color.push_back(vec3f(0.f,1.f,1.f));
        // color.push_back(vec3f(1.f,0.f,1.f));
        // color.push_back(vec3f(1.f,1.f,1.f));

        // from old qtivewre: "cool to warm"
        color.push_back(ospcommon::vec3f(0.231373  , 0.298039    , 0.752941    ));       
        color.push_back(ospcommon::vec3f(0.865003  , 0.865003    , 0.865003    ));       
        color.push_back(ospcommon::vec3f(0.705882  , 0.0156863   , 0.14902     ));
      }

      vec4f colorFor(float f)
      {
        if (f <= lo) return vec4f(color.front(),1.f);
        if (f >= hi) return vec4f(color.back(),1.f);

        float r = ((f-lo) * (color.size()-1)) / (hi-lo);
        int idx = int(r);
        if (idx < 0) idx = 0;
        if (idx >= color.size()) idx = color.size()-2;
        
        vec3f c = color[idx] + (r-idx)*(color[idx+1]-color[idx]);
        return vec4f(c,1.f);
      }

      float lo, hi;
      std::vector<vec3f> color;
    };

    bool readOne(FILE *file, float *f, int N, bool ascii)
    {
      if (!ascii)
        return fread(f,sizeof(float),N,file) == N;
      
      // ascii:
      for (int i=0;i<N;i++) {
        int rc = fscanf(file,"%f",f+i);
        if (rc == 0) return (i == 0);
        fscanf(file,"\n");
      }
      return true;
    }

    // for now, let's hardcode the importers - should be moved to a
    // registry at some point ...
    void importFileType_points(std::shared_ptr<Node> &world,
                               const FileName &url)
    {
      std::cout << "--------------------------------------------" << std::endl;
      std::cout << "#osp.sg: importer for 'points': " << url.str() << std::endl;

      FormatURL fu(url.str());
      FILE *file = fopen(fu.fileName.c_str(),"rb");
      if (!file)
        throw std::runtime_error("could not open file "+fu.fileName);

      // read the data vector
      std::shared_ptr<DataVectorT<Spheres::Sphere,OSP_RAW>> sphereData
        = std::make_shared<DataVectorT<Spheres::Sphere,OSP_RAW>>();

      float radius = .1f;
      if (fu.hasArg("radius"))
        radius = std::stof(fu["radius"]);
      if (radius == 0.f)
        throw std::runtime_error("#sg.importPoints: could not parse radius ...");
      
      std::string format = "xyz";
      if (fu.hasArg("format"))
        format = fu["format"];

      bool ascii = fu.hasArg("ascii");

      /* for now, hard-coded sphere componetns to be in float format,
         so the number of chars in the format string is the num components */
      int numFloatsPerSphere = format.size();
      size_t xPos = format.find("x");
      size_t yPos = format.find("y");
      size_t zPos = format.find("z");
      size_t rPos = format.find("r");
      size_t sPos = format.find("s");

      if (xPos == std::string::npos)
        throw std::runtime_error("invalid points format: no x component");
      if (yPos == std::string::npos)
        throw std::runtime_error("invalid points format: no y component");
      if (zPos == std::string::npos)
        throw std::runtime_error("invalid points format: no z component");

      float * const f = (float *)alloca(sizeof(float)*numFloatsPerSphere);
      box3f bounds;

      std::vector<float> mappedScalarVector;
      float mappedScalarMin = +std::numeric_limits<float>::infinity();
      float mappedScalarMax = -std::numeric_limits<float>::infinity();

      while (readOne(file,f,numFloatsPerSphere,ascii)) {
        // read one more sphere ....
        Spheres::Sphere s;
        s.position.x = f[xPos];
        s.position.y = f[yPos];
        s.position.z = f[zPos];
        s.radius
          = (rPos == std::string::npos)
          ? radius
          : f[rPos];
        sphereData->v.push_back(s);
        bounds.extend(s.position-s.radius);
        bounds.extend(s.position+s.radius);

        if (sPos != std::string::npos) {
          mappedScalarVector.push_back(f[sPos]);
          mappedScalarMin = std::min(mappedScalarMin,f[sPos]);
          mappedScalarMax = std::max(mappedScalarMax,f[sPos]);
        }
      }
      fclose(file);

      // create the node
      auto &sphereObject = world->createChild("spheres","Spheres");

      // iw - note that 'add' sounds wrong here, but that's the way
      // the current scene graph works - 'adding' that node (which
      // happens to have the right name) will essentially replace the
      // old value of that node, and thereby assign the 'data' field
      sphereData->setName("sphereData");
      sphereObject.add(sphereData); //["data"]->setValue(data);
      
      if (!mappedScalarVector.empty()) {
        std::cout << "#osp.sg: creating color map for points data ..."
                  << std::endl;
        ColorMap cm(mappedScalarMin,mappedScalarMax);
        std::shared_ptr<DataVectorT<vec4f,OSP_RAW>> colorData
          = std::make_shared<DataVectorT<vec4f,OSP_RAW>>();
        for (size_t i = 0; i < mappedScalarVector.size(); i++)
          colorData->v.push_back(cm.colorFor(mappedScalarVector[i]));
        colorData->setName("colorData");
        sphereObject.add(colorData);
      }

      std::cout << "#osp.sg: imported " << prettyNumber(sphereData->v.size()) 
                << " points, bounds = " << bounds << std::endl;;
    }

  }// ::ospray::sg
}// ::ospray


