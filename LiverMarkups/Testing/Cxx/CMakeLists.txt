set(KIT qSlicer${MODULE_NAME}Module)

#-----------------------------------------------------------------------------
set(TEMP ${Slicer_BINARY_DIR}/Testing/Temporary)
set(INPUT ${CMAKE_CURRENT_SOURCE_DIR}/../Data/Input)

#-----------------------------------------------------------------------------
set(KIT_TEST_SRCS
  vtkOpenGLBezierResectionPolyDataMapperTest1.cxx
  )

#-----------------------------------------------------------------------------
slicerMacroConfigureModuleCxxTestDriver(
  NAME ${KIT}
  SOURCES ${KIT_TEST_SRCS}
  INCLUDE_DIRECTORIES
    ${vtkAddon_INCLUDE_DIRS}
    ${MRMLCore_INCLUDE_DIRS}
  TARGET_LIBRARIES
    ${vtkAddon_LIBS}
    ${VTK_LIBRARIES}
    ${MRML_LIBRARIES}
  WITH_VTK_DEBUG_LEAKS_CHECK
  WITH_VTK_ERROR_OUTPUT_CHECK
  )

SIMPLE_TEST( vtkOpenGLBezierResectionPolyDataMapperTest1 )
