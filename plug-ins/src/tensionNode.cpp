#include "tensionNode.h"
#include <maya/MGlobal.h>
#include <maya/MFnStringData.h>

const MString origAttrName( "origShape" );
const MString deformedAttrName( "deformedShape" );

MTypeId tensionNode::id( 0x00001 );
MObject tensionNode::aOrigShape;
MObject tensionNode::aDeformedShape;
MObject tensionNode::aOutShape;
MObject tensionNode::aColorRamp;
MObject tensionNode::aColorSetName;

MStatus initialize_ramp( MObject parentNode, MObject rampObj, int index, float position, MColor value, int interpolation )
// initialize color ramp values
{
    MStatus status;

    MPlug rampPlug( parentNode, rampObj );

    MPlug elementPlug = rampPlug.elementByLogicalIndex( index, &status );

    MPlug positionPlug = elementPlug.child(0, &status);
    status = positionPlug.setFloat(position);

    MPlug valuePlug = elementPlug.child(1);
    status = valuePlug.child(0).setFloat(value.r);
    status = valuePlug.child(1).setFloat(value.g);
    status = valuePlug.child(2).setFloat(value.b);

    MPlug interpPlug = elementPlug.child(2);
    interpPlug.setInt(interpolation);

    return MS::kSuccess;
}

void tensionNode::postConstructor()
{
    MStatus status;
    initialize_ramp( tensionNode::thisMObject(), aColorRamp, 0, 0.0f, MColor( 0, 1, 0 ), 1 );
    initialize_ramp( tensionNode::thisMObject(), aColorRamp, 1, 0.5f, MColor( 0, 0, 0 ), 1 );
    initialize_ramp( tensionNode::thisMObject(), aColorRamp, 2, 1.0f, MColor( 1, 0, 0 ), 1 );
}

MStatus tensionNode::initialize()
{
    MFnTypedAttribute tAttr;

    aOrigShape = tAttr.create( origAttrName, origAttrName, MFnMeshData::kMesh );
    tAttr.setStorable( true );

    aDeformedShape = tAttr.create( deformedAttrName, deformedAttrName, MFnMeshData::kMesh );
    tAttr.setStorable( true );

    aColorSetName = tAttr.create( "colorSetName", "colorSetName", MFnData::kString );
    MString defaultString("tension");
    MFnStringData defaultTextData;
    MObject defaultTextAttr = defaultTextData.create(defaultString);
    tAttr.setWritable( true );
    tAttr.setStorable( true );
    tAttr.setDefault( defaultTextAttr );

    aOutShape = tAttr.create( "out", "out", MFnMeshData::kMesh );
    tAttr.setWritable( false );
    tAttr.setStorable( false );

    aColorRamp = MRampAttribute::createColorRamp("color", "color");

    addAttribute( aOrigShape );
    addAttribute( aDeformedShape );
    addAttribute( aOutShape );
    addAttribute( aColorRamp );
    addAttribute( aColorSetName );

    attributeAffects( aOrigShape, aOutShape );
    attributeAffects( aDeformedShape, aOutShape );
    attributeAffects( aColorRamp, aOutShape );
    attributeAffects( aColorSetName, aOutShape );

    return MStatus::kSuccess;
}

