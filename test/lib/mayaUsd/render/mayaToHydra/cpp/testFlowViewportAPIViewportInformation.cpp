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

//maya hydra
#include <mayaHydraLib/hydraUtils.h>
#include <mayaHydraLib/mayaUtils.h>

//Flow viewport headers
#include <flowViewport/API/fvpInformationInterface.h>
#include <flowViewport/API/samples/fvpInformationClientExample.h>

//Google tests
#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

//Subclass FVP_NS_DEF::InformationClient to register this class to receive callbacks.
class InfoClientTest : public FVP_NS_DEF::InformationClient
{
public:
    InfoClientTest() = default;
    ~InfoClientTest() override = default;

    //From FVP_NS_DEF::InformationClient
    void SceneIndexAdded(const FVP_NS_DEF::InformationInterface::ViewportInformation& viewportInformation)override
    {
        ++_numSceneIndexAdded;//We want to count the number of times this is called
    }

    void SceneIndexRemoved(const FVP_NS_DEF::InformationInterface::ViewportInformation& viewportInformation)override
    {
        ++_numSceneIndexRemoved;//We want to count the number of times this is called
    }

    int GetSceneIndexAdded() const  {return _numSceneIndexAdded;}
    int GetSceneIndexRemoved() const{return _numSceneIndexRemoved;}

protected:
    int _numSceneIndexAdded     = 0; 
    int _numSceneIndexRemoved   = 0;
};


//Is global instance
static InfoClientTest _infoClientTest;

//Storm renderer name
static const std::string _stormRendererName ("GL");


//The test is done through 3 Steps :

//Step 1) The python script sets Storm as the renderer for the viewport then call viewportInformationWithHydra
//This is with Hydra, Storm should be the current renderer, check the python script with the same filename as this cpp file
TEST(FlowViewportAPI, viewportInformationWithHydra)
{    
    //Get Information interface
    Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();
    
    //Register our callbacks client
    informationInterface.RegisterInformationClient(_infoClientTest);
    
    //Get all Hydra viewports information
    Fvp::ViewportInformationSet viewportInformationSet;
    informationInterface.GetViewportsInformation(viewportInformationSet);
    ASSERT_EQ(viewportInformationSet.size(), 1);//We should have 1 hydra viewport

    //Check renderer name
    auto it = viewportInformationSet.cbegin();
    ASSERT_NE(it, viewportInformationSet.cend());
    auto viewportInfo = (*it);
    ASSERT_EQ(viewportInfo->_rendererName, _stormRendererName);

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
    Fvp::ViewportInformationSet viewportInformationSet;
    informationInterface.GetViewportsInformation(viewportInformationSet);
    ASSERT_EQ(viewportInformationSet.size(), 0); //we should have no Hydra viewports

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
    Fvp::ViewportInformationSet viewportInformationSet;
    informationInterface.GetViewportsInformation(viewportInformationSet);
    ASSERT_EQ(viewportInformationSet.size(), 1);//We should have 1 hydra viewport

    //Check renderer name
    auto it = viewportInformationSet.cbegin();
    ASSERT_NE(it, viewportInformationSet.cend());
    auto viewportInfo = (*it);
    ASSERT_EQ(viewportInfo->_rendererName, _stormRendererName);

    //Both should have been called once only
    ASSERT_EQ(_infoClientTest.GetSceneIndexAdded(), 1);
    ASSERT_EQ(_infoClientTest.GetSceneIndexRemoved(), 1);

    //Unregister our callbacks client
    informationInterface.UnregisterInformationClient(_infoClientTest);
}
