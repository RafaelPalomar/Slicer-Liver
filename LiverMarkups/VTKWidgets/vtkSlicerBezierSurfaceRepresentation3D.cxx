/*==============================================================================

 Distributed under the OSI-approved BSD 3-Clause License.

  Copyright (c) Oslo University Hospital. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

  * Neither the name of Oslo University Hospital nor the names
    of Contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  This file was originally developed by Rafael Palomar (The Intervention Centre,
  Oslo University Hospital) and was supported by The Research Council of Norway
  through the ALive project (grant nr. 311393).

  ==============================================================================*/

#include "vtkSlicerBezierSurfaceRepresentation3D.h"

#include "vtkMRMLMarkupsBezierSurfaceNode.h"
#include "vtkMRMLMarkupsBezierSurfaceDisplayNode.h"
#include "vtkBezierSurfaceSource.h"

// MRML includes
#include <qMRMLThreeDWidget.h>
#include <vtkMRMLDisplayableManagerGroup.h>
#include <vtkMRMLModelDisplayableManager.h>

// Slicer includes
#include <qSlicerApplication.h>
#include <qSlicerLayoutManager.h>

// VTK-Addon includes
#include <vtkAddonMathUtilities.h>

// VTK includes
#include <vtkActor.h>
#include <vtkOpenGLCamera.h>
#include <vtkCollection.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkOpenGLActor.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkOpenGLPolyDataMapper.h>
#include <vtkOpenGLVertexBufferObjectGroup.h>
#include <vtkOpenGLVertexBufferObject.h>
#include <vtkPlaneSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataNormals.h>
#include <vtkPolyLine.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkShaderProperty.h>
#include <vtkTextureObject.h>
#include <vtkUniforms.h>

//------------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerBezierSurfaceRepresentation3D);

//------------------------------------------------------------------------------
vtkSlicerBezierSurfaceRepresentation3D::vtkSlicerBezierSurfaceRepresentation3D()
{
  this->BezierSurfaceSource = vtkSmartPointer<vtkBezierSurfaceSource>::New();
  this->BezierSurfaceSource->SetResolution(20,20);

  // Set the initial position of the bezier surface
  auto planeSource = vtkSmartPointer<vtkPlaneSource>::New();
  planeSource->SetResolution(3,3);
  planeSource->Update();

  this->BezierSurfaceNormals = vtkSmartPointer<vtkPolyDataNormals>::New();
  this->BezierSurfaceNormals->SetInputConnection(this->BezierSurfaceSource->GetOutputPort());

  this->BezierSurfaceControlPoints = vtkSmartPointer<vtkPoints>::New();
  this->BezierSurfaceControlPoints->SetNumberOfPoints(16);
  this->BezierSurfaceControlPoints->DeepCopy(planeSource->GetOutput()->GetPoints());;

  this->BezierSurfaceMapper = vtkSmartPointer<vtkOpenGLPolyDataMapper>::New();
  this->BezierSurfaceMapper->SetInputConnection(this->BezierSurfaceNormals->GetOutputPort());
  this->BezierSurfaceActor = vtkSmartPointer<vtkOpenGLActor>::New();
  this->BezierSurfaceActor->SetMapper(this->BezierSurfaceMapper);

  this->ControlPolygonPolyData = vtkSmartPointer<vtkPolyData>::New();
  this->ControlPolygonTubeFilter = vtkSmartPointer<vtkTubeFilter>::New();
  this->ControlPolygonTubeFilter->SetInputData(this->ControlPolygonPolyData.GetPointer());
  this->ControlPolygonTubeFilter->SetRadius(1);
  this->ControlPolygonTubeFilter->SetNumberOfSides(20);

  this->ControlPolygonMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->ControlPolygonMapper->SetInputConnection(this->ControlPolygonTubeFilter->GetOutputPort());

  this->ControlPolygonActor = vtkSmartPointer<vtkActor>::New();
  this->ControlPolygonActor->SetMapper(this->ControlPolygonMapper);

  this->DistanceMap = nullptr;
}

//------------------------------------------------------------------------------
vtkSlicerBezierSurfaceRepresentation3D::~vtkSlicerBezierSurfaceRepresentation3D() = default;

