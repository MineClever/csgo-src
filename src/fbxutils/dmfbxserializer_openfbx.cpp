//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
// OpenFBX-backed implementation of the legacy FBX->DMX serializer interface.
// This intentionally keeps the public surface stable while providing a
// geometry-focused fallback backend when Autodesk FBX SDK is unavailable.
//
//=============================================================================

#include "filesystem.h"
#include "fbxutils/dmfbxserializer.h"
#include "icommandline.h"
#include "movieobjects/dmedccmakefile.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmeexporttags.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmetransform.h"
#include "tier1/fmtstr.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlstring.h"

#include "ofbx.h"

#include "tier0/memdbgon.h"

namespace
{
	void OFBXDataViewToString( CUtlString &out, ofbx::DataView value )
	{
		const int length = value.begin && value.end ? static_cast< int >( value.end - value.begin ) : 0;
		if ( length <= 0 )
		{
			out = "";
			return;
		}

		char *buffer = reinterpret_cast< char * >( stackalloc( length + 1 ) );
		V_memcpy( buffer, value.begin, length );
		buffer[length] = '\0';
		out = buffer;
	}

	void OFBXMatrixMul( const ofbx::DMatrix &a, const ofbx::DMatrix &b, ofbx::DMatrix &out )
	{
		for ( int row = 0; row < 4; ++row )
		{
			for ( int col = 0; col < 4; ++col )
			{
				double sum = 0.0;
				for ( int k = 0; k < 4; ++k )
				{
					sum += a.m[row * 4 + k] * b.m[k * 4 + col];
				}
				out.m[row * 4 + col] = sum;
			}
		}
	}

	Vector OFBXTransformPoint( const ofbx::DMatrix &m, const ofbx::Vec3 &v, float flScale )
	{
		const double x = v.x * m.m[0] + v.y * m.m[1] + v.z * m.m[2] + m.m[3];
		const double y = v.x * m.m[4] + v.y * m.m[5] + v.z * m.m[6] + m.m[7];
		const double z = v.x * m.m[8] + v.y * m.m[9] + v.z * m.m[10] + m.m[11];
		return Vector( x * flScale, y * flScale, z * flScale );
	}

	Vector OFBXTransformNormal( const ofbx::DMatrix &m, const ofbx::Vec3 &v )
	{
		Vector normal(
			v.x * m.m[0] + v.y * m.m[1] + v.z * m.m[2],
			v.x * m.m[4] + v.y * m.m[5] + v.z * m.m[6],
			v.x * m.m[8] + v.y * m.m[9] + v.z * m.m[10] );
		VectorNormalize( normal );
		return normal;
	}

	CDmeAxisSystem::Axis_t OFBXUpAxisToDmeAxis( const ofbx::GlobalSettings &settings )
	{
		const int sign = settings.UpAxisSign >= 0 ? 1 : -1;
		switch ( settings.UpAxis )
		{
		case ofbx::UpVector_AxisX: return sign > 0 ? CDmeAxisSystem::AS_AXIS_X : CDmeAxisSystem::AS_AXIS_NX;
		case ofbx::UpVector_AxisZ: return sign > 0 ? CDmeAxisSystem::AS_AXIS_Z : CDmeAxisSystem::AS_AXIS_NZ;
		case ofbx::UpVector_AxisY:
		default:
			return sign > 0 ? CDmeAxisSystem::AS_AXIS_Y : CDmeAxisSystem::AS_AXIS_NY;
		}
	}

	CDmeAxisSystem::ForwardParity_t OFBXFrontParityToDme( const ofbx::GlobalSettings &settings )
	{
		int front = settings.FrontAxis;
		if ( front == 0 )
		{
			front = 1;
		}

		const int sign = settings.FrontAxisSign >= 0 ? 1 : -1;
		const int parity = sign * front;
		switch ( parity )
		{
		case -2: return CDmeAxisSystem::AS_PARITY_NODD;
		case -1: return CDmeAxisSystem::AS_PARITY_NEVEN;
		case 1: return CDmeAxisSystem::AS_PARITY_EVEN;
		case 2:
		default:
			return CDmeAxisSystem::AS_PARITY_ODD;
		}
	}

	bool OFBXBuildMaterialPath( CUtlString &outPath, const ofbx::Material *pMaterial )
	{
		if ( !pMaterial )
			return false;

		const ofbx::Texture *pTexture = pMaterial->getTexture( ofbx::Texture::DIFFUSE );
		CUtlString textureName;
		if ( pTexture )
		{
			OFBXDataViewToString( textureName, pTexture->getRelativeFileName() );
			if ( textureName.IsEmpty() )
			{
				OFBXDataViewToString( textureName, pTexture->getFileName() );
			}
		}

		if ( !textureName.IsEmpty() )
		{
			char materialName[MAX_PATH] = {};
			V_FileBase( textureName.String(), materialName, ARRAYSIZE( materialName ) );
			outPath = materialName;
			return !outPath.IsEmpty();
		}

		if ( pMaterial->name[0] )
		{
			outPath = pMaterial->name;
			return true;
		}

		return false;
	}

	void AddOpenFbxMeshToDmx( CDmFbxSerializer *pSerializer, DmFileId_t nDmFileId, CDmeModel *pDmeModel, const ofbx::Mesh *pMesh )
	{
		const ofbx::GeometryData &geom = pMesh->getGeometryData();
		if ( !geom.hasVertices() )
			return;

		CUtlString meshName = pMesh->name[0] ? pMesh->name : "fbx_mesh";
		CDmeDag *pDmeDag = CreateElement< CDmeDag >( meshName.String(), nDmFileId );
		pDmeDag->SetParent( pDmeModel );
		pDmeModel->AddJoint( pDmeDag );

		if ( CDmeTransform *pTransform = pDmeDag->GetTransform() )
		{
			pTransform->SetPosition( vec3_origin );
			pTransform->SetOrientation( quat_identity );
		}

		CDmeMesh *pDmeMesh = CreateElement< CDmeMesh >( meshName.String(), nDmFileId );
		CDmeVertexData *pDmeVertexData = pDmeMesh->FindOrCreateBaseState( "bind" );
		pDmeVertexData->FlipVCoordinate( true );
		pDmeMesh->SetBindBaseState( pDmeVertexData );
		pDmeMesh->SetCurrentBaseState( "bind" );

		const FieldIndex_t positionField = pDmeVertexData->CreateField( CDmeVertexData::FIELD_POSITION );
		const FieldIndex_t normalField = pDmeVertexData->CreateField( CDmeVertexData::FIELD_NORMAL );
		const ofbx::Vec2Attributes uvs = geom.getUVs();
		const bool hasUVs = uvs.values != NULL && uvs.count > 0;
		const FieldIndex_t uvField = hasUVs ? pDmeVertexData->CreateField( CDmeVertexData::FIELD_TEXCOORD ) : -1;

		CUtlVector< Vector > positions;
		CUtlVector< Vector > normals;
		CUtlVector< Vector2D > texcoords;
		CUtlVector< int > indices;

		const float flScale = FloatsAreEqual( pSerializer->m_flOptScale, 0.0f, 1.0e-6f ) ? 1.0f : ( 1.0f / pSerializer->m_flOptScale );
		ofbx::DMatrix objectMatrix = pMesh->getGlobalTransform();
		ofbx::DMatrix geometryMatrix = pMesh->getGeometricMatrix();
		ofbx::DMatrix transformMatrix;
		OFBXMatrixMul( objectMatrix, geometryMatrix, transformMatrix );

		const ofbx::Vec3Attributes meshPositions = geom.getPositions();
		const ofbx::Vec3Attributes meshNormals = geom.getNormals();
		const bool hasNormals = meshNormals.values != NULL && meshNormals.count > 0;

		for ( int partitionIndex = 0; partitionIndex < geom.getPartitionCount(); ++partitionIndex )
		{
			const ofbx::GeometryPartition partition = geom.getPartition( partitionIndex );

			CDmeFaceSet *pFaceSet = CreateElement< CDmeFaceSet >( CFmtStr( "%s_faceset_%d", meshName.String(), partitionIndex ).Access(), nDmFileId );
			CDmeMaterial *pMaterial = CreateElement< CDmeMaterial >( CFmtStr( "%s_material_%d", meshName.String(), partitionIndex ).Access(), nDmFileId );

			CUtlString materialPath;
			if ( !OFBXBuildMaterialPath( materialPath, pMesh->getMaterial( partitionIndex ) ) )
			{
				materialPath = pMaterial->GetName();
			}
			pMaterial->SetMaterial( materialPath.String() );
			pFaceSet->SetMaterial( pMaterial );
			pDmeMesh->AddFaceSet( pFaceSet );

			for ( int polygonIndex = 0; polygonIndex < partition.polygon_count; ++polygonIndex )
			{
				const ofbx::GeometryPartition::Polygon &polygon = partition.polygons[polygonIndex];
				const int faceStart = indices.Count();

				Vector fallbackNormal( 0.0f, 0.0f, 1.0f );
				if ( !hasNormals && polygon.vertex_count >= 3 )
				{
					const Vector a = OFBXTransformPoint( transformMatrix, meshPositions.get( polygon.from_vertex + 0 ), flScale );
					const Vector b = OFBXTransformPoint( transformMatrix, meshPositions.get( polygon.from_vertex + 1 ), flScale );
					const Vector c = OFBXTransformPoint( transformMatrix, meshPositions.get( polygon.from_vertex + 2 ), flScale );
					CrossProduct( b - a, c - a, fallbackNormal );
					VectorNormalize( fallbackNormal );
				}

				for ( int vertexOffset = 0; vertexOffset < polygon.vertex_count; ++vertexOffset )
				{
					const int srcIndex = polygon.from_vertex + vertexOffset;
					const int vertexIndex = positions.AddToTail();

					positions[vertexIndex] = OFBXTransformPoint( transformMatrix, meshPositions.get( srcIndex ), flScale );
					normals.AddToTail( hasNormals ? OFBXTransformNormal( transformMatrix, meshNormals.get( srcIndex ) ) : fallbackNormal );
					if ( hasUVs )
					{
						const ofbx::Vec2 uv = uvs.get( srcIndex );
						texcoords.AddToTail( Vector2D( uv.x, uv.y ) );
					}
					indices.AddToTail( vertexIndex );
				}

				indices.AddToTail( -1 );
				pFaceSet->AddIndices( indices.Count() - faceStart );
				pFaceSet->SetIndices( pFaceSet->NumIndices() - ( indices.Count() - faceStart ), indices.Count() - faceStart, indices.Base() + faceStart );
			}
		}

		pDmeVertexData->AddVertexData( positionField, positions.Count() );
		pDmeVertexData->SetVertexData( positionField, 0, positions.Count(), AT_VECTOR3, positions.Base() );
		pDmeVertexData->AddVertexIndices( positions.Count() );
		CUtlVector< int > vertexIndices;
		vertexIndices.SetCount( positions.Count() );
		for ( int i = 0; i < positions.Count(); ++i )
		{
			vertexIndices[i] = i;
		}
		pDmeVertexData->SetVertexIndices( positionField, 0, vertexIndices.Count(), vertexIndices.Base() );

		pDmeVertexData->AddVertexData( normalField, normals.Count() );
		pDmeVertexData->SetVertexData( normalField, 0, normals.Count(), AT_VECTOR3, normals.Base() );
		pDmeVertexData->SetVertexIndices( normalField, 0, vertexIndices.Count(), vertexIndices.Base() );

		if ( hasUVs )
		{
			pDmeVertexData->AddVertexData( uvField, texcoords.Count() );
			pDmeVertexData->SetVertexData( uvField, 0, texcoords.Count(), AT_VECTOR2, texcoords.Base() );
			pDmeVertexData->SetVertexIndices( uvField, 0, vertexIndices.Count(), vertexIndices.Base() );
		}

		pDmeDag->SetShape( pDmeMesh );

		if ( hasUVs )
		{
			pDmeMesh->ComputeDefaultTangentData( false );
		}
	}
}

