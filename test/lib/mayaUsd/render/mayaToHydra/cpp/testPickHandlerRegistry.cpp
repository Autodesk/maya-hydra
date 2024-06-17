// Copyright 2024 Autodesk
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

#include <mayaHydraLib/mayaHydra.h>
#include <mayaHydraLib/pick/mhPickHandlerRegistry.h>
#include <mayaHydraLib/pick/mhPickHandler.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace {

class TestPickHandler : public MayaHydra::PickHandler {
public:

    TestPickHandler() = default;

    static PickHandlerConstPtr create() {
        return std::make_shared<TestPickHandler>();
    }

    bool handlePickHit(const Input&, Output&) const override { return true; }
};

}

TEST(TestPickHandlerRegistry, testRegistry)
{
    // Exercise the pick handler registry.
    auto& r = PickHandlerRegistry::Instance();
    
    auto dummy = TestPickHandler::create();

    // Can't register for an empty prefix.
    ASSERT_FALSE(r.Register(SdfPath(), dummy));

    std::vector<SdfPath> registered;
    auto fooBarH = TestPickHandler::create();
    SdfPath fooBarP("/foo/bar");
    SdfPath fooP("/foo");

    ASSERT_TRUE(r.Register(fooBarP, fooBarH));
    ASSERT_EQ(r.GetHandler(fooBarP), fooBarH);
    registered.push_back(fooBarP);

    // fooBarH is the handler for its own path and descendants, not ancestors
    // or unrelated paths.
    ASSERT_EQ(r.GetHandler(SdfPath("/foo/bar/bli")), fooBarH);
    ASSERT_FALSE(r.GetHandler(fooP));
    ASSERT_FALSE(r.GetHandler(SdfPath("/bar")));
    ASSERT_FALSE(r.GetHandler(SdfPath("/zebra")));

    // Add handlers for siblings, legal.
    auto fooBackH = TestPickHandler::create();
    auto fooRedH = TestPickHandler::create();
    SdfPath fooBackP("/foo/back");
    SdfPath fooRedP("/foo/red");

    ASSERT_TRUE(r.Register(fooBackP, fooBackH));
    ASSERT_TRUE(r.Register(fooRedP, fooRedH));
    registered.push_back(fooBackP);
    registered.push_back(fooRedP);

    ASSERT_EQ(r.GetHandler(SdfPath("/foo/bar/bli")), fooBarH);
    ASSERT_EQ(r.GetHandler(SdfPath("/foo/back/bli")), fooBackH);
    ASSERT_EQ(r.GetHandler(SdfPath("/foo/red/bli")), fooRedH);

    // Add handlers for ancestors, descendants, illegal.
    ASSERT_FALSE(r.Register(fooP, dummy));
    ASSERT_FALSE(r.Register(SdfPath("/foo/bar/bli"), dummy));

    // Add handlers to the head, tail of the map.
    SdfPath appleP("/apple");
    SdfPath wizardP("/wizard");
    auto appleH = TestPickHandler::create();
    auto wizardH = TestPickHandler::create();

    ASSERT_TRUE(r.Register(appleP, appleH));
    ASSERT_TRUE(r.Register(wizardP, wizardH));
    registered.push_back(appleP);
    registered.push_back(wizardP);

    ASSERT_EQ(r.GetHandler(SdfPath("/apple/pear")), appleH);
    ASSERT_EQ(r.GetHandler(SdfPath("/wizard/sorcerer")), wizardH);

    // Clean up.
    for (const auto& h : registered) {
        ASSERT_TRUE(r.Unregister(h));
    }
}