//----------------------------------------------------------------------
void vtkSlicerBezierSurfaceRepresentation3D::UpdateFromMRML(vtkMRMLNode* caller, unsigned long event, void *callData /*=nullptr*/)
{

 this->Superclass::UpdateFromMRML(caller, event, callData);

 auto liverMarkupsBezierSurfaceNode =
   vtkMRMLMarkupsBezierSurfaceNode::SafeDownCast(this->GetMarkupsNode());
 if (!liverMarkupsBezierSurfaceNode || !this->IsDisplayable())
   {
   this->VisibilityOff();
   return;
   }

 this->UpdateBezierSurface(liverMarkupsBezierSurfaceNode);
 this->UpdateControlPolygon(liverMarkupsBezierSurfaceNode);

  double diameter = ( this->MarkupsDisplayNode->GetCurveLineSizeMode() == vtkMRMLMarkupsDisplayNode::UseLineDiameter ?
    this->MarkupsDisplayNode->GetLineDiameter() : this->ControlPointSize * this->MarkupsDisplayNode->GetLineThickness() );
  this->ControlPolygonTubeFilter->SetRadius(diameter * 0.5);

  int controlPointType = Active;
  if (this->MarkupsDisplayNode->GetActiveComponentType() != vtkMRMLMarkupsDisplayNode::ComponentLine)
    {
    controlPointType = this->GetAllControlPointsSelected() ? Selected : Unselected;
    }
  this->ControlPolygonActor->SetProperty(this->GetControlPointsPipeline(controlPointType)->Property);

  // Update the distance map as 3D texture (if changed)
  auto distanceMap = liverMarkupsBezierSurfaceNode->GetDistanceMapVolumeNode();
  if ( this->DistanceMap != distanceMap)
    {
    auto renderWindow = vtkOpenGLRenderWindow::SafeDownCast(this->GetRenderer()->GetRenderWindow());
    if (renderWindow && distanceMap)
      {
      this->DistanceMapTexture = vtkSmartPointer<vtkTextureObject>::New();
      this->DistanceMapTexture->SetContext(renderWindow);
      this->DistanceMap = distanceMap;
      auto imageData = this->DistanceMap->GetImageData();
      auto dimensions = imageData->GetDimensions();
      this->DistanceMapTexture->SetWrapS(vtkTextureObject::ClampToBorder);
      this->DistanceMapTexture->SetWrapT(vtkTextureObject::ClampToBorder);
      this->DistanceMapTexture->SetWrapR(vtkTextureObject::ClampToBorder);
      this->DistanceMapTexture->SetMinificationFilter(vtkTextureObject::Linear);
      this->DistanceMapTexture->SetMagnificationFilter(
          vtkTextureObject::Linear);
      this->DistanceMapTexture->SetBorderColor(1000.0f, 0.0f, 0.0f, 0.0f);
      this->DistanceMapTexture->Create3DFromRaw(dimensions[0], dimensions[1],
                                                dimensions[2], 1, VTK_FLOAT,
                                                imageData->GetScalarPointer());
    }
    this->DistanceMap = distanceMap;
    }

  auto mapper = vtkOpenGLPolyDataMapper::SafeDownCast(this->BezierSurfaceActor->GetMapper());
  if (mapper) {
    auto VBOs = mapper->GetVBOs();
    if (VBOs) {
      auto vertexVBO = VBOs->GetVBO("vertexMC");
      if (vertexVBO) {
        auto shift = vertexVBO->GetShift();
        auto scale = vertexVBO->GetScale();

        this->VBOInverseTransform->Identity();
        if (shift.size() == 3 && scale.size() == 3)
          {
          this->VBOInverseTransform->Translate(shift[0], shift[1], shift[2]);
          this->VBOInverseTransform->Scale(1.0/scale[0], 1.0/scale[1], 1.0/scale[2]);
          }

        this->VBOInverseTransform->GetTranspose(this->VBOShiftScale);
      }
    }
  }


  auto shaderProperty = this->BezierSurfaceActor->GetShaderProperty();
  if (this->ShaderProperty != shaderProperty)
    {
    shaderProperty->AddVertexShaderReplacement("//VTK::PositionVC::Dec", true,
                                               "//VTK::PositionVC::Dec\n"
                                               "out vec4 vertexMCVSOutput;\n"
                                               "out vec4 vertexWCVSOutput;\n",
                                               false);

    shaderProperty->AddVertexShaderReplacement(
        "//VTK::PositionVC::Impl", true,
        "//VTK::PositionVC::Impl\n"
        "vertexMCVSOutput = vertexMC;\n"
        "vertexWCVSOutput = ijkToTexture*rasToIjk*shiftScale*vertexMC;\n",
        false);

    shaderProperty->AddFragmentShaderReplacement(
        "//VTK::PositionVC::Dec", true,
        "//VTK::PositionVC::Dec\n"
        "in vec4 vertexMCVSOutput;\n"
        "in vec4 vertexWCVSOutput;\n"
        "vec4 fragPositionMC = vertexWCVSOutput;\n",
        false);

    shaderProperty->AddFragmentShaderReplacement(
        "//VTK::Color::Dec", true,
        "//VTK::Color::Dec\n"
        "uniform sampler3D distanceTexture;\n",
        false);

    shaderProperty->AddFragmentShaderReplacement(
        "//VTK::Color::Impl", true,
        "//VTK::Color::Impl\n"
        "float dist = texture(distanceTexture, fragPositionMC.xyz).r;\n"
        "if(clipOut == 1 && dist > 200.0){\n"
        "  discard;\n"
        "}\n"
        "if(dist<margin-uncertainty){\n"
        "   ambientColor = vec3(1.0, 0.0, 0.0);\n"
        "   diffuseColor = vec3(0.0, 0.0, 0.0);\n"
        "}\n"
        "else if(dist<margin+uncertainty){\n"
        "   ambientColor = vec3(1.0, 1.0, 0.0);\n"
        "   diffuseColor = vec3(0.0, 0.0, 0.0);\n"
        "}\n"
        "else{\n"
        "  ambientColor = vec3(0.2, 0.2 ,0.2);\n"
        "}\n",
        false);
    this->ShaderProperty = shaderProperty;
  }

  auto rasToIjk = vtkSmartPointer<vtkMatrix4x4>::New();
  auto ijkToTexture = vtkSmartPointer<vtkMatrix4x4>::New();

  if (distanceMap)
    {
    auto imageData = distanceMap->GetImageData();
    auto dimensions = imageData->GetDimensions();

    distanceMap->GetRASToIJKMatrix(rasToIjk);
    rasToIjk->Transpose();
    this->rasToIjk = rasToIjk;

    auto scaling = vtkSmartPointer<vtkTransform>::New();
    scaling->Scale(1.0/dimensions[0], 1.0/dimensions[1], 1.0/dimensions[2]);
    scaling->GetTranspose(ijkToTexture);
    }

  // this->BezierSurfaceActor->GetKeyMatrices(mcwc, anorms);
  auto vertexUniforms= shaderProperty->GetVertexCustomUniforms();
  vertexUniforms->SetUniformMatrix("shiftScale", this->VBOShiftScale);
  vertexUniforms->SetUniformMatrix("rasToIjk", rasToIjk);
  vertexUniforms->SetUniformMatrix("ijkToTexture", ijkToTexture);

  auto liverMarkupsBezierSurfaceDisplayNode =
    vtkMRMLMarkupsBezierSurfaceDisplayNode::SafeDownCast(liverMarkupsBezierSurfaceNode->GetDisplayNode());

  auto fragmentUniforms = shaderProperty->GetFragmentCustomUniforms();
  fragmentUniforms->SetUniformf("margin", static_cast<float>(liverMarkupsBezierSurfaceNode->GetDistanceMargin()));
  fragmentUniforms->SetUniformf("uncertainty", static_cast<float>(liverMarkupsBezierSurfaceNode->GetUncertaintyMargin()));
  if (!liverMarkupsBezierSurfaceDisplayNode)
  {
    fragmentUniforms->SetUniformi("clipOut", 0);
  }
  else
  {
    fragmentUniforms->SetUniformi("clipOut", liverMarkupsBezierSurfaceDisplayNode->GetClipOut());
  }

  this->NeedToRenderOn();
}

//----------------------------------------------------------------------
void vtkSlicerBezierSurfaceRepresentation3D::GetActors(vtkPropCollection *pc)
{
  this->Superclass::GetActors(pc);
  this->BezierSurfaceActor->GetActors(pc);
  this->ControlPolygonActor->GetActors(pc);
}

//----------------------------------------------------------------------
void vtkSlicerBezierSurfaceRepresentation3D::ReleaseGraphicsResources(
  vtkWindow *win)
{
  this->Superclass::ReleaseGraphicsResources(win);
  this->BezierSurfaceActor->ReleaseGraphicsResources(win);
  this->ControlPolygonActor->ReleaseGraphicsResources(win);
}

//----------------------------------------------------------------------
int vtkSlicerBezierSurfaceRepresentation3D::RenderOverlay(vtkViewport *viewport)
{
  int count=0;
  count = this->Superclass::RenderOverlay(viewport);
  if (this->BezierSurfaceActor->GetVisibility())
    {
    count +=  this->BezierSurfaceActor->RenderOverlay(viewport);
    count +=  this->ControlPolygonActor->RenderOverlay(viewport);
    }
  return count;
}

//-----------------------------------------------------------------------------
int vtkSlicerBezierSurfaceRepresentation3D::RenderOpaqueGeometry(
  vtkViewport *viewport)
{
  int count=0;
  count = this->Superclass::RenderOpaqueGeometry(viewport);
  if (this->BezierSurfaceActor->GetVisibility())
    {
    count += this->BezierSurfaceActor->RenderOpaqueGeometry(viewport);
    }
  if (this->ControlPolygonActor->GetVisibility())
    {
    double diameter = ( this->MarkupsDisplayNode->GetCurveLineSizeMode() == vtkMRMLMarkupsDisplayNode::UseLineDiameter ?
                        this->MarkupsDisplayNode->GetLineDiameter() : this->ControlPointSize * this->MarkupsDisplayNode->GetLineThickness() );
    this->ControlPolygonTubeFilter->SetRadius(diameter * 0.5);
    count += this->ControlPolygonActor->RenderOpaqueGeometry(viewport);
    }
  return count;
}

