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

#include "ui/BoxSelectionTool.h"

#include "base/Logger.h"
#include "gl/Camera.h"
#include "mdl/BrushBuilder.h"
#include "mdl/BrushFace.h"
#include "mdl/BrushNode.h"
#include "mdl/EditorContext.h"
#include "mdl/Map.h"
#include "mdl/Map_Selection.h"
#include "mdl/ModelUtils.h"
#include "mdl/Node.h"
#include "mdl/Polyhedron3.h"
#include "mdl/Transaction.h"
#include "mdl/WorldNode.h"
#include "ui/MapDocument.h"

#include "kd/result.h"

#include "vm/plane.h"
#include "vm/ray.h"

#include <array>
#include <optional>
#include <unordered_set>
#include <vector>

namespace tb::ui
{
namespace
{

std::vector<mdl::Node*> collectNodes(
  mdl::Map& map,
  const mdl::BrushNode& selectionVolume,
  const BoxSelectionBoundsMode boundsMode)
{
  const auto& editorContext = map.editorContext();
  auto seen = std::unordered_set<mdl::Node*>{};
  auto result = std::vector<mdl::Node*>{};

  for (auto* indexedNode :
       map.worldNode().nodeTree().find_intersectors(selectionVolume.logicalBounds()))
  {
    auto* node = mdl::findOutermostClosedGroupOrNode(indexedNode);
    if (!seen.insert(node).second || !editorContext.selectable(*node))
    {
      continue;
    }

    const auto matches = boundsMode == BoxSelectionBoundsMode::Contain
                           ? selectionVolume.contains(*node)
                           : selectionVolume.intersects(*node);
    if (matches)
    {
      result.push_back(node);
    }
  }

  return result;
}

void applySelection(
  mdl::Map& map, const std::vector<mdl::Node*>& nodes, const BoxSelectionMode mode)
{
  auto transaction = mdl::Transaction{map, "Box Select Objects"};

  switch (mode)
  {
  case BoxSelectionMode::Replace:
    deselectAll(map);
    selectNodes(map, nodes);
    break;
  case BoxSelectionMode::Add:
    if (!nodes.empty() && map.selection().hasBrushFaces())
    {
      deselectAll(map);
    }
    selectNodes(map, nodes);
    break;
  case BoxSelectionMode::Subtract:
    deselectNodes(map, nodes);
    break;
  }

  transaction.commit();
}

std::optional<mdl::Polyhedron3> createSelectionPolyhedron(
  const gl::Camera& camera, const vm::bbox2d& screenBounds, const vm::bbox3d& worldBounds)
{
  if (screenBounds.is_empty())
  {
    return std::nullopt;
  }

  const auto worldVertices = worldBounds.vertices();
  auto polyhedron =
    mdl::Polyhedron3{std::vector<vm::vec3d>{worldVertices.begin(), worldVertices.end()}};

  const auto min = screenBounds.min;
  const auto max = screenBounds.max;
  const auto cornerRays = std::array{
    vm::ray3d{camera.pickRay(float(min.x()), float(min.y()))},
    vm::ray3d{camera.pickRay(float(max.x()), float(min.y()))},
    vm::ray3d{camera.pickRay(float(max.x()), float(max.y()))},
    vm::ray3d{camera.pickRay(float(min.x()), float(max.y()))},
  };
  const auto centerRay = vm::ray3d{
    camera.pickRay(float((min.x() + max.x()) / 2.0), float((min.y() + max.y()) / 2.0))};
  const auto centerPoint = vm::point_at_distance(centerRay, 1.0);
  const auto cameraPosition = vm::vec3d{camera.position()};

  for (size_t i = 0u; i < cornerRays.size(); ++i)
  {
    auto plane = vm::from_points(
      cameraPosition,
      vm::point_at_distance(cornerRays[i], 1.0),
      vm::point_at_distance(cornerRays[(i + 1u) % cornerRays.size()], 1.0));
    if (!plane)
    {
      return std::nullopt;
    }
    if (plane->point_distance(centerPoint) > 0.0)
    {
      plane = vm::plane3d{-plane->distance, -plane->normal};
    }
    if (polyhedron.clip(*plane).empty())
    {
      return std::nullopt;
    }
  }

  const auto nearPlane = vm::plane3d{
    cameraPosition + double(camera.nearPlane()) * vm::vec3d{camera.direction()},
    -vm::vec3d{camera.direction()}};
  if (polyhedron.clip(nearPlane).empty())
  {
    return std::nullopt;
  }

  return polyhedron;
}

} // namespace

BoxSelectionTool::BoxSelectionTool(MapDocument& document)
  : Tool{false}
  , m_document{document}
{
}

const vm::bbox3d& BoxSelectionTool::worldBounds() const
{
  return m_document.map().worldBounds();
}

void BoxSelectionTool::select(
  const vm::bbox3d& bounds,
  const BoxSelectionBoundsMode boundsMode,
  const BoxSelectionMode selectionMode)
{
  auto& map = m_document.map();
  if (bounds.is_empty())
  {
    applySelection(map, {}, selectionMode);
    return;
  }

  auto builder = mdl::BrushBuilder{map.worldNode().mapFormat(), map.worldBounds()};
  builder.createCuboid(bounds, mdl::BrushFace::NoMaterialName)
    | kdl::transform([&](auto brush) {
        const auto selectionVolume = mdl::BrushNode{std::move(brush)};
        applySelection(
          map, collectNodes(map, selectionVolume, boundsMode), selectionMode);
      })
    | kdl::transform_error([&](auto e) {
        m_document.logger().error() << "Could not create box selection volume: " << e;
      });
}

void BoxSelectionTool::select(
  const gl::Camera& camera,
  const vm::bbox2d& screenBounds,
  const BoxSelectionBoundsMode boundsMode,
  const BoxSelectionMode selectionMode)
{
  auto& map = m_document.map();
  const auto selectionPolyhedron =
    createSelectionPolyhedron(camera, screenBounds, map.worldBounds().expand(-1.0));
  if (!selectionPolyhedron)
  {
    applySelection(map, {}, selectionMode);
    return;
  }

  auto builder = mdl::BrushBuilder{map.worldNode().mapFormat(), map.worldBounds()};
  builder.createBrush(*selectionPolyhedron, mdl::BrushFace::NoMaterialName)
    | kdl::transform([&](auto brush) {
        const auto selectionVolume = mdl::BrushNode{std::move(brush)};
        applySelection(
          map, collectNodes(map, selectionVolume, boundsMode), selectionMode);
      })
    | kdl::transform_error([&](auto e) {
        m_document.logger().error()
          << "Could not create projected box selection volume: " << e;
      });
}

} // namespace tb::ui
