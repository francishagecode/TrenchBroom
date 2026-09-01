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

#pragma once

#include "base/Notifier.h"
#include "ui/DrawShapeToolExtension.h"
#include "ui/DrawShapeToolExtensionRegistry.h"
#include "ui/DrawShapeToolParameters.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tb::ui
{
class MapDocument;

struct DrawShapeToolExtensionInfo
{
  std::string id;
  std::string name;
  std::filesystem::path iconPath;
};

class DrawShapeToolExtensionManager
{
private:
  MapDocument& m_document;
  std::vector<DrawShapeToolExtensionInfo> m_extensionInfos;
  std::vector<std::unique_ptr<DrawShapeToolParameters>> m_parameters;
  std::vector<std::unique_ptr<DrawShapeToolExtension>> m_extensions;
  size_t m_currentExtensionIndex = 0;

public:
  Notifier<size_t> currentExtensionDidChangeNotifier;

  DrawShapeToolExtensionManager(
    MapDocument& document, const DrawShapeToolExtensionRegistry& registry);

  MapDocument& document() const;
  DrawShapeToolParameters& parameters(size_t index);

  const std::vector<DrawShapeToolExtensionInfo>& extensionInfos() const;

  const DrawShapeToolExtension& currentExtension() const;
  const DrawShapeToolExtensionInfo& currentExtensionInfo() const;
  size_t currentExtensionIndex() const;
  bool setCurrentExtension(std::string_view id);
  bool setCurrentExtensionIndex(size_t currentExtensionIndex);

  Result<std::vector<mdl::Brush>> createBrushes(const vm::bbox3d& bounds) const;
};

} // namespace tb::ui
