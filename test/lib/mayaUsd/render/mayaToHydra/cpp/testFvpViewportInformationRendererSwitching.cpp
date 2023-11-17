//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

//Local headers
#include "testUtils.h"
#include "infoClientTest.h"

//Google tests
#include <gtest/gtest.h>

namespace {
    //Is a global instance
    InfoClientTest _infoClientTest;

    //Storm renderer name
    const std::string _stormRendererName ("GL");
}

//The test is done through 3 Steps in the python script testFlowViewportAPIViewportInformation.py, function test_RendererSwitching

//Step 1) The python script sets Storm as the renderer for the viewport then call viewportInformationWithHydra
//This is with Hydra, Storm should be the current renderer
TEST(FlowViewportAPI, viewportInformationWithHydra)
{    
    //Get Information interface
    Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();
    
    //Register our callbacks client
    informationInterface.RegisterInformationClient(&_infoClientTest);
    
    //Get all Hydra viewports information
    Fvp::InformationInterface::ViewportInformationSet allViewportInformation;
    informationInterface.GetViewportsInformation(allViewportInformation);
    ASSERT_EQ(allViewportInformation.size(), (size_t)1);//We should have 1 hydra viewport

    //Check renderer name
    auto it = allViewportInformation.cbegin();
    ASSERT_NE(it, allViewportInformation.cend());
    ASSERT_EQ(it->_rendererName, _stormRendererName);

    //Check initial count for _infoClientTest callbacks
    ASSERT_EQ(_infoClientTest.GetSceneIndexAdded(), 0);
    ASSERT_EQ(_infoClientTest.GetSceneIndexRemoved(), 0);

    //We don't call UnregisterInformationClient on purpose as we want to check if the callbacks are called and will unregister it in the code below
}

//Step 2 : the python script sets VP2 as the renderer for the viewport then call viewportInformationWithoutHydra
//and _infoClientTest is registered in Fvp::InformationInterface so InfoClientTest::SceneIndexRemoved should be called when switching from Storm to VP2
//This is without Hydra, VP2 should be the current renderer
TEST(FlowViewportAPI, viewportInformationWithoutHydra)
{
    //Get Information interface
    Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();
    
    //Get all Hydra viewports information
    Fvp::InformationInterface::ViewportInformationSet allViewportInformation;
    informationInterface.GetViewportsInformation(allViewportInformation);
    ASSERT_EQ(allViewportInformation.size(), (size_t)0); //we should have no Hydra viewports

    //Only SceneIndexRemoved should have been called once
    ASSERT_EQ(_infoClientTest.GetSceneIndexAdded(), 0);
    ASSERT_EQ(_infoClientTest.GetSceneIndexRemoved(), 1);
}

//Step 3 : the python script sets Storm again as the renderer for the viewport then call viewportInformationWithHydraAgain
//and _infoClientTest is still registered in Fvp::InformationInterface so InfoClientTest::SceneIndexAdded should be called when switching from VP2 to Storm
//This is with Hydra again, Storm should be the current renderer
TEST(FlowViewportAPI, viewportInformationWithHydraAgain)
{
    //Get Information interface
    Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();
    
    //Get all Hydra viewports information
    Fvp::InformationInterface::ViewportInformationSet allViewportInformation;
    informationInterface.GetViewportsInformation(allViewportInformation);
    ASSERT_EQ(allViewportInformation.size(), (size_t)1);//We should have 1 hydra viewport

    //Check renderer name
    auto it = allViewportInformation.cbegin();
    ASSERT_NE(it, allViewportInformation.cend());
    ASSERT_EQ(it->_rendererName, _stormRendererName);

    //Both should have been called once only
    ASSERT_EQ(_infoClientTest.GetSceneIndexAdded(), 1);
    ASSERT_EQ(_infoClientTest.GetSceneIndexRemoved(), 1);

    //Unregister our callbacks client
    informationInterface.UnregisterInformationClient(&_infoClientTest);
}
