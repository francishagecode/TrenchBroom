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

#pragma once

#include "ui/DrawShapeToolExtension.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tb::ui
{
class MapDocument;

using DrawShapeToolExtensionFactory =
  std::function<std::unique_ptr<DrawShapeToolExtension>(MapDocument&)>;

/**
 * Describes a shape tool extension independently of its document-bound implementation.
 */
struct DrawShapeToolExtensionDescriptor
{
  static constexpr auto currentApiVersion = std::uint32_t{1};

  std::uint32_t apiVersion = currentApiVersion;
  std::string id;
  std::string name;
  std::filesystem::path iconPath;
  std::optional<std::filesystem::path> originPath;
  DrawShapeToolExtensionFactory factory;
};

class DrawShapeToolExtensionRegistry
{
private:
  std::vector<DrawShapeToolExtensionDescriptor> m_descriptors;

public:
  explicit DrawShapeToolExtensionRegistry(
    std::vector<DrawShapeToolExtensionDescriptor> descriptors);

  const std::vector<DrawShapeToolExtensionDescriptor>& descriptors() const;

  const DrawShapeToolExtensionDescriptor* find(std::string_view id) const;
};

/** Creates the complete registry of built-in native shape generators. */
DrawShapeToolExtensionRegistry createDrawShapeToolExtensionRegistry();

} // namespace tb::ui
