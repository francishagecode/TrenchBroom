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

#include "ui/DrawShapeToolParameters.h"

#include <algorithm>

namespace tb::ui
{

vm::axis::type DrawShapeToolParameters::axis() const
{
  return m_axis;
}

void DrawShapeToolParameters::setAxis(const vm::axis::type axis)
{
  if (axis != m_axis)
  {
    m_axis = axis;
    parametersDidChangeNotifier();
  }
}

const mdl::CircleShape& DrawShapeToolParameters::circleShape() const
{
  return m_circleShape;
}

void DrawShapeToolParameters::setCircleShape(mdl::CircleShape circleShape)
{
  if (circleShape != m_circleShape)
  {
    m_circleShape = std::move(circleShape);
    parametersDidChangeNotifier();
  }
}

bool DrawShapeToolParameters::hollow() const
{
  return m_hollow;
}

void DrawShapeToolParameters::setHollow(const bool hollow)
{
  if (hollow != m_hollow)
  {
    m_hollow = hollow;
    parametersDidChangeNotifier();
  }
}

double DrawShapeToolParameters::thickness() const
{
  return m_thickness;
}

void DrawShapeToolParameters::setThickness(const double thickness)
{
  if (thickness != m_thickness)
  {
    m_thickness = thickness;
    parametersDidChangeNotifier();
  }
}

size_t DrawShapeToolParameters::numRings() const
{
  return m_numRings;
}

void DrawShapeToolParameters::setNumRings(const size_t numRings)
{
  if (numRings != m_numRings)
  {
    m_numRings = numRings;
    parametersDidChangeNotifier();
  }
}

size_t DrawShapeToolParameters::accuracy() const
{
  return m_accuracy;
}

void DrawShapeToolParameters::setAccuracy(const size_t accuracy)
{
  if (accuracy != m_accuracy)
  {
    m_accuracy = accuracy;
    parametersDidChangeNotifier();
  }
}

double DrawShapeToolParameters::stepHeight() const
{
  return m_stepHeight;
}

void DrawShapeToolParameters::setStepHeight(const double stepHeight)
{
  if (stepHeight != m_stepHeight)
  {
    m_stepHeight = stepHeight;
    parametersDidChangeNotifier();
  }
}

DrawShapeToolParameters::StairDirection DrawShapeToolParameters::stairDirection() const
{
  return m_stairDirection;
}

void DrawShapeToolParameters::setStairDirection(const StairDirection stairDirection)
{
  if (stairDirection != m_stairDirection)
  {
    m_stairDirection = stairDirection;
    parametersDidChangeNotifier();
  }
}

vm::axis::type DrawShapeToolParameters::corridorAxis() const
{
  return m_corridorAxis;
}

void DrawShapeToolParameters::setCorridorAxis(const vm::axis::type axis)
{
  if (axis != m_corridorAxis)
  {
    m_corridorAxis = axis;
    parametersDidChangeNotifier();
  }
}

const mdl::CorridorShape& DrawShapeToolParameters::corridorShape() const
{
  return m_corridorShape;
}

void DrawShapeToolParameters::setCorridorShape(mdl::CorridorShape corridorShape)
{
  const auto junctionWidth =
    std::max(m_corridorJunctionWidth, 2.0 * corridorShape.wallThickness + 1.0);
  if (corridorShape != m_corridorShape || junctionWidth != m_corridorJunctionWidth)
  {
    m_corridorShape = std::move(corridorShape);
    m_corridorJunctionWidth = junctionWidth;
    parametersDidChangeNotifier();
  }
}

mdl::CorridorBendAngle DrawShapeToolParameters::corridorBendAngle() const
{
  return m_corridorBendAngle;
}

void DrawShapeToolParameters::setCorridorBendAngle(const mdl::CorridorBendAngle angle)
{
  if (angle != m_corridorBendAngle)
  {
    m_corridorBendAngle = angle;
    parametersDidChangeNotifier();
  }
}

mdl::CorridorBendDirection DrawShapeToolParameters::corridorBendDirection() const
{
  return m_corridorBendDirection;
}

void DrawShapeToolParameters::setCorridorBendDirection(
  const mdl::CorridorBendDirection direction)
{
  if (direction != m_corridorBendDirection)
  {
    m_corridorBendDirection = direction;
    parametersDidChangeNotifier();
  }
}

size_t DrawShapeToolParameters::corridorBendSegments() const
{
  return m_corridorBendSegments;
}

void DrawShapeToolParameters::setCorridorBendSegments(const size_t segments)
{
  if (segments != m_corridorBendSegments)
  {
    m_corridorBendSegments = segments;
    parametersDidChangeNotifier();
  }
}

double DrawShapeToolParameters::corridorJunctionWidth() const
{
  return m_corridorJunctionWidth;
}

void DrawShapeToolParameters::setCorridorJunctionWidth(const double width)
{
  const auto clampedWidth = std::max(width, 2.0 * m_corridorShape.wallThickness + 1.0);
  if (clampedWidth != m_corridorJunctionWidth)
  {
    m_corridorJunctionWidth = clampedWidth;
    parametersDidChangeNotifier();
  }
}

vm::axis::type DrawShapeToolParameters::chamberAxis() const
{
  return m_chamberAxis;
}

void DrawShapeToolParameters::setChamberAxis(const vm::axis::type axis)
{
  if (axis != m_chamberAxis)
  {
    m_chamberAxis = axis;
    parametersDidChangeNotifier();
  }
}

const mdl::ChamberShape& DrawShapeToolParameters::chamberShape() const
{
  return m_chamberShape;
}

void DrawShapeToolParameters::setChamberShape(mdl::ChamberShape chamberShape)
{
  if (chamberShape != m_chamberShape)
  {
    m_chamberShape = std::move(chamberShape);
    parametersDidChangeNotifier();
  }
}

} // namespace tb::ui