CDmFbxSerializer::CDmFbxSerializer()
: m_nOptVerbosity( 0 )
, m_bOptUnderscoreForCorrectors( false )
, m_bAnimation( false )
, m_bReturnDmeModel( false )
, m_flOptScale( 1.0f )
{
	CDmeAxisSystem::GetPredefinedAxisSystem( m_eOptUpAxis, m_eOptForwardParity, m_eCoordSys, CDmeAxisSystem::AS_MAYA_YUP );
}

CDmFbxSerializer::~CDmFbxSerializer()
{
}

CDmElement *CDmFbxSerializer::ReadFBX( const char *pszFilename )
{
	DmFileId_t nDmFileId = g_pDataModel->FindOrCreateFileId( pszFilename );
	if ( nDmFileId == DMFILEID_INVALID )
	{
		Warning( "Warning! Couldn't create DmFileId_t for \"%s\"\n", pszFilename );
		return NULL;
	}

	CUtlBuffer fileBuffer;
	if ( !g_pFullFileSystem->ReadFile( pszFilename, NULL, fileBuffer ) )
	{
		Warning( "Warning! Couldn't read FBX file \"%s\"\n", pszFilename );
		return NULL;
	}

	const ofbx::u16 flags =
		static_cast< ofbx::u16 >(
			ofbx::LoadFlags::IGNORE_BLEND_SHAPES |
			ofbx::LoadFlags::IGNORE_CAMERAS |
			ofbx::LoadFlags::IGNORE_LIGHTS |
			ofbx::LoadFlags::IGNORE_POSES );

	ofbx::IScene *pScene = ofbx::load( reinterpret_cast< const ofbx::u8 * >( fileBuffer.Base() ), fileBuffer.TellPut(), flags );
	if ( !pScene )
	{
		Warning( "Warning! OpenFBX failed to load \"%s\": %s\n", pszFilename, ofbx::getError() ? ofbx::getError() : "unknown error" );
		return NULL;
	}

	char szFileBase[MAX_PATH] = {};
	V_FileBase( pszFilename, szFileBase, ARRAYSIZE( szFileBase ) );

	CDmElement *pDmeRoot = NULL;
	CDmeModel *pDmeModel = CreateElement< CDmeModel >( szFileBase, nDmFileId );
	if ( !m_bAnimation && m_bReturnDmeModel )
	{
		pDmeRoot = pDmeModel;
	}
	else
	{
		pDmeRoot = CreateElement< CDmElement >( "root", nDmFileId );
		pDmeRoot->SetValue( "skeleton", pDmeModel );
		pDmeRoot->SetValue( "model", pDmeModel );
	}

	g_pDataModel->SetFileRoot( nDmFileId, pDmeRoot->GetHandle() );

	CDmeDCCMakefile *pDmeMakefile = CreateElement< CDmeDCCMakefile >( "makefile", nDmFileId );
	pDmeRoot->SetValue( pDmeMakefile->GetName(), pDmeMakefile );
	pDmeMakefile->AddSource< CDmeSource >( pszFilename );

	CDmeExportTags *pDmeExportTags = CreateElement< CDmeExportTags >( "exportTags", nDmFileId );
	pDmeExportTags->Init( "fbx2dmx", "OpenFBX" );
	pDmeExportTags->SetValue( "cmdLine", CommandLine()->GetCmdLine() );
	pDmeRoot->SetValue( pDmeExportTags->GetName(), pDmeExportTags );

	CDmAttribute *pRootAttr = pDmeModel->AddAttribute( "__rootElement", AT_ELEMENT );
	if ( pRootAttr )
	{
		pRootAttr->AddFlag( FATTRIB_DONTSAVE );
		pRootAttr->SetValue( pDmeRoot );
	}

	if ( const ofbx::GlobalSettings *pSettings = pScene->getGlobalSettings() )
	{
		pDmeModel->SetAxisSystem(
			OFBXUpAxisToDmeAxis( *pSettings ),
			OFBXFrontParityToDme( *pSettings ),
			pSettings->CoordAxis == ofbx::CoordSystem_LeftHanded ? CDmeAxisSystem::AS_LEFT_HANDED : CDmeAxisSystem::AS_RIGHT_HANDED );
	}
	else
	{
		pDmeModel->SetAxisSystem( CDmeAxisSystem::AS_MAYA_YUP );
	}

	if ( m_bAnimation )
	{
		AddConversionError( nDmFileId, "OpenFBX backend currently imports geometry only; animation data was ignored." );
	}

	for ( int i = 0; i < pScene->getMeshCount(); ++i )
	{
		const ofbx::Mesh *pMesh = pScene->getMesh( i );
		if ( !pMesh )
			continue;

		if ( pMesh->getSkin() )
		{
			AddConversionError( nDmFileId, CFmtStr( "OpenFBX backend does not yet import skinning for mesh \"%s\".", pMesh->name ).Access() );
		}
		if ( pMesh->getBlendShape() )
		{
			AddConversionError( nDmFileId, CFmtStr( "OpenFBX backend does not yet import blend shapes for mesh \"%s\".", pMesh->name ).Access() );
		}

		AddOpenFbxMeshToDmx( this, nDmFileId, pDmeModel, pMesh );
	}

	pDmeModel->CaptureJointsToBaseState( "bind" );
	pDmeModel->ConvertToAxisSystem( CDmeAxisSystem::AS_VALVE_ENGINE );

	pScene->destroy();
	return pDmeRoot;
}

