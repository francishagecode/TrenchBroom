/*
 Copyright (C) 2023 Kristian Duske

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

#include "kd/contracts.h"

#include <ranges>

namespace tb::ui
{

DrawShapeToolExtensionManager::DrawShapeToolExtensionManager(
  MapDocument& document, const DrawShapeToolExtensionRegistry& registry)
  : m_document{document}
{
  for (const auto& descriptor : registry.descriptors())
  {
    m_extensionInfos.push_back(
      DrawShapeToolExtensionInfo{
        .id = descriptor.id,
        .name = descriptor.name,
        .iconPath = descriptor.iconPath,
      });
    m_parameters.push_back(std::make_unique<DrawShapeToolParameters>());
    m_extensions.push_back(descriptor.factory(document));
    contract_post(m_extensions.back() != nullptr);
  }

  contract_pre(!m_extensions.empty());
  contract_post(m_parameters.size() == m_extensions.size());
  contract_post(m_extensionInfos.size() == m_extensions.size());
}

MapDocument& DrawShapeToolExtensionManager::document() const
{
  return m_document;
}

DrawShapeToolParameters& DrawShapeToolExtensionManager::parameters(const size_t index)
{
  contract_pre(index < m_parameters.size());
  return *m_parameters[index];
}

const std::vector<DrawShapeToolExtensionInfo>& DrawShapeToolExtensionManager::
  extensionInfos() const
{
  return m_extensionInfos;
}

const DrawShapeToolExtension& DrawShapeToolExtensionManager::currentExtension() const
{
  return *m_extensions[m_currentExtensionIndex];
}

const DrawShapeToolExtensionInfo& DrawShapeToolExtensionManager::currentExtensionInfo()
  const
{
  return m_extensionInfos[m_currentExtensionIndex];
}

size_t DrawShapeToolExtensionManager::currentExtensionIndex() const
{
  return m_currentExtensionIndex;
}

bool DrawShapeToolExtensionManager::setCurrentExtension(const std::string_view id)
{
  const auto it =
    std::ranges::find(m_extensionInfos, id, &DrawShapeToolExtensionInfo::id);
  return it != m_extensionInfos.end()
           ? setCurrentExtensionIndex(size_t(std::distance(m_extensionInfos.begin(), it)))
           : false;
}

bool DrawShapeToolExtensionManager::setCurrentExtensionIndex(size_t currentExtensionIndex)
{
  if (currentExtensionIndex >= m_extensions.size())
  {
    return false;
  }

  if (currentExtensionIndex != m_currentExtensionIndex)
  {
    m_currentExtensionIndex = currentExtensionIndex;
    currentExtensionDidChangeNotifier(m_currentExtensionIndex);
    return true;
  }

  return false;
}

Result<std::vector<mdl::Brush>> DrawShapeToolExtensionManager::createBrushes(
  const vm::bbox3d& bounds) const
{
  return currentExtension().createBrushes(bounds, *m_parameters[m_currentExtensionIndex]);
}

} // namespace tb::ui
