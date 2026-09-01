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

#include "ui/DrawShapeToolExtensionRegistry.h"

#include "ui/DrawShapeToolExtensions.h"

#include "kd/contracts.h"

#include <algorithm>
#include <ranges>
#include <utility>

namespace tb::ui
{
namespace
{

template <typename Extension>
DrawShapeToolExtensionDescriptor makeBuiltinDescriptor(
  std::string id, std::string name, std::filesystem::path iconPath)
{
  return DrawShapeToolExtensionDescriptor{
    .apiVersion = DrawShapeToolExtensionDescriptor::currentApiVersion,
    .id = std::move(id),
    .name = std::move(name),
    .iconPath = std::move(iconPath),
    .originPath = std::nullopt,
    .factory = [](auto& document) -> std::unique_ptr<DrawShapeToolExtension> {
      return std::make_unique<Extension>(document);
    },
  };
}

} // namespace

DrawShapeToolExtensionRegistry::DrawShapeToolExtensionRegistry(
  std::vector<DrawShapeToolExtensionDescriptor> descriptors)
  : m_descriptors{std::move(descriptors)}
{
  contract_pre(std::ranges::all_of(m_descriptors, [](const auto& descriptor) {
    return descriptor.apiVersion == DrawShapeToolExtensionDescriptor::currentApiVersion
           && !descriptor.id.empty() && !descriptor.name.empty()
           && !descriptor.iconPath.empty() && bool(descriptor.factory);
  }));

  for (auto i = size_t{0}; i < m_descriptors.size(); ++i)
  {
    for (auto j = i + 1u; j < m_descriptors.size(); ++j)
    {
      contract_pre(m_descriptors[i].id != m_descriptors[j].id);
    }
  }
}

const std::vector<DrawShapeToolExtensionDescriptor>& DrawShapeToolExtensionRegistry::
  descriptors() const
{
  return m_descriptors;
}

const DrawShapeToolExtensionDescriptor* DrawShapeToolExtensionRegistry::find(
  const std::string_view id) const
{
  const auto it =
    std::ranges::find(m_descriptors, id, &DrawShapeToolExtensionDescriptor::id);
  return it != m_descriptors.end() ? &*it : nullptr;
}

DrawShapeToolExtensionRegistry createDrawShapeToolExtensionRegistry()
{
  return DrawShapeToolExtensionRegistry{{
    makeBuiltinDescriptor<DrawShapeToolCuboidExtension>(
      "builtin.cuboid", "Cuboid", "ShapeTool_Cuboid.svg"),
    makeBuiltinDescriptor<DrawShapeToolStairsExtension>(
      "builtin.stairs", "Stairs", "ShapeTool_Stairs.svg"),
    makeBuiltinDescriptor<DrawShapeToolArchExtension>(
      "builtin.arch", "Arch", "ShapeTool_Arch.svg"),
    makeBuiltinDescriptor<DrawShapeToolCorridorExtension>(
      "builtin.corridor", "Corridor", "ShapeTool_Corridor.svg"),
    makeBuiltinDescriptor<DrawShapeToolCorridorBendExtension>(
      "builtin.corridor-bend", "Corridor Bend", "ShapeTool_CorridorBend.svg"),
    makeBuiltinDescriptor<DrawShapeToolCorridorTJunctionExtension>(
      "builtin.corridor-t-junction", "Corridor T", "ShapeTool_CorridorT.svg"),
    makeBuiltinDescriptor<DrawShapeToolChamberExtension>(
      "builtin.chamber", "Chamber", "ShapeTool_Chamber.svg"),
    makeBuiltinDescriptor<DrawShapeToolCylinderExtension>(
      "builtin.cylinder", "Cylinder", "ShapeTool_Cylinder.svg"),
    makeBuiltinDescriptor<DrawShapeToolConeExtension>(
      "builtin.cone", "Cone", "ShapeTool_Cone.svg"),
    makeBuiltinDescriptor<DrawShapeToolTorusExtension>(
      "builtin.torus", "Torus", "ShapeTool_Torus.svg"),
    makeBuiltinDescriptor<DrawShapeToolUvSphereExtension>(
      "builtin.uv-sphere", "Spheroid (UV)", "ShapeTool_UVSphere.svg"),
    makeBuiltinDescriptor<DrawShapeToolIcoSphereExtension>(
      "builtin.ico-sphere", "Spheroid (Icosahedron)", "ShapeTool_IcoSphere.svg"),
  }};
}

} // namespace tb::ui