bool CDmFbxSerializer::HasConversionErrors( CDmElement *pDmRoot )
{
	if ( !pDmRoot )
		return false;

	CDmAttribute *pConversionErrorsAttr = pDmRoot->GetAttribute( "conversionErrors", AT_STRING_ARRAY );
	return pConversionErrorsAttr && CDmrStringArrayConst( pConversionErrorsAttr ).Count() > 0;
}

void CDmFbxSerializer::GetConversionErrors( CDmElement *pDmRoot, CUtlVector< CUtlString > *pConversionErrors )
{
	if ( !pDmRoot || !pConversionErrors || !HasConversionErrors( pDmRoot ) )
		return;

	CDmAttribute *pConversionErrorsAttr = pDmRoot->GetAttribute( "conversionErrors", AT_STRING_ARRAY );
	if ( !pConversionErrorsAttr )
		return;

	CDmrStringArrayConst conversionErrors( pConversionErrorsAttr );
	for ( int i = 0; i < conversionErrors.Count(); ++i )
	{
		pConversionErrors->AddToTail( conversionErrors[i] );
	}
}

FbxManager *CDmFbxSerializer::GetFbxManager()
{
	return NULL;
}

void CDmFbxSerializer::AddConversionError( DmFileId_t nDmFileId, const char *pszErrorMsg )
{
	if ( !pszErrorMsg )
		return;

	CDmElement *pDmRoot = g_pDataModel->GetElement( g_pDataModel->GetFileRoot( nDmFileId ) );
	if ( !pDmRoot )
		return;

	CDmAttribute *pConversionErrorsAttr = pDmRoot->AddAttribute( "conversionErrors", AT_STRING_ARRAY );
	if ( !pConversionErrorsAttr )
		return;

	CDmrStringArray conversionErrors( pConversionErrorsAttr );
	for ( int i = 0; i < conversionErrors.Count(); ++i )
	{
		if ( !V_stricmp( conversionErrors[i], pszErrorMsg ) )
			return;
	}

	conversionErrors.AddToTail( pszErrorMsg );
	Warning( "%s\n", pszErrorMsg );
}
