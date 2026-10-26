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

#include "base/Error.h"
#include "base/PreferenceManager.h"
#include "mdl/BrushNode.h"
#include "mdl/GroupNode.h"
#include "mdl/Map.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Selection.h"
#include "mdl/TestFactory.h"
#include "prefs/Preferences.h"
#include "ui/DrawShapeTool.h"
#include "ui/DrawShapeToolExtension.h"
#include "ui/DrawShapeToolExtensionRegistry.h"
#include "ui/MapDocument.h"
#include "ui/MapDocumentFixture.h"

#include "vm/bbox.h"
#include "vm/mat_ext.h"

#include <algorithm>
#include <ranges>

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{
namespace
{

class FailingDrawShapeToolExtension : public DrawShapeToolExtension
{
public:
  explicit FailingDrawShapeToolExtension(MapDocument& document)
    : DrawShapeToolExtension{document}
  {
  }

  const std::string& name() const override
  {
    static const auto value = std::string{"Failing"};
    return value;
  }

  const std::filesystem::path& iconPath() const override
  {
    static const auto value = std::filesystem::path{"ShapeTool_Cuboid.svg"};
    return value;
  }

  Result<std::vector<mdl::Brush>> createBrushes(
    const vm::bbox3d&, const DrawShapeToolParameters&) const override
  {
    return Error{"Expected test failure"};
  }
};

} // namespace

TEST_CASE("DrawShapeTool")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto& map = document.map();

  constexpr auto bounds = vm::bbox3d{{0, 0, 0}, {64, 64, 64}};

  SECTION("failed parameter application keeps the current selection")
  {
    const auto registry = DrawShapeToolExtensionRegistry{{
      {
        .id = "builtin.cuboid",
        .name = "Failing",
        .iconPath = "ShapeTool_Cuboid.svg",
        .originPath = std::nullopt,
        .factory =
          [](auto& factoryDocument) {
            return std::make_unique<FailingDrawShapeToolExtension>(factoryDocument);
          },
      },
    }};

    auto* brushNode = mdl::createBrushNode(map);
    mdl::addNodes(map, {{&mdl::parentForNodes(map), {brushNode}}});
    mdl::selectNodes(map, {brushNode});
    REQUIRE(map.selection().nodes == std::vector<mdl::Node*>{brushNode});

    auto tool = DrawShapeTool{document, registry};
    tool.applyExtensionParameters();

    CHECK(map.selection().nodes == std::vector<mdl::Node*>{brushNode});
    CHECK(brushNode->parent() != nullptr);
  }

  SECTION("update transforms brushes from grid coordinates into world coordinates")
  {
    const auto registry = createDrawShapeToolExtensionRegistry();
    auto tool = DrawShapeTool{document, registry};

    const auto localBounds = vm::bbox3d{{0, 0, 0}, {8, 16, 4}};
    const auto localToWorld =
      vm::translation_matrix(vm::vec3d{64, 32, 8})
      * vm::rotation_matrix(vm::vec3d{0, 0, 1}, vm::to_radians(45.0));

    tool.update(localBounds, localToWorld);
    tool.createBrushes(std::nullopt);

    REQUIRE(map.selection().nodes.size() == 1u);
    const auto* brushNode = dynamic_cast<const mdl::BrushNode*>(map.selection().nodes[0]);
    REQUIRE(brushNode != nullptr);

    const auto positions = brushNode->brush().vertexPositions();
    for (const auto& localVertex : localBounds.vertices())
    {
      const auto expected = localToWorld * localVertex;
      CHECK(std::ranges::any_of(positions, [&](const auto& position) {
        return vm::is_equal(position, expected, vm::Cd::almost_zero());
      }));
    }
  }

  SECTION("createBrushes groups multiple brushes and names the group after the shape")
  {
    const auto registry = createDrawShapeToolExtensionRegistry();
    auto tool = DrawShapeTool{document, registry};

    REQUIRE(pref(Preferences::GroupBrushesCreatedByShapeTool));

    REQUIRE(tool.extensionManager().setCurrentExtension("builtin.stairs"));

    tool.update(bounds);
    tool.createBrushes(tool.groupNameForCreatedBrushes());

    const auto& selectedNodes = map.selection().nodes;
    REQUIRE(selectedNodes.size() == 1);

    auto* groupNode = dynamic_cast<mdl::GroupNode*>(selectedNodes.front());
    REQUIRE(groupNode != nullptr);
    CHECK(groupNode->group().name() == "Stairs");
    CHECK(groupNode->childCount() > 1);
  }

  SECTION(
    "createBrushes does not group multiple brushes when "
    "GroupBrushesCreatedByShapeTool is disabled")
  {
    const auto registry = createDrawShapeToolExtensionRegistry();
    auto tool = DrawShapeTool{document, registry};

    setPref(Preferences::GroupBrushesCreatedByShapeTool, false);

    REQUIRE(tool.extensionManager().setCurrentExtension("builtin.stairs"));

    tool.update(bounds);
    tool.createBrushes(tool.groupNameForCreatedBrushes());

    const auto& selectedNodes = map.selection().nodes;
    CHECK(selectedNodes.size() > 1);
    for (auto* node : selectedNodes)
    {
      CHECK(dynamic_cast<mdl::BrushNode*>(node) != nullptr);
    }

    PreferenceManager::instance().resetToDefault(
      Preferences::GroupBrushesCreatedByShapeTool);
  }

  SECTION("createBrushes does not create a group for a single brush")
  {
    const auto registry = createDrawShapeToolExtensionRegistry();
    auto tool = DrawShapeTool{document, registry};

    // Cuboid is the default extension and always creates a single brush.
    REQUIRE(tool.extensionManager().currentExtension().name() == "Cuboid");

    tool.update(bounds);
    tool.createBrushes(tool.groupNameForCreatedBrushes());

    const auto& selectedNodes = map.selection().nodes;
    REQUIRE(selectedNodes.size() == 1);
    CHECK(dynamic_cast<mdl::BrushNode*>(selectedNodes.front()) != nullptr);
  }
}

} // namespace tb::ui
