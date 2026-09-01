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

#include "ui/DrawShapeToolExtensionPage.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class QWidget;

namespace tb::ui
{
class DrawShapeToolParameters;
class MapDocument;

using DrawShapeToolExtensionPageFactory =
  std::function<std::unique_ptr<DrawShapeToolExtensionPage>(
    MapDocument&, DrawShapeToolParameters&, QWidget*)>;

struct DrawShapeToolExtensionPageDescriptor
{
  std::string extensionId;
  DrawShapeToolExtensionPageFactory factory;
};

class DrawShapeToolExtensionPageRegistry
{
private:
  std::vector<DrawShapeToolExtensionPageDescriptor> m_descriptors;

public:
  explicit DrawShapeToolExtensionPageRegistry(
    std::vector<DrawShapeToolExtensionPageDescriptor> descriptors);

  const std::vector<DrawShapeToolExtensionPageDescriptor>& descriptors() const;

  const DrawShapeToolExtensionPageDescriptor* find(std::string_view extensionId) const;

  std::unique_ptr<DrawShapeToolExtensionPage> create(
    std::string_view extensionId,
    MapDocument& document,
    DrawShapeToolParameters& parameters,
    QWidget* parent) const;
};

DrawShapeToolExtensionPageRegistry createDrawShapeToolExtensionPageRegistry();

} // namespace tb::ui
