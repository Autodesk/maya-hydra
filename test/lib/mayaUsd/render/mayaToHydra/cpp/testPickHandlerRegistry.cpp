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

#include <pxr/base/tf/errorMark.h>

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

// A handler of another type, which cannot share a prefix with TestPickHandler.
class OtherTestPickHandler : public MayaHydra::PickHandler {
public:

    OtherTestPickHandler() = default;

    static PickHandlerConstPtr create() {
        return std::make_shared<OtherTestPickHandler>();
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

    // Can't register for an absolute root prefix.
    ASSERT_FALSE(r.Register(SdfPath::AbsoluteRootPath(), dummy));

    // Releasing a registration unregisters its handler, so keep them.
    std::vector<PickRegistrationPtr> registrations;
    auto fooBarH = TestPickHandler::create();
    SdfPath fooBarP("/foo/bar");
    SdfPath fooP("/foo");

    auto fooBarR = r.Register(fooBarP, fooBarH);
    ASSERT_TRUE(fooBarR);
    ASSERT_EQ(fooBarR->GetPrefix(), fooBarP);
    ASSERT_EQ(r.GetHandler(fooBarP), fooBarH);
    registrations.push_back(fooBarR);

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

    registrations.push_back(r.Register(fooBackP, fooBackH));
    ASSERT_TRUE(registrations.back());
    registrations.push_back(r.Register(fooRedP, fooRedH));
    ASSERT_TRUE(registrations.back());

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

    registrations.push_back(r.Register(appleP, appleH));
    ASSERT_TRUE(registrations.back());
    registrations.push_back(r.Register(wizardP, wizardH));
    ASSERT_TRUE(registrations.back());

    ASSERT_EQ(r.GetHandler(SdfPath("/apple/pear")), appleH);
    ASSERT_EQ(r.GetHandler(SdfPath("/wizard/sorcerer")), wizardH);

    // Clean up: releasing the registrations unregisters the handlers.
    registrations.clear();
    fooBarR.reset();
    for (const auto& p : { fooBarP, fooBackP, fooRedP, appleP, wizardP }) {
        ASSERT_FALSE(r.RegisteredHandler(p));
    }
}

TEST(TestPickHandlerRegistry, testSharedRegistration)
{
    auto& r = PickHandlerRegistry::Instance();

    SdfPath prefix("/shared");
    auto firstH = TestPickHandler::create();
    auto secondH = TestPickHandler::create();

    // A second client registering the same prefix with a handler of the same type
    // shares the first registration, and the first handler stays registered.
    auto firstR = r.Register(prefix, firstH);
    ASSERT_TRUE(firstR);
    auto secondR = r.Register(prefix, secondH);
    ASSERT_EQ(secondR, firstR);
    ASSERT_EQ(r.RegisteredHandler(prefix), firstH);

    // A handler of another type cannot share the prefix.
    {
        TfErrorMark errorMark;
        ASSERT_FALSE(r.Register(prefix, OtherTestPickHandler::create()));
        ASSERT_FALSE(errorMark.IsClean());
        errorMark.Clear();
    }
    ASSERT_EQ(r.RegisteredHandler(prefix), firstH);

    // The handler stays registered until the last client releases the registration.
    firstR.reset();
    ASSERT_EQ(r.RegisteredHandler(prefix), firstH);
    secondR.reset();
    ASSERT_FALSE(r.RegisteredHandler(prefix));

    // Registering again after the full release creates a new registration, with the
    // new handler, of any type.
    auto otherH = OtherTestPickHandler::create();
    auto otherR = r.Register(prefix, otherH);
    ASSERT_TRUE(otherR);
    ASSERT_EQ(r.RegisteredHandler(prefix), otherH);
    otherR.reset();
    ASSERT_FALSE(r.RegisteredHandler(prefix));
}
