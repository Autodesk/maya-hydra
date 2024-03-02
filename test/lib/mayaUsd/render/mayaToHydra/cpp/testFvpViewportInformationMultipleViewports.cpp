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
    std::shared_ptr<InfoClientTest> _infoClientTest;

    //Storm renderer name
    const std::string _stormRendererName ("GL");
}

//The test is done through 4 Steps in the python script testFlowViewportAPIViewportInformation.py function test_MultipleViewports

//Step 1) The python script calls viewportInformationMultipleViewportsInit before anything is done.
TEST(FlowViewportAPI, viewportInformationMultipleViewportsInit)
{    
    //Get Information interface
    Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();
    
    //Register our callbacks client
    _infoClientTest = std::make_shared<InfoClientTest>();
    informationInterface.RegisterInformationClient(_infoClientTest);
    
    //Get all Hydra viewports information
    Fvp::InformationInterface::ViewportInformationSet allViewportsInformation;
    informationInterface.GetViewportsInformation(allViewportsInformation);
    ASSERT_EQ(allViewportsInformation.size(), (size_t)1);//We should have 0 hydra viewport

    //Check initial count for _infoClientTest callbacks
    ASSERT_EQ(_infoClientTest->GetSceneIndexAdded(), 0);
    ASSERT_EQ(_infoClientTest->GetSceneIndexRemoved(), 0);

    //We don't call UnregisterInformationClient on purpose as we want to check if the callbacks are called and will unregister it in the code below
}

//Step 2 : The python script has created 4 viewports and 2 are set to Storm
//and _infoClientTest is registered in Fvp::InformationInterface so InfoClientTest::SceneIndexAdded should be have been called twice
//then it calls viewportInformationMultipleViewports2Viewports
TEST(FlowViewportAPI, viewportInformationMultipleViewports2Viewports)
{
    //Get Information interface
    Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();
    
    //Get all Hydra viewports information
    Fvp::InformationInterface::ViewportInformationSet allViewportsInformation;
    informationInterface.GetViewportsInformation(allViewportsInformation);
    ASSERT_EQ(allViewportsInformation.size(), (size_t)3); //we should have 2 Hydra viewports

    //Check renderers name, they all should be Storm
    for (auto info : allViewportsInformation){
        ASSERT_EQ(info._rendererName, _stormRendererName);
    }

    ASSERT_EQ(_infoClientTest->GetSceneIndexAdded(), 2);//Has been called twice
    ASSERT_EQ(_infoClientTest->GetSceneIndexRemoved(), 0);
}

//Step 3 : the python script removed hydra from 1 of the 2 viewports
//and _infoClientTest is registered in Fvp::InformationInterface so InfoClientTest::SceneIndexRemoved should be have been called once
//then it calls viewportInformationMultipleViewports1Viewport
TEST(FlowViewportAPI, viewportInformationMultipleViewports1Viewport)
{
    //Get Information interface
    Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();
    
    //Get all Hydra viewports information
    Fvp::InformationInterface::ViewportInformationSet allViewportsInformation;
    informationInterface.GetViewportsInformation(allViewportsInformation);
    ASSERT_EQ(allViewportsInformation.size(), (size_t)2);//We should have 1 hydra viewport

   //Check renderers name, they all should be Storm
    for (auto info : allViewportsInformation){
        ASSERT_EQ(info._rendererName, _stormRendererName);
    }

    //Both should have been called once only
    ASSERT_EQ(_infoClientTest->GetSceneIndexAdded(), 2);
    ASSERT_EQ(_infoClientTest->GetSceneIndexRemoved(), 1);
}

//Step 4 : the python script removed hydra from the last viewport where hydra was
//and _infoClientTest is registered in Fvp::InformationInterface so InfoClientTest::SceneIndexRemoved should be have been called once again
//then it calls viewportInformationMultipleViewports0Viewport
TEST(FlowViewportAPI, viewportInformationMultipleViewports0Viewport)
{
    //Get Information interface
    Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();
    
    //Get all Hydra viewports information
    Fvp::InformationInterface::ViewportInformationSet allViewportsInformation;
    informationInterface.GetViewportsInformation(allViewportsInformation);
    ASSERT_EQ(allViewportsInformation.size(), (size_t)1);//We should not have any hydra viewport

   ///Both should have been called once only
    ASSERT_EQ(_infoClientTest->GetSceneIndexAdded(), 2);
    ASSERT_EQ(_infoClientTest->GetSceneIndexRemoved(), 2);

    //Unregister our callbacks client
    informationInterface.UnregisterInformationClient(_infoClientTest);
}
