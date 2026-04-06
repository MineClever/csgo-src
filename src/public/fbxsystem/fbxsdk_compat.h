//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
// Minimal compatibility entrypoint for FBX backends.
//
//=============================================================================

#ifndef FBXSDK_COMPAT_H
#define FBXSDK_COMPAT_H
#pragma once

#if defined( VALVE_FBX_BACKEND_OPENFBX )
struct FbxManager;
struct FbxScene;
struct FbxNode;
struct FbxMesh;
struct FbxMatrix;
struct FbxFileTexture;
struct FbxSurfaceMaterial;
struct FbxAnimLayer;
struct FbxGeometryElementVertexColor;
struct FbxGeometryElementUV;
namespace FbxTime
{
	enum EMode
	{
		eFrames30 = 0
	};
}
#else
#include <fbxsdk.h>
#endif

#endif // FBXSDK_COMPAT_H
