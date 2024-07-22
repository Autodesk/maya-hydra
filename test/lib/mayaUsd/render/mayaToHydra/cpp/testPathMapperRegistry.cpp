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
#include <flowViewport/selection/fvpPathMapperRegistry.h>
#include <flowViewport/selection/fvpPathMapper.h>

#include <ufe/pathString.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace {

class TestPathMapper : public Fvp::PathMapper {
public:

    TestPathMapper() = default;

    static Fvp::PathMapperConstPtr create() {
        return std::make_shared<TestPathMapper>();
    }

    Fvp::PrimSelections
    UfePathToPrimSelections(const Ufe::Path&) const override { return {}; }
};

}

TEST(TestPathMapperRegistry, testRegistry)
{
    // Exercise the path mapper registry.
    auto& r = Fvp::PathMapperRegistry::Instance();
    
    auto dummy = TestPathMapper::create();

    // Can't register for an empty path.
    ASSERT_FALSE(r.Register(Ufe::Path(), dummy));

    std::vector<Ufe::Path> registered;
    auto fooBarM = TestPathMapper::create();
    auto fooBarP = Ufe::PathString::path("|foo|bar");
    auto fooP = Ufe::PathString::path("|foo");

    ASSERT_TRUE(r.Register(fooBarP, fooBarM));
    ASSERT_EQ(r.GetMapper(fooBarP), fooBarM);
    registered.push_back(fooBarP);

    // fooBarM is the mapper for its own path and descendants, not ancestors
    // or unrelated paths.
    ASSERT_EQ(r.GetMapper(Ufe::PathString::path("|foo|bar|bli")), fooBarM);
    ASSERT_FALSE(r.GetMapper(fooP));
    ASSERT_FALSE(r.GetMapper(Ufe::PathString::path("|bar")));
    ASSERT_FALSE(r.GetMapper(Ufe::PathString::path("|zebra")));

    // Add mappers for siblings, legal.
    auto fooBackM = TestPathMapper::create();
    auto fooRedM = TestPathMapper::create();
    auto fooBackP = Ufe::PathString::path("|foo|back");
    auto fooRedP = Ufe::PathString::path("|foo|red");

    ASSERT_TRUE(r.Register(fooBackP, fooBackM));
    ASSERT_TRUE(r.Register(fooRedP, fooRedM));
    registered.push_back(fooBackP);
    registered.push_back(fooRedP);

    ASSERT_EQ(r.GetMapper(Ufe::PathString::path("|foo|bar|bli")), fooBarM);
    ASSERT_EQ(r.GetMapper(Ufe::PathString::path("|foo|back|bli")), fooBackM);
    ASSERT_EQ(r.GetMapper(Ufe::PathString::path("|foo|red|bli")), fooRedM);

    // Add mappers for ancestors, descendants, illegal.
    ASSERT_FALSE(r.Register(fooP, dummy));
    ASSERT_FALSE(r.Register(Ufe::PathString::path("|foo|bar|bli"), dummy));

    // Add other mappers to the registry.
    auto appleP = Ufe::PathString::path("|apple");
    auto wizardP = Ufe::PathString::path("|wizard");
    auto appleM = TestPathMapper::create();
    auto wizardM = TestPathMapper::create();

    ASSERT_TRUE(r.Register(appleP, appleM));
    ASSERT_TRUE(r.Register(wizardP, wizardM));
    registered.push_back(appleP);
    registered.push_back(wizardP);

    ASSERT_EQ(r.GetMapper(Ufe::PathString::path("|apple|pear")), appleM);
    ASSERT_EQ(r.GetMapper(Ufe::PathString::path("|wizard|sorcerer")), wizardM);

    // Clean up.
    for (const auto& h : registered) {
        ASSERT_TRUE(r.Unregister(h));
    }
}
