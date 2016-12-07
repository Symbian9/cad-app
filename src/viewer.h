
#pragma once

#include <iostream>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <nanogui/nanogui.h>
#include <nanogui/opengl.h>

#include "mesh.h"
#include "shader.h"
#include "control.h"
#include "ray.h"
#include "deform.h"

class Viewer {
public:
  void init();
  void initCallbacks();
  void launch();
  void drawMesh();
};

