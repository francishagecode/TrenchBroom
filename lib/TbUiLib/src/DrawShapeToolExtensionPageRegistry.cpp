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

#include "ui/DrawShapeToolExtensionPageRegistry.h"

#include "ui/DrawShapeToolExtensionPages.h"

#include "kd/contracts.h"

#include <algorithm>
#include <ranges>
#include <utility>

namespace tb::ui
{
namespace
{

template <typename Page>
DrawShapeToolExtensionPageDescriptor makePageDescriptor(std::string extensionId)
{
  return DrawShapeToolExtensionPageDescriptor{
    .extensionId = std::move(extensionId),
    .factory = [](auto& document, auto& parameters, auto* parent)
      -> std::unique_ptr<DrawShapeToolExtensionPage> {
      return std::make_unique<Page>(document, parameters, parent);
    },
  };
}

} // namespace

DrawShapeToolExtensionPageRegistry::DrawShapeToolExtensionPageRegistry(
  std::vector<DrawShapeToolExtensionPageDescriptor> descriptors)
  : m_descriptors{std::move(descriptors)}
{
  contract_pre(std::ranges::all_of(m_descriptors, [](const auto& descriptor) {
    return !descriptor.extensionId.empty() && bool(descriptor.factory);
  }));

  for (auto i = size_t{0}; i < m_descriptors.size(); ++i)
  {
    for (auto j = i + 1u; j < m_descriptors.size(); ++j)
    {
      contract_pre(m_descriptors[i].extensionId != m_descriptors[j].extensionId);
    }
  }
}

const std::vector<DrawShapeToolExtensionPageDescriptor>&
DrawShapeToolExtensionPageRegistry::descriptors() const
{
  return m_descriptors;
}

const DrawShapeToolExtensionPageDescriptor* DrawShapeToolExtensionPageRegistry::find(
  const std::string_view extensionId) const
{
  const auto it = std::ranges::find(
    m_descriptors, extensionId, &DrawShapeToolExtensionPageDescriptor::extensionId);
  return it != m_descriptors.end() ? &*it : nullptr;
}

std::unique_ptr<DrawShapeToolExtensionPage> DrawShapeToolExtensionPageRegistry::create(
  const std::string_view extensionId,
  MapDocument& document,
  DrawShapeToolParameters& parameters,
  QWidget* parent) const
{
  const auto* descriptor = find(extensionId);
  contract_pre(descriptor != nullptr);
  auto page = descriptor->factory(document, parameters, parent);
  contract_post(page != nullptr);
  return page;
}

DrawShapeToolExtensionPageRegistry createDrawShapeToolExtensionPageRegistry()
{
  return DrawShapeToolExtensionPageRegistry{{
    {
      .extensionId = "builtin.cuboid",
      .factory =
        [](auto&, auto&, auto* parent) {
          return std::make_unique<DrawShapeToolExtensionPage>(parent);
        },
    },
    makePageDescriptor<DrawShapeToolStairsExtensionPage>("builtin.stairs"),
    makePageDescriptor<DrawShapeToolArchShapeExtensionPage>("builtin.arch"),
    makePageDescriptor<DrawShapeToolCorridorShapeExtensionPage>("builtin.corridor"),
    makePageDescriptor<DrawShapeToolCorridorBendExtensionPage>("builtin.corridor-bend"),
    makePageDescriptor<DrawShapeToolCorridorTJunctionExtensionPage>(
      "builtin.corridor-t-junction"),
    makePageDescriptor<DrawShapeToolChamberExtensionPage>("builtin.chamber"),
    makePageDescriptor<DrawShapeToolCylinderShapeExtensionPage>("builtin.cylinder"),
    makePageDescriptor<DrawShapeToolConeShapeExtensionPage>("builtin.cone"),
    makePageDescriptor<DrawShapeToolTorusShapeExtensionPage>("builtin.torus"),
    makePageDescriptor<DrawShapeToolUvSphereShapeExtensionPage>("builtin.uv-sphere"),
    makePageDescriptor<DrawShapeToolIcoSphereShapeExtensionPage>("builtin.ico-sphere"),
  }};
}

} // namespace tb::ui
