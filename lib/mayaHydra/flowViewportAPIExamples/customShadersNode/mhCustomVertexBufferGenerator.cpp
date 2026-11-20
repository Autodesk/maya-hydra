//
// Copyright 2026 Autodesk, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//maya headers
#include <maya/MDrawRegistry.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MHWGeometry.h>
#include <maya/MPxVertexBufferGenerator.h>

namespace {

//Custom semantic of the stream, it must match the customSemantics annotation of myCustomShader.ogsfx
//and the primvar name read by the FVP_CustomBufferShader glslfx shader.
const char* const kCustomSemantic = "myCustomStreamB";

bool _registered = false;

/// Vertex buffer generator filling the myCustomStreamB custom stream by combining the Position
/// and Normal streams : (position.y, position.z, normal.x, normal.z).
/// This is the same stream as the one created by the Maya devkit vertexBufferGenerator plugin
/// MyCustomBufferGenerator2 class.
class MhCustomVertexBufferGenerator : public MHWRender::MPxVertexBufferGenerator
{
public:
    static MHWRender::MPxVertexBufferGenerator* creator() { return new MhCustomVertexBufferGenerator(); }

    bool getSourceIndexing(const MObject& object, MHWRender::MComponentDataIndexing& sourceIndexing) const override
    {
        MStatus status;
        MFnMesh mesh(object, &status);
        if (!status) {
            return false;
        }

        MIntArray vertexCount, vertexList;
        mesh.getVertices(vertexCount, vertexList);

        MUintArray& vertices = sourceIndexing.indices();
        for (unsigned int i = 0; i < vertexList.length(); ++i) {
            vertices.append(static_cast<unsigned int>(vertexList[i]));
        }

        return true;
    }

    bool getSourceStreams(const MObject& /*object*/, MStringArray& sourceStreams) const override
    {
        sourceStreams.append("Position");
        sourceStreams.append("Normal");
        return true;
    }

    void createVertexStream(
        const MObject&                           object,
        MHWRender::MVertexBuffer&                vertexBuffer,
        const MHWRender::MComponentDataIndexing& targetIndexing,
        const MHWRender::MComponentDataIndexing& /*sharedIndexing*/,
        const MHWRender::MVertexBufferArray&     sourceStreams) const override
    {
        const MHWRender::MVertexBufferDescriptor& descriptor = vertexBuffer.descriptor();

        const MHWRender::MGeometry::DataType dataType = descriptor.dataType();
        if (dataType != MHWRender::MGeometry::kFloat && dataType != MHWRender::MGeometry::kInt32) {
            return;
        }

        const int dimension = descriptor.dimension();
        if (dimension != 3 && dimension != 4) {
            return;
        }

        if (descriptor.semantic() != MHWRender::MGeometry::kTexture) {
            return;
        }

        MStatus status;
        MFnMesh mesh(object, &status);
        if (!status) {
            return;
        }

        const unsigned int vertexCount = targetIndexing.indices().length();
        if (vertexCount == 0) {
            return;
        }

        MHWRender::MVertexBuffer* positionStream = sourceStreams.getBuffer("Position");
        if (!positionStream || positionStream->descriptor().dataType() != MHWRender::MGeometry::kFloat) {
            return;
        }
        const int positionDimension = positionStream->descriptor().dimension();
        if (positionDimension != 3 && positionDimension != 4) {
            return;
        }

        MHWRender::MVertexBuffer* normalStream = sourceStreams.getBuffer("Normal");
        if (!normalStream || normalStream->descriptor().dataType() != MHWRender::MGeometry::kFloat) {
            return;
        }
        const int normalDimension = normalStream->descriptor().dimension();
        if (normalDimension != 3 && normalDimension != 4) {
            return;
        }

        const float* positionBuffer = static_cast<const float*>(positionStream->map());
        if (!positionBuffer) {
            return;
        }

        const float* normalBuffer = static_cast<const float*>(normalStream->map());
        if (normalBuffer) {
            void* buffer = vertexBuffer.acquire(vertexCount, true /*writeOnly*/);
            if (buffer) {
                float* bufferAsFloat = static_cast<float*>(buffer);
                int*   bufferAsInt = static_cast<int*>(buffer);

                for (unsigned int i = 0; i < vertexCount; ++i) {
                    if (dataType == MHWRender::MGeometry::kFloat) {
                        *(bufferAsFloat++) = positionBuffer[1];
                        *(bufferAsFloat++) = positionBuffer[2];
                        *(bufferAsFloat++) = normalBuffer[0];
                        if (dimension == 4) {
                            *(bufferAsFloat++) = normalBuffer[2];
                        }
                    } else {
                        *(bufferAsInt++) = static_cast<int>(positionBuffer[1] * 255);
                        *(bufferAsInt++) = static_cast<int>(positionBuffer[2] * 255);
                        *(bufferAsInt++) = static_cast<int>(normalBuffer[0] * 255);
                        if (dimension == 4) {
                            *(bufferAsInt++) = static_cast<int>(normalBuffer[2] * 255);
                        }
                    }

                    positionBuffer += positionDimension;
                    normalBuffer += normalDimension;
                }

                vertexBuffer.commit(buffer);
            }

            normalStream->unmap();
        }

        positionStream->unmap();
    }
};

} // namespace

MStatus registerCustomVertexBufferGenerator()
{
    // Registration fails if another plugin (e.g. the Maya devkit vertexBufferGenerator plugin) already
    // registered a generator for this semantic, which then fills the same stream.
    const MStatus status = MHWRender::MDrawRegistry::registerVertexBufferGenerator(
        kCustomSemantic, MhCustomVertexBufferGenerator::creator);
    _registered = (status == MS::kSuccess);
    if (!_registered) {
        MGlobal::displayWarning(
            MString("A vertex buffer generator is already registered for the custom semantic ")
            + kCustomSemantic);
    }
    return MS::kSuccess;
}

MStatus deregisterCustomVertexBufferGenerator()
{
    if (!_registered) {
        return MS::kSuccess;
    }
    _registered = false;
    return MHWRender::MDrawRegistry::deregisterVertexBufferGenerator(kCustomSemantic);
}
