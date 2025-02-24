// Copyright 2025 Autodesk
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

#include "testUtils.h"

#include <flowViewport/imageWriter/fvpImageBufferWriter.h>

#include <gtest/gtest.h>

#include <mayaHydraLib/mayaHydra.h>

#include <maya/MViewport2Renderer.h>

#include <pxr/pxr.h>

#include <string>
#include <cstdlib>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

TEST(TestWriteFile, setFileName)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const std::string fileName(argv[0]);

    Fvp::ImageBufferWriter::SetFileName(fileName);
}

TEST(TestWriteFile, setImageSize)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 2);
    int width {std::atoi(argv[0])};
    int height{std::atoi(argv[1])};
    ASSERT_GT(width, 0);
    ASSERT_GT(height, 0);

    auto renderer = MHWRender::MRenderer::theRenderer();
    ASSERT_NE(renderer, nullptr);

    renderer->setOutputTargetOverrideSize(unsigned int(width), unsigned int(height));
}

TEST(TestWriteFile, unsetImageSize)
{
    auto renderer = MHWRender::MRenderer::theRenderer();
    ASSERT_NE(renderer, nullptr);

    renderer->unsetOutputTargetOverrideSize();
}