//-----------------------------------------------------------------------------
int vtkSlicerBezierSurfaceRepresentation3D::RenderTranslucentPolygonalGeometry(
  vtkViewport *viewport)
{
  int count=0;
  count = this->Superclass::RenderTranslucentPolygonalGeometry(viewport);
  if (this->BezierSurfaceActor->GetVisibility())
    {
    // The internal actor needs to share property keys.
    // This ensures the mapper state is consistent and allows depth peeling to work as expected.
    this->BezierSurfaceActor->SetPropertyKeys(this->GetPropertyKeys());
    count += this->BezierSurfaceActor->RenderTranslucentPolygonalGeometry(viewport);
    }
  if (this->ControlPolygonActor->GetVisibility())
    {
    // The internal actor needs to share property keys.
    // This ensures the mapper state is consistent and allows depth peeling to work as expected.
    this->ControlPolygonActor->SetPropertyKeys(this->GetPropertyKeys());
    count += this->ControlPolygonActor->RenderTranslucentPolygonalGeometry(viewport);
    }
  return count;
}

//-----------------------------------------------------------------------------
vtkTypeBool vtkSlicerBezierSurfaceRepresentation3D::HasTranslucentPolygonalGeometry()
{
  if (this->Superclass::HasTranslucentPolygonalGeometry())
    {
    return true;
    }
  if (this->BezierSurfaceActor->GetVisibility() && this->BezierSurfaceActor->HasTranslucentPolygonalGeometry())
    {
    return true;
    }
  if (this->ControlPolygonActor->GetVisibility() && this->ControlPolygonActor->HasTranslucentPolygonalGeometry())
    {
    return true;
    }
  return false;
}

//----------------------------------------------------------------------
double *vtkSlicerBezierSurfaceRepresentation3D::GetBounds()
{
  vtkBoundingBox boundingBox;
  const std::vector<vtkProp*> actors({ this->BezierSurfaceActor, this->ControlPolygonActor });
  this->AddActorsBounds(boundingBox, actors, Superclass::GetBounds());
  boundingBox.GetBounds(this->Bounds);
  return this->Bounds;
}


