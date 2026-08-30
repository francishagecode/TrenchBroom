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

#pragma once

#include "ui/Tool.h"

#include "vm/bbox.h"

namespace tb::ui
{
class MapDocument;

enum class BoxSelectionMode
{
  Replace,
  Add,
  Subtract,
};

enum class BoxSelectionBoundsMode
{
  Contain,
  Intersect,
};

class BoxSelectionTool : public Tool
{
private:
  MapDocument& m_document;

public:
  explicit BoxSelectionTool(MapDocument& document);

  const vm::bbox3d& worldBounds() const;

  void select(
    const vm::bbox3d& bounds,
    BoxSelectionBoundsMode boundsMode,
    BoxSelectionMode selectionMode);
};

} // namespace tb::ui
