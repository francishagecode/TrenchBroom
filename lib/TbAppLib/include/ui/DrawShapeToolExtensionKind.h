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

#include <array>

namespace tb::ui
{

/**
 * Legacy shape ordering retained temporarily for downstream code. The Simple Shape Tool
 * runtime no longer includes this header; registry descriptor order is authoritative.
 */
enum class DrawShapeToolExtensionKind
{
  Cuboid,
  Stairs,
  Arch,
  Corridor,
  CorridorBend,
  CorridorTJunction,
  Chamber,
  Cylinder,
  Cone,
  Torus,
  UvSphere,
  IcoSphere,
};

inline constexpr auto DrawShapeToolExtensionKinds = std::array{
  DrawShapeToolExtensionKind::Cuboid,
  DrawShapeToolExtensionKind::Stairs,
  DrawShapeToolExtensionKind::Arch,
  DrawShapeToolExtensionKind::Corridor,
  DrawShapeToolExtensionKind::CorridorBend,
  DrawShapeToolExtensionKind::CorridorTJunction,
  DrawShapeToolExtensionKind::Chamber,
  DrawShapeToolExtensionKind::Cylinder,
  DrawShapeToolExtensionKind::Cone,
  DrawShapeToolExtensionKind::Torus,
  DrawShapeToolExtensionKind::UvSphere,
  DrawShapeToolExtensionKind::IcoSphere,
};

} // namespace tb::ui