//----------------------------------------------------------------------
// void vtkSlicerBezierSurfaceRepresentation3D::CanInteract(
//   vtkMRMLInteractionEventData* interactionEventData,
//   int &foundComponentType, int &foundComponentIndex, double &closestDistance2)
// {
//   foundComponentType = vtkMRMLMarkupsDisplayNode::ComponentNone;
//   vtkMRMLMarkupsNode* markupsNode = this->GetMarkupsNode();
//   if ( !markupsNode || markupsNode->GetLocked() || markupsNode->GetNumberOfDefinedControlPoints(true) < 1
//     || !interactionEventData )
//     {
//     return;
//     }
//   Superclass::CanInteract(interactionEventData, foundComponentType, foundComponentIndex, closestDistance2);
//   if (foundComponentType != vtkMRMLMarkupsDisplayNode::ComponentNone)
//     {
//     // if mouse is near a control point then select that (ignore the line)
//     return;
//     }

//   this->CanInteractWithBezierSurface(interactionEventData, foundComponentType, foundComponentIndex, closestDistance2);
// }

//-----------------------------------------------------------------------------
void vtkSlicerBezierSurfaceRepresentation3D::PrintSelf(ostream& os, vtkIndent indent)
{
  //Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
  this->Superclass::PrintSelf(os, indent);

  if (this->BezierSurfaceActor)
    {
    os << indent << "BezierSurface Visibility: " << this->BezierSurfaceActor->GetVisibility() << "\n";
    }
  else
    {
    os << indent << "BezierSurface Visibility: (none)\n";
    }

  if (this->ControlPolygonActor)
    {
    os << indent << "ControlPolygon Visibility: " << this->ControlPolygonActor->GetVisibility() << "\n";
    }
  else
    {
    os << indent << "ControlPolygon Visibility: (none)\n";
    }
}

//-----------------------------------------------------------------------------
// void vtkSlicerBezierSurfaceRepresentation3D::UpdateInteractionPipeline()
// {
//   if (!this->MarkupsNode || this->MarkupsNode->GetNumberOfDefinedControlPoints(true) < 16)
//     {
//     this->InteractionPipeline->Actor->SetVisibility(false);
//     return;
//     }
//   // Final visibility handled by superclass in vtkSlicerMarkupsWidgetRepresentation
//   Superclass::UpdateInteractionPipeline();
// }

//-----------------------------------------------------------------------------
void vtkSlicerBezierSurfaceRepresentation3D::UpdateBezierSurface(vtkMRMLMarkupsBezierSurfaceNode *node)
{
  if (!node)
    {
    return;
    }

  if (node->GetNumberOfControlPoints() == 16)
    {
    for (int i=0; i<16; i++)
      {
      double point[3];
      node->GetNthControlPointPosition(i,point);
      this->BezierSurfaceControlPoints->SetPoint(i,
                                                 static_cast<float>(point[0]),
                                                 static_cast<float>(point[1]),
                                                 static_cast<float>(point[2]));
      }

    this->BezierSurfaceSource->SetControlPoints(this->BezierSurfaceControlPoints);
    }
}

//-----------------------------------------------------------------------------
void vtkSlicerBezierSurfaceRepresentation3D::UpdateControlPolygon(vtkMRMLMarkupsBezierSurfaceNode *node)
{
  if (node->GetNumberOfControlPoints() == 16)
    {
    //Generate topology;
    vtkSmartPointer<vtkCellArray> planeCells =
      vtkSmartPointer<vtkCellArray>::New();
    for(int i=0; i<3; ++i)
      {
      for(int j=0; j<3; ++j)
        {
        vtkSmartPointer<vtkPolyLine> polyLine = vtkSmartPointer<vtkPolyLine>::New();
        polyLine->GetPointIds()->SetNumberOfIds(5);
        polyLine->GetPointIds()->SetId(0,i*4+j);
        polyLine->GetPointIds()->SetId(1,i*4+j+1);
        polyLine->GetPointIds()->SetId(2,(i+1)*4+j+1);
        polyLine->GetPointIds()->SetId(3,(i+1)*4+j);
        polyLine->GetPointIds()->SetId(4,i*4+j);
        planeCells->InsertNextCell(polyLine);
        }
      }

    this->ControlPolygonPolyData->SetPoints(this->BezierSurfaceControlPoints);
    this->ControlPolygonPolyData->SetLines(planeCells);
    }
}
