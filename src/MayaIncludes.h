#pragma once

#ifndef MAYA_INCLUDES_H
#define MAYA_INCLUDES_H

// Centralized include file for Maya API headers.
// This file is used to simplify the inclusion of common Maya headers and to prevent circular dependencies.

#include <maya/MGlobal.h>
#include <maya/MFnDagNode.h>
#include <maya/MIOStream.h>
#include <maya/MFn.h>
#include <maya/MPxNode.h>
#include <maya/MPxManipContainer.h>
#include <maya/MPxSelectionContext.h>
#include <maya/MPxContextCommand.h>
#include <maya/MModelMessage.h>
#include <maya/MGlobal.h>
#include <maya/MItSelectionList.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <maya/MDagPath.h>
#include <maya/MManipData.h>
#include <maya/MMatrix.h>
#include <maya/MItDag.h>
#include <maya/MFnMesh.h>
#include <maya/MBoundingBox.h>
#include <maya/MPointArray.h>
#include <maya/MQuaternion.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnFreePointTriadManip.h>
#include <maya/MFnDistanceManip.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnRotateManip.h>
#include <maya/MQtUtil.h>
#include <maya/MFnScaleManip.h>

#endif //MAYA_INCLUDES_H