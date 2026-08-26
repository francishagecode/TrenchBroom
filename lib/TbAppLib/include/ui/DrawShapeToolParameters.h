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
#include "mdl/BrushBuilder.h"

namespace tb::ui
{

class DrawShapeToolParameters
{
public:
  enum class StairDirection
  {
    PosX,
    NegX,
    PosY,
    NegY,
  };

private:
  // For axis aligned shapes
  vm::axis::type m_axis = vm::axis::z;

  // For circular shapes
  mdl::CircleShape m_circleShape = mdl::EdgeAlignedCircle{8};

  // For hollow shapes
  bool m_hollow = false;
  double m_thickness = 16.0;

  // For UV sphere
  size_t m_numRings = 8;

  // For ICO sphere
  size_t m_accuracy = 1;

  // For stair shapes
  double m_stepHeight = 16.0;
  StairDirection m_stairDirection = StairDirection::PosX;

  // For corridor shapes
  vm::axis::type m_corridorAxis = vm::axis::x;
  mdl::CorridorShape m_corridorShape{
    .wallThickness = 16.0,
    .cornerRadius = 32.0,
    .cornerSegments = 2u,
    .ceilingRecessWidth = 64.0,
    .ceilingRecessDepth = 8.0,
    .sideRecessHeight = 48.0,
    .sideRecessDepth = 8.0,
  };

public:
  Notifier<> parametersDidChangeNotifier;

  vm::axis::type axis() const;
  void setAxis(vm::axis::type axis);

  const mdl::CircleShape& circleShape() const;
  void setCircleShape(mdl::CircleShape circleShape);

  bool hollow() const;
  void setHollow(bool hollow);

  double thickness() const;
  void setThickness(double thickness);

  size_t numRings() const;
  void setNumRings(size_t numRings);

  size_t accuracy() const;
  void setAccuracy(size_t accuracy);

  double stepHeight() const;
  void setStepHeight(double stepHeight);

  StairDirection stairDirection() const;
  void setStairDirection(StairDirection stairDirection);

  vm::axis::type corridorAxis() const;
  void setCorridorAxis(vm::axis::type axis);

  const mdl::CorridorShape& corridorShape() const;
  void setCorridorShape(mdl::CorridorShape corridorShape);
};

} // namespace tb::ui
