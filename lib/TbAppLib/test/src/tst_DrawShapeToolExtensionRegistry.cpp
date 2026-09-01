/*
 Copyright (C) 2026 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ui/DrawShapeToolExtensionManager.h"
#include "ui/DrawShapeToolExtensionRegistry.h"
#include "ui/MapDocumentFixture.h"

#include <array>

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{

TEST_CASE("DrawShapeToolExtensionRegistry")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  const auto registry = createDrawShapeToolExtensionRegistry();

  SECTION("contains every built-in shape in UI order")
  {
    const auto& descriptors = registry.descriptors();
    static constexpr auto expectedIds = std::array{
      "builtin.cuboid",
      "builtin.stairs",
      "builtin.arch",
      "builtin.corridor",
      "builtin.corridor-bend",
      "builtin.corridor-t-junction",
      "builtin.chamber",
      "builtin.cylinder",
      "builtin.cone",
      "builtin.torus",
      "builtin.uv-sphere",
      "builtin.ico-sphere",
    };
    REQUIRE(descriptors.size() == expectedIds.size());

    for (auto i = size_t{0}; i < descriptors.size(); ++i)
    {
      CHECK(descriptors[i].apiVersion == 1u);
      CHECK(descriptors[i].id == expectedIds[i]);
      CHECK_FALSE(descriptors[i].originPath.has_value());
    }
  }

  SECTION("looks up descriptors by stable id")
  {
    REQUIRE(registry.find("builtin.arch") != nullptr);
    CHECK(registry.find("builtin.arch")->name == "Arch");
    CHECK(registry.find("missing") == nullptr);
  }

  SECTION("constructs the existing native extensions")
  {
    for (const auto& descriptor : registry.descriptors())
    {
      const auto extension = descriptor.factory(document);
      CHECK(extension->name() == descriptor.name);
      CHECK(extension->iconPath() == descriptor.iconPath);
    }
  }
}

TEST_CASE("DrawShapeToolExtensionManager")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  const auto registry = createDrawShapeToolExtensionRegistry();
  auto manager = DrawShapeToolExtensionManager{document, registry};

  SECTION("instantiates every extension exclusively from the registry")
  {
    const auto& infos = manager.extensionInfos();
    REQUIRE(infos.size() == registry.descriptors().size());

    CHECK(infos[0].id == "builtin.cuboid");
    CHECK(infos[1].id == "builtin.stairs");
    CHECK(infos[2].id == "builtin.arch");
    CHECK(infos[3].id == "builtin.corridor");
    CHECK(infos.back().id == "builtin.ico-sphere");
  }

  SECTION("does not append extensions outside the supplied registry")
  {
    const auto singleExtensionRegistry =
      DrawShapeToolExtensionRegistry{{registry.descriptors().front()}};
    auto singleExtensionManager =
      DrawShapeToolExtensionManager{document, singleExtensionRegistry};

    REQUIRE(singleExtensionManager.extensionInfos().size() == 1u);
    CHECK(singleExtensionManager.currentExtensionInfo().id == "builtin.cuboid");
  }

  SECTION("keeps parameter values independently for each extension")
  {
    manager.parameters(1u).setStepHeight(32.0);
    manager.parameters(2u).setStepHeight(64.0);

    CHECK(manager.parameters(1u).stepHeight() == 32.0);
    CHECK(manager.parameters(2u).stepHeight() == 64.0);
  }

  SECTION("selects extensions by stable id")
  {
    CHECK(manager.currentExtensionInfo().id == "builtin.cuboid");
    CHECK(manager.currentExtensionIndex() == 0u);

    CHECK(manager.setCurrentExtension("builtin.arch"));
    CHECK(manager.currentExtensionInfo().id == "builtin.arch");
    CHECK(manager.currentExtensionIndex() == 2u);

    CHECK_FALSE(manager.setCurrentExtension("missing"));
    CHECK(manager.currentExtensionInfo().id == "builtin.arch");
    CHECK_FALSE(manager.setCurrentExtensionIndex(manager.extensionInfos().size()));
  }
}

} // namespace tb::ui