MStatus tensionNode::compute( const MPlug& plug, MDataBlock& data )
{
    MStatus status;

    if ( plug == aOutShape )
    {
        MObject thisObj = thisMObject();

        MDataHandle outHandle = data.outputValue( aOutShape, &status );
        MCheckStatus( status, "ERR: getting out dataHandle" );

        MRampAttribute colorAttribute( thisObj, aColorRamp, &status );
        MCheckStatus( status, "ERR: getting colorRamp attribute" );

        if ( isOrigDirty == true )
        {
            MDataHandle origHandle = data.inputValue( aOrigShape, &status );
            MCheckStatus( status, "ERR: getting origShape dataHandle" );
            origEdgeLenArray = getEdgeLen( origHandle );
            isOrigDirty = false;
        }


        MDataHandle deformedHandle = data.inputValue( aDeformedShape, &status );
        MCheckStatus( status, "ERR: getting deformedShape dataHandle" );
        deformedEdgeLenArray = getEdgeLen( deformedHandle );
        outHandle.set( deformedHandle.asMesh() );
        isDeformedDirty = false;
        isColorSetDirty = true;

        MObject outMesh = outHandle.asMesh();
        MFnMesh meshFn( outMesh, &status );
        MCheckStatus( status, "ERR: getting meshfn" );
        int numVerts = meshFn.numVertices( &status );
        MCheckStatus( status, "ERR: getting vert count" );

        if ( isColorSetDirty == true )
        {
            colorSetName = data.inputValue( aColorSetName, &status ).asString();
            MCheckStatus( status, "ERR: getting colorSetName attribute" );

            status = meshFn.createColorSetDataMesh(colorSetName);
            MCheckStatus( status, "ERR: creating colorSet" );
            isColorSetDirty = false;
        }

        MColorArray vertColors;
        MIntArray vertexCount, vertexList;


        if (numVerts == origEdgeLenArray.length() && numVerts == deformedEdgeLenArray.length())
        {
            meshFn.getVertices(vertexCount, vertexList);
            MCheckStatus( vertColors.setLength( numVerts ), "ERR: setting array length" );
            double delta;
            MColor vertColor;
            for ( int i = 0; i < numVerts; ++i)
            {
                delta = ( ( origEdgeLenArray[i] - deformedEdgeLenArray[i] ) / origEdgeLenArray[i] ) + 0.5;

                colorAttribute.getColorAtPosition(delta, vertColor, &status);
                MCheckStatus( status, "ERR: getting color ramp attribute" );
                vertColors.set( vertColor, i );
            }
        }
        else
        {
            MCheckStatus( vertColors.setLength( 1 ), "ERR: setting array length" );
            MColor vertColor;
            vertexList = MIntArray(meshFn.numFaceVertices(), 0);
            colorAttribute.getColorAtPosition(0.5f, vertColor, &status);
            MCheckStatus( status, "ERR: getting color ramp attribute" );
            vertColors.set( vertColor, 0 );
        }

        meshFn.clearColors( &colorSetName );
        meshFn.setColors( vertColors, &colorSetName );
        meshFn.assignColors( vertexList, &colorSetName );
    }
    data.setClean( plug );
    return MStatus::kSuccess;
}

MDoubleArray tensionNode::getEdgeLen( const MDataHandle& meshHandle )
// iterate over each vertex, get all connected edge lengths, sum them up and
// append them to the MDoubleArray that will be returned.
{
    MStatus status;
    int dummy;
    MDoubleArray edgeLenArray;

    MObject meshObj = meshHandle.asMesh();
    MItMeshEdge edgeIter( meshObj, &status );
    MItMeshVertex vertIter( meshObj,  &status );

    while ( !vertIter.isDone() )
    {
        double lengthSum = 0.0;
        MIntArray connectedEdges;
        vertIter.getConnectedEdges( connectedEdges );
        for ( int i = 0; i < connectedEdges.length(); i++)
        {
            double length;
            edgeIter.setIndex( connectedEdges[i], dummy );
            edgeIter.getLength( length );
            lengthSum += length;
        }
        edgeLenArray.append( lengthSum );
        vertIter.next();
    }
    return edgeLenArray;
}

MStatus tensionNode::setDependentsDirty( const MPlug &dirtyPlug, MPlugArray &affectedPlugs )
// set isOrigDirty and/or isDeformedDirty
{
    if (dirtyPlug == aColorSetName )
    {
        isColorSetDirty = true;
    }
    if (dirtyPlug == aDeformedShape )
    {
        isDeformedDirty = true;
    }
    if ( dirtyPlug == aOrigShape )
    {
        isOrigDirty = true;
    }
    return MStatus::kSuccess;
}
