/*
 Copyright (C) 2026 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gl/OrthographicCamera.h"
#include "gl/PerspectiveCamera.h"
#include "mdl/BrushBuilder.h"
#include "mdl/BrushNode.h"
#include "mdl/CatchConfig.h"
#include "mdl/Group.h"
#include "mdl/GroupNode.h"
#include "mdl/Map.h"
#include "mdl/Map_NodeLocking.h"
#include "mdl/Map_NodeVisibility.h"
#include "mdl/Map_Nodes.h"
#include "mdl/Map_Selection.h"
#include "mdl/TestUtils.h"
#include "mdl/WorldNode.h"
#include "ui/BoxSelectionTool.h"
#include "ui/BoxSelectionToolController.h"
#include "ui/GestureTracker.h"
#include "ui/InputState.h"
#include "ui/MapDocument.h"
#include "ui/MapDocumentFixture.h"
#include "ui/PickRequest.h"
#include "ui/ToolController.h"

#include "kd/result.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

namespace tb::ui
{
namespace
{

mdl::BrushNode* addBrush(mdl::Map& map, const vm::bbox3d& bounds)
{
  auto builder = mdl::BrushBuilder{map.worldNode().mapFormat(), map.worldBounds()};
  auto* node =
    new mdl::BrushNode{builder.createCuboid(bounds, "material") | kdl::value()};
  addNodes(map, {{&parentForNodes(map), {node}}});
  return node;
}

vm::bbox3d tallBounds(mdl::Map& map, const vm::vec2d& min, const vm::vec2d& max)
{
  const auto worldBounds = map.worldBounds().expand(-1.0);
  return {
    {min.x(), min.y(), worldBounds.min.z()}, {max.x(), max.y(), worldBounds.max.z()}};
}

InputState inputStateFor(
  const gl::OrthographicCamera& camera,
  const vm::vec3d& rayOrigin,
  const float mouseX,
  const ModifierKeyState modifiers = ModifierKeys::None)
{
  auto inputState = InputState{mouseX, 0.0f};
  inputState.setPickRequest(
    PickRequest{vm::ray3d{rayOrigin, vm::vec3d{camera.direction()}}, camera});
  inputState.setModifierKeys(modifiers);
  inputState.mouseDown(MouseButtons::Left);
  return inputState;
}

InputState inputStateFor(
  const gl::PerspectiveCamera& camera,
  const float mouseX,
  const float mouseY,
  const ModifierKeyState modifiers = ModifierKeys::None)
{
  auto inputState = InputState{mouseX, mouseY};
  inputState.setPickRequest(
    PickRequest{vm::ray3d{camera.pickRay(mouseX, mouseY)}, camera});
  inputState.setModifierKeys(modifiers);
  inputState.mouseDown(MouseButtons::Left);
  return inputState;
}

mdl::Selection makeNodeSelection(
  mdl::Map& map, const std::initializer_list<mdl::Node*> nodes)
{
  return mdl::makeSelection(map, std::vector<mdl::Node*>{nodes});
}

} // namespace

TEST_CASE("BoxSelectionTool")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto& map = document.map();

  auto* inside = addBrush(map, {{-16, -16, -16}, {16, 16, 16}});
  auto* crossing = addBrush(map, {{16, -8, 128}, {48, 8, 160}});
  auto* outside = addBrush(map, {{64, 64, -16}, {96, 96, 16}});

  auto tool = BoxSelectionTool{document};

  SECTION("containment and intersection")
  {
    tool.select(
      tallBounds(map, {-32, -32}, {32, 32}),
      BoxSelectionBoundsMode::Contain,
      BoxSelectionMode::Replace);
    CHECK(map.selection() == makeNodeSelection(map, {inside}));

    tool.select(
      tallBounds(map, {-32, -32}, {32, 32}),
      BoxSelectionBoundsMode::Intersect,
      BoxSelectionMode::Replace);
    CHECK(map.selection() == makeNodeSelection(map, {inside, crossing}));
  }

  SECTION("selection modes and undo")
  {
    selectNodes(map, {outside});

    tool.select(
      tallBounds(map, {-32, -32}, {32, 32}),
      BoxSelectionBoundsMode::Contain,
      BoxSelectionMode::Add);
    CHECK(map.selection() == makeNodeSelection(map, {outside, inside}));

    map.undoCommand();
    CHECK(map.selection() == makeNodeSelection(map, {outside}));

    selectNodes(map, {inside});
    tool.select(
      tallBounds(map, {-32, -32}, {32, 32}),
      BoxSelectionBoundsMode::Contain,
      BoxSelectionMode::Subtract);
    CHECK(map.selection() == makeNodeSelection(map, {outside}));
  }

  SECTION("empty replacement clears the selection")
  {
    selectNodes(map, {inside});
    tool.select(vm::bbox3d{}, BoxSelectionBoundsMode::Contain, BoxSelectionMode::Replace);
    CHECK_FALSE(map.selection().hasAny());
  }

  SECTION("adding converts a face selection only when objects match")
  {
    selectBrushFaces(map, {{outside, 0}});
    const auto initialSelection = map.selection();

    tool.select(
      tallBounds(map, {-128, -128}, {-96, -96}),
      BoxSelectionBoundsMode::Contain,
      BoxSelectionMode::Add);
    CHECK(map.selection() == initialSelection);

    tool.select(
      tallBounds(map, {-32, -32}, {32, 32}),
      BoxSelectionBoundsMode::Contain,
      BoxSelectionMode::Add);
    CHECK(map.selection() == makeNodeSelection(map, {inside}));
  }

  SECTION("hidden and locked nodes are excluded")
  {
    lockNodes(map, {inside});
    hideNodes(map, {crossing});

    tool.select(
      tallBounds(map, {-32, -32}, {32, 32}),
      BoxSelectionBoundsMode::Intersect,
      BoxSelectionMode::Replace);

    CHECK_FALSE(map.selection().hasAny());
  }

  SECTION("closed groups are selected atomically")
  {
    auto builder = mdl::BrushBuilder{map.worldNode().mapFormat(), map.worldBounds()};
    auto* groupedBrush = new mdl::BrushNode{
      builder.createCuboid({{-112, -16, -16}, {-96, 16, 16}}, "material") | kdl::value()};
    auto* group = new mdl::GroupNode{mdl::Group{"group"}};
    addNodes(map, {{&parentForNodes(map), {group}}});
    addNodes(map, {{group, {groupedBrush}}});

    tool.select(
      tallBounds(map, {-128, -32}, {-80, 32}),
      BoxSelectionBoundsMode::Contain,
      BoxSelectionMode::Replace);

    CHECK(map.selection() == makeNodeSelection(map, {group}));
  }

  SECTION("2D controller works in all orthographic views")
  {
    struct View
    {
      vm::vec3f position;
      vm::vec3f direction;
      vm::vec3f up;
      vm::vec3d start;
      vm::vec3d end;
    };

    const auto views = std::vector<View>{
      {{0, 0, 256}, {0, 0, -1}, {0, 1, 0}, {-32, -32, 256}, {32, 32, 256}},
      {{256, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {256, -32, -32}, {256, 32, 32}},
      {{0, 256, 0}, {0, -1, 0}, {0, 0, 1}, {-32, 256, -32}, {32, 256, 32}},
    };

    for (const auto& view : views)
    {
      auto camera = gl::OrthographicCamera{};
      camera.moveTo(view.position);
      camera.setDirection(view.direction, view.up);

      auto controllerImpl = BoxSelectionToolController{tool};
      auto& controller = static_cast<ToolController&>(controllerImpl);

      auto start = inputStateFor(camera, view.start, 0.0f);
      auto tracker = controller.acceptMouseDrag(start);
      REQUIRE(tracker != nullptr);

      auto end = inputStateFor(camera, view.end, 100.0f);
      tracker->end(end);

      CHECK(map.selection() == makeNodeSelection(map, {inside}));
    }
  }

  SECTION("2D controller uses drag direction")
  {
    auto camera = gl::OrthographicCamera{};
    camera.moveTo({0, 0, 256});
    camera.setDirection({0, 0, -1}, {0, 1, 0});

    auto controllerImpl = BoxSelectionToolController{tool};
    auto& controller = static_cast<ToolController&>(controllerImpl);

    SECTION("left to right contains")
    {
      auto start = inputStateFor(camera, {-32, -32, 256}, 0.0f);
      auto tracker = controller.acceptMouseDrag(start);
      REQUIRE(tracker != nullptr);

      auto end = inputStateFor(camera, {32, 32, 256}, 100.0f);
      CHECK(tracker->update(end));
      tracker->end(end);

      CHECK(map.selection() == makeNodeSelection(map, {inside}));
    }

    SECTION("right to left intersects")
    {
      auto start = inputStateFor(camera, {32, 32, 256}, 100.0f);
      auto tracker = controller.acceptMouseDrag(start);
      REQUIRE(tracker != nullptr);

      auto end = inputStateFor(camera, {-32, -32, 256}, 0.0f);
      CHECK(tracker->update(end));
      tracker->end(end);

      CHECK(map.selection() == makeNodeSelection(map, {inside, crossing}));
    }

    SECTION("modifiers add and subtract")
    {
      selectNodes(map, {outside});

      auto start = inputStateFor(camera, {-32, -32, 256}, 0.0f, ModifierKeys::CtrlCmd);
      auto tracker = controller.acceptMouseDrag(start);
      REQUIRE(tracker != nullptr);

      auto end = inputStateFor(camera, {32, 32, 256}, 100.0f, ModifierKeys::CtrlCmd);
      tracker->end(end);
      CHECK(map.selection() == makeNodeSelection(map, {outside, inside}));

      start = inputStateFor(camera, {-32, -32, 256}, 0.0f, ModifierKeys::Alt);
      tracker = controller.acceptMouseDrag(start);
      REQUIRE(tracker != nullptr);

      end = inputStateFor(camera, {32, 32, 256}, 100.0f, ModifierKeys::Alt);
      tracker->end(end);
      CHECK(map.selection() == makeNodeSelection(map, {outside}));
    }

    SECTION("cancel leaves the selection unchanged")
    {
      selectNodes(map, {outside});

      auto start = inputStateFor(camera, {-32, -32, 256}, 0.0f);
      auto tracker = controller.acceptMouseDrag(start);
      REQUIRE(tracker != nullptr);

      auto end = inputStateFor(camera, {32, 32, 256}, 100.0f);
      tracker->update(end);
      tracker->cancel();

      CHECK(map.selection() == makeNodeSelection(map, {outside}));
    }
  }

  SECTION("3D controller selects through the view depth")
  {
    auto* behind = addBrush(map, {{-16, -16, -160}, {16, 16, -128}});
    addBrush(map, {{-16, -16, 288}, {16, 16, 320}});

    auto camera = gl::PerspectiveCamera{
      90.0f, 1.0f, 4096.0f, {0, 0, 1024, 768}, {0, 0, 256}, {0, 0, -1}, {0, 1, 0}};
    auto controllerImpl = BoxSelectionToolController{tool};
    auto& controller = static_cast<ToolController&>(controllerImpl);

    SECTION("left to right contains occluded objects")
    {
      auto start = inputStateFor(camera, 450.0f, 320.0f);
      auto tracker = controller.acceptMouseDrag(start);
      REQUIRE(tracker != nullptr);

      auto end = inputStateFor(camera, 600.0f, 448.0f);
      tracker->end(end);

      CHECK_THAT(
        map.selection().nodes,
        Catch::Matchers::UnorderedEquals(std::vector<mdl::Node*>{inside, behind}));
    }

    SECTION("right to left intersects occluded objects")
    {
      auto start = inputStateFor(camera, 600.0f, 320.0f);
      auto tracker = controller.acceptMouseDrag(start);
      REQUIRE(tracker != nullptr);

      auto end = inputStateFor(camera, 450.0f, 448.0f);
      tracker->end(end);

      CHECK_THAT(
        map.selection().nodes,
        Catch::Matchers::UnorderedEquals(
          std::vector<mdl::Node*>{inside, crossing, behind}));
    }
  }
}

} // namespace tb::ui
