#pragma once

#include <maya/MPxNode.h>
#include <maya/MDagPath.h>
#include <maya/MFnMesh.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MDoubleArray.h>
#include <maya/MRampAttribute.h>
#include <maya/MFnMeshData.h>
#include <maya/MPlugArray.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItMeshEdge.h>

// Macros
#define MCheckStatus(status,message)        \
        if( MStatus::kSuccess != status ) { \
                cerr << message << "\n";    \
                return status;              \
        }

class tensionNode : public MPxNode
{
public:
    tensionNode() {}
    virtual ~tensionNode() {}
    virtual void postConstructor();
    virtual MStatus compute( const MPlug& plug, MDataBlock& data );
    virtual MStatus setDependentsDirty( const MPlug& dirtyPlug, MPlugArray& affectedPlugs );
    static void* creator(){ return new tensionNode(); }
    static MStatus initialize();
    static MDoubleArray getEdgeLen( const MDataHandle& inMesh );

    static MTypeId id;
    static MObject aOrigShape;
    static MObject aDeformedShape;
    static MObject aOutShape;
    static MObject aColorRamp;
    static MObject aColorSetName;

private:
    bool isOrigDirty = true;
    bool isDeformedDirty = true;
    MDoubleArray origEdgeLenArray;
    MDoubleArray deformedEdgeLenArray;

};

