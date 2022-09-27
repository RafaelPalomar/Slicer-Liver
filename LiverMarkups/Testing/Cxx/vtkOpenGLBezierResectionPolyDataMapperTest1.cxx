
// This module includes
#include "vtkOpenGLBezierResectionPolyDataMapper.h"

// VTK includes
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>

// STD includes
#include <cstdlib>

int vtkOpenGLBezierResectionPolyDataMapperTest1(int argc, char** argv)
{
  auto sphereSource = vtkSmartPointer<vtkSphereSource>::New();
  sphereSource->SetThetaResolution(100);
  sphereSource->SetPhiResolution(100);



  return EXIT_SUCCESS;
}
