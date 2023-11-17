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

#ifndef MAYATOHYDRA_CPP_INFO_CLIENT_TEST
#define MAYATOHYDRA_CPP_INFO_CLIENT_TEST


//Flow viewport headers
#include <flowViewport/API/fvpInformationInterface.h>
#include <flowViewport/API/samples/fvpInformationClientExample.h>

//Subclass Fvp::InformationClient to register this class to receive callbacks.
class InfoClientTest : public Fvp::InformationClient
{
public:
    InfoClientTest() = default;
    ~InfoClientTest() override = default;

    //From Fvp::InformationClient
    void SceneIndexAdded(const Fvp::InformationInterface::ViewportInformation& viewportInformation)override
    {
        ++_numSceneIndexAdded;//We want to count the number of times this is called
    }

    void SceneIndexRemoved(const Fvp::InformationInterface::ViewportInformation& viewportInformation)override
    {
        ++_numSceneIndexRemoved;//We want to count the number of times this is called
    }

    int GetSceneIndexAdded() const  {return _numSceneIndexAdded;}
    int GetSceneIndexRemoved() const{return _numSceneIndexRemoved;}

protected:
    int _numSceneIndexAdded     = 0; 
    int _numSceneIndexRemoved   = 0;
};

#endif //MAYATOHYDRA_CPP_INFO_CLIENT_TEST