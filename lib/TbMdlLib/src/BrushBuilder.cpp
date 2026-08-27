/*
 Copyright (C) 2010 Kristian Duske

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

#include "mdl/BrushBuilder.h"

#include "mdl/BasicShapes.h"
#include "mdl/Brush.h"
#include "mdl/BrushFace.h"

#include "kd/contracts.h"
#include "kd/range_utils.h"
#include "kd/ranges/concat_view.h"
#include "kd/ranges/to.h"
#include "kd/result.h"
#include "kd/result_fold.h"
#include "kd/vector_utils.h"

#include "vm/intersection.h"
#include "vm/line.h"
#include "vm/mat.h"
#include "vm/mat_ext.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <utility>

namespace tb::mdl
{

BrushBuilder::BrushBuilder(const MapFormat mapFormat, const vm::bbox3d& worldBounds)
  : m_mapFormat{mapFormat}
  , m_worldBounds{worldBounds}
  , m_defaultUvAttributes{}
  , m_defaultSurfaceAttributes{}
{
}

BrushBuilder::BrushBuilder(
  const MapFormat mapFormat,
  const vm::bbox3d& worldBounds,
  UvAttributes defaultUvAttributes,
  SurfaceAttributes defaultSurfaceAttributes)
  : m_mapFormat{mapFormat}
  , m_worldBounds{worldBounds}
  , m_defaultUvAttributes{std::move(defaultUvAttributes)}
  , m_defaultSurfaceAttributes{std::move(defaultSurfaceAttributes)}
{
}

Result<Brush> BrushBuilder::createCube(
  const double size, const std::string& materialName) const
{
  return createCuboid(
    vm::bbox3d{size / 2.0},
    materialName,
    materialName,
    materialName,
    materialName,
    materialName,
    materialName);
}

Result<Brush> BrushBuilder::createCube(
  double size,
  const std::string& leftMaterial,
  const std::string& rightMaterial,
  const std::string& frontMaterial,
  const std::string& backMaterial,
  const std::string& topMaterial,
  const std::string& bottomMaterial) const
{
  return createCuboid(
    vm::bbox3d{size / 2.0},
    leftMaterial,
    rightMaterial,
    frontMaterial,
    backMaterial,
    topMaterial,
    bottomMaterial);
}

Result<Brush> BrushBuilder::createCuboid(
  const vm::vec3d& size, const std::string& materialName) const
{
  return createCuboid(
    vm::bbox3d{-size / 2.0, size / 2.0},
    materialName,
    materialName,
    materialName,
    materialName,
    materialName,
    materialName);
}

Result<Brush> BrushBuilder::createCuboid(
  const vm::vec3d& size,
  const std::string& leftMaterial,
  const std::string& rightMaterial,
  const std::string& frontMaterial,
  const std::string& backMaterial,
  const std::string& topMaterial,
  const std::string& bottomMaterial) const
{
  return createCuboid(
    vm::bbox3d{-size / 2.0, size / 2.0},
    leftMaterial,
    rightMaterial,
    frontMaterial,
    backMaterial,
    topMaterial,
    bottomMaterial);
}

Result<Brush> BrushBuilder::createCuboid(
  const vm::bbox3d& bounds, const std::string& materialName) const
{
  return createCuboid(
    bounds,
    materialName,
    materialName,
    materialName,
    materialName,
    materialName,
    materialName);
}

Result<Brush> BrushBuilder::createCuboid(
  const vm::bbox3d& bounds,
  const std::string& leftMaterial,
  const std::string& rightMaterial,
  const std::string& frontMaterial,
  const std::string& backMaterial,
  const std::string& topMaterial,
  const std::string& bottomMaterial) const
{
  return std::vector{
           BrushFace::create(
             bounds.min,
             bounds.min + vm::vec3d{0, 1, 0},
             bounds.min + vm::vec3d{0, 0, 1},
             leftMaterial,
             m_defaultUvAttributes,
             m_defaultSurfaceAttributes,
             m_mapFormat), // left
           BrushFace::create(
             bounds.max,
             bounds.max + vm::vec3d{0, 0, 1},
             bounds.max + vm::vec3d{0, 1, 0},
             rightMaterial,
             m_defaultUvAttributes,
             m_defaultSurfaceAttributes,
             m_mapFormat), // right
           BrushFace::create(
             bounds.min,
             bounds.min + vm::vec3d{0, 0, 1},
             bounds.min + vm::vec3d{1, 0, 0},
             frontMaterial,
             m_defaultUvAttributes,
             m_defaultSurfaceAttributes,
             m_mapFormat), // front
           BrushFace::create(
             bounds.max,
             bounds.max + vm::vec3d{1, 0, 0},
             bounds.max + vm::vec3d{0, 0, 1},
             backMaterial,
             m_defaultUvAttributes,
             m_defaultSurfaceAttributes,
             m_mapFormat), // back
           BrushFace::create(
             bounds.max,
             bounds.max + vm::vec3d{0, 1, 0},
             bounds.max + vm::vec3d{1, 0, 0},
             topMaterial,
             m_defaultUvAttributes,
             m_defaultSurfaceAttributes,
             m_mapFormat), // top
           BrushFace::create(
             bounds.min,
             bounds.min + vm::vec3d{1, 0, 0},
             bounds.min + vm::vec3d{0, 1, 0},
             bottomMaterial,
             m_defaultUvAttributes,
             m_defaultSurfaceAttributes,
             m_mapFormat), // bottom
         }
         | kdl::fold | kdl::and_then([&](auto faces) {
             return Brush::create(m_worldBounds, std::move(faces));
           });
}

namespace
{
auto makeEdgeAlignedCircle(const size_t numSides, const vm::bbox2d& bounds)
{
  contract_pre(numSides > 2);

  const auto transform = vm::translation_matrix(bounds.min)
                         * vm::scaling_matrix(bounds.size())
                         * vm::translation_matrix(vm::vec2d{0.5, 0.5})
                         * vm::scaling_matrix(vm::vec2d{0.5, 0.5});

  auto vertices = std::vector<vm::vec2d>{};
  for (size_t i = 0; i < numSides; ++i)
  {
    const auto angle =
      (double(i) + 0.5) * vm::Cd::two_pi() / double(numSides) - vm::Cd::half_pi();
    const auto a = vm::Cd::pi() / double(numSides); // Half angle
    const auto ca = std::cos(a);
    const auto x = std::cos(angle) / ca;
    const auto y = std::sin(angle) / ca;
    vertices.emplace_back(x, y);
  }
  return transform * vertices;
}

auto makeVertexAlignedCircle(const size_t numSides, const vm::bbox2d& bounds)
{
  contract_pre(numSides > 2);

  const auto transform = vm::translation_matrix(bounds.min)
                         * vm::scaling_matrix(bounds.size())
                         * vm::translation_matrix(vm::vec2d{0.5, 0.5})
                         * vm::scaling_matrix(vm::vec2d{0.5, 0.5});

  auto vertices = std::vector<vm::vec2d>{};
  for (size_t i = 0; i < numSides; ++i)
  {
    const auto angle =
      double(i) * vm::Cd::two_pi() / double(numSides) - vm::Cd::half_pi();
    const auto x = std::cos(angle);
    const auto y = std::sin(angle);
    vertices.emplace_back(x, y);
  }
  return transform * vertices;
}

auto makeScalableCircle(const size_t precision, const vm::bbox2d& bounds)
{
  auto vertices = std::vector<vm::vec2d>{
    {-0.25, +1.00},
    {-0.75, +0.75},
    {-1.00, +0.25},
    {-1.00, -0.25},
    {-0.75, -0.75},
    {-0.25, -1.00},
    {+0.25, -1.00},
    {+0.75, -0.75},
    {+1.00, -0.25},
    {+1.00, +0.25},
    {+0.75, +0.75},
    {+0.25, +1.00},
  };

  // Clip off each corner to get a scalable unit circle with double the vertices
  for (size_t i = 0; i < precision; ++i)
  {

    const auto previousVertices = std::exchange(vertices, std::vector<vm::vec2d>{});
    const auto count = previousVertices.size();
    for (size_t j = 0; j < previousVertices.size(); ++j)
    {
      const auto prev = previousVertices[(j + count - 1) % count];
      const auto cur = previousVertices[j];
      const auto next = previousVertices[(j + 1) % count];

      vertices.push_back(prev + (cur - prev) * 0.75);
      vertices.push_back(cur + (next - cur) * 0.25);
    }
  }

  const auto size = bounds.size();
  const auto minSize = vm::min(size.x(), size.y());
  const auto squareSize = vm::vec2d::fill(minSize);

  vertices = vm::scaling_matrix(squareSize) * vm::translation_matrix(vm::vec2d{0.5, 0.5})
             * vm::scaling_matrix(vm::vec2d{0.5, 0.5}) * vertices;

  // Stretch the circle to fit the bounds by moving the right half and the top half
  // instead of uniformly scaling all vertices
  const auto offset = vm::vec2d{
    vm::max(size.x() - size.y(), 0.0),
    vm::max(size.y() - size.x(), 0.0),
  };

  for (auto& v : vertices)
  {
    if (v.x() > minSize / 2.0)
    {
      v = vm::vec2d{v.x() + offset.x(), v.y()};
    }
    if (v.y() > minSize / 2.0)
    {
      v = vm::vec2d{v.x(), v.y() + offset.y()};
    }
  }

  return vm::translation_matrix(bounds.min) * vertices;
}

auto makeCircle(const CircleShape& circleShape, const vm::bbox2d& bounds)
{

  return std::visit(
    kdl::overload(
      [&](const EdgeAlignedCircle& edgeAligned) {
        return makeEdgeAlignedCircle(edgeAligned.numSides, bounds);
      },
      [&](const VertexAlignedCircle& vertexAligned) {
        return makeVertexAlignedCircle(vertexAligned.numSides, bounds);
      },
      [&](const ScalableCircle& scalable) {
        return makeScalableCircle(scalable.precision, bounds);
      }),
    circleShape);
}

auto makeCylinder(const CircleShape& circleShape, const vm::bbox3d& boundsXY)
{
  auto vertices = std::vector<vm::vec3d>{};
  for (const auto& v : makeCircle(circleShape, boundsXY.xy()))
  {
    vertices.emplace_back(v.x(), v.y(), boundsXY.min.z());
    vertices.emplace_back(v.x(), v.y(), boundsXY.max.z());
  }
  return vertices;
}

} // namespace

Result<Brush> BrushBuilder::createCylinder(
  const vm::bbox3d& bounds,
  const CircleShape& circleShape,
  const vm::axis::type axis,
  const std::string& textureName) const
{
  const auto toXY = vm::rotation_matrix(vm::vec3d::axis(axis), vm::vec3d{0, 0, 1});
  const auto fromXY = vm::rotation_matrix(vm::vec3d{0, 0, 1}, vm::vec3d::axis(axis));

  const auto cylinder = makeCylinder(circleShape, bounds.transform(toXY));
  return createBrush(fromXY * cylinder, textureName);
}

namespace
{

auto makeVerticesForWedges(
  const std::vector<vm::vec2d>& outerCircle, const vm::bbox2d& bounds)
{
  // The bounds are too small to create an inner circle, but we can still create
  // wedges for the hollow cylinder.
  // Generate four points (here called corners) where the wedges should meet.
  // If the bounds are square, all corners coincide. If the bounds are
  // rectangular, two pairs of corners coincide.
  // Then map each vertex of the outer circle to the closest corner.
  const auto offset = vm::min(bounds.size().x(), bounds.size().y()) / 2.0;
  const auto corners = std::vector<vm::vec2d>{
    {bounds.min.x() + offset, bounds.min.y() + offset},
    {bounds.min.x() + offset, bounds.max.y() - offset},
    {bounds.max.x() - offset, bounds.min.y() + offset},
    {bounds.max.x() - offset, bounds.max.y() - offset},
  };
  return outerCircle | std::views::transform([&](const auto& v) {
           return *std::ranges::min_element(corners, [&](const auto& a, const auto& b) {
             return vm::squared_distance(v, a) < vm::squared_distance(v, b);
           });
         })
         | kdl::ranges::to<std::vector>();
}

auto makeHollowCylinderInnerCircle(
  const std::vector<vm::vec2d>& outerCircle,
  const double thickness,
  const CircleShape& circleShape,
  const vm::bbox2d& bounds)
{
  if (bounds.size().x() <= thickness * 2.0 || bounds.size().y() <= thickness * 2.0)
  {
    return Result<std::vector<vm::vec2d>>{makeVerticesForWedges(outerCircle, bounds)};
  }

  return std::visit(
    kdl::overload(
      [&](const ScalableCircle& scalable) -> Result<std::vector<vm::vec2d>> {
        const auto delta = vm::vec2d{thickness, thickness};
        const auto innerBounds = vm::bbox2d{bounds.min + delta, bounds.max - delta};
        return makeScalableCircle(scalable.precision, innerBounds);
      },
      [&](const auto& axisOrVertexAligned) -> Result<std::vector<vm::vec2d>> {
        const auto numSides = axisOrVertexAligned.numSides;
        auto outerLines = std::vector<vm::line2d>{};
        outerLines.reserve(numSides);
        for (size_t i = 0; i < numSides; ++i)
        {
          const auto p1 = outerCircle[i];
          const auto p2 = outerCircle[(i + 1) % numSides];
          outerLines.emplace_back(p1, vm::normalize(p2 - p1));
        }

        const auto innerLines =
          outerLines | std::views::transform([&](const auto& l) {
            const auto offsetDir = vm::vec2d{-l.direction.y(), l.direction.x()};
            return vm::line2d{l.point + offsetDir * thickness, l.direction};
          })
          | kdl::ranges::to<std::vector>();

        auto innerCircle = std::vector<vm::vec2d>{};
        innerCircle.reserve(numSides);
        for (size_t i = 0; i < numSides; ++i)
        {
          const auto l1 = innerLines[(i + numSides - 1) % numSides];
          const auto l2 = innerLines[i];
          const auto d = vm::intersect_line_line(l1, l2);
          if (!d)
          {
            return Error{"Failed to intersect lines"};
          }

          innerCircle.push_back(vm::point_at_distance(l1, *d));
        }

        return innerCircle;
      }),
    circleShape);
}

auto makeHollowCylinderFragmentVertices(
  const std::vector<vm::vec2d>& outerCircle,
  const std::vector<vm::vec2d>& innerCircle,
  const size_t i,
  const vm::bbox3d& boundsXY)
{
  contract_pre(outerCircle.size() == innerCircle.size());

  const auto numSides = outerCircle.size();

  const auto po = outerCircle[(i + 0) % numSides];
  const auto pi = innerCircle[(i + 0) % numSides];
  const auto no = outerCircle[(i + 1) % numSides];
  const auto ni = innerCircle[(i + 1) % numSides];

  const auto brushVertices = std::vector<vm::vec3d>{
    {po, boundsXY.min.z()},
    {po, boundsXY.max.z()},
    {pi, boundsXY.min.z()},
    {pi, boundsXY.max.z()},
    {no, boundsXY.min.z()},
    {no, boundsXY.max.z()},
    {ni, boundsXY.min.z()},
    {ni, boundsXY.max.z()},
  };

  return brushVertices;
}

} // namespace

Result<std::vector<Brush>> BrushBuilder::createHollowCylinder(
  const vm::bbox3d& bounds,
  const double thickness,
  const CircleShape& circleShape,
  const vm::axis::type axis,
  const std::string& textureName) const
{
  const auto toXY = vm::rotation_matrix(vm::vec3d::axis(axis), vm::vec3d{0, 0, 1});
  const auto fromXY = vm::rotation_matrix(vm::vec3d{0, 0, 1}, vm::vec3d::axis(axis));
  const auto boundsXY = bounds.transform(toXY);

  const auto outerCircle = makeCircle(circleShape, boundsXY.xy());

  return makeHollowCylinderInnerCircle(outerCircle, thickness, circleShape, boundsXY.xy())
    .and_then([&](const auto& innerCircle) {
      contract_assert(innerCircle.size() == outerCircle.size());

      const auto numFragments = outerCircle.size();

      auto brushes = std::vector<Result<Brush>>{};
      brushes.reserve(numFragments);

      for (size_t i = 0; i < numFragments; ++i)
      {
        const auto fragmentVertices =
          makeHollowCylinderFragmentVertices(outerCircle, innerCircle, i, boundsXY);
        const auto rotatedFragmentVertices = fromXY * fragmentVertices;

        brushes.push_back(createBrush(rotatedFragmentVertices, textureName));
      }

      return brushes | kdl::fold;
    });
}

namespace
{

// Maps the tunnel (extrusion) axis to the span and vertical axes of the arch's
// cross-section. Arches rise along world Z where possible so they stand upright.
struct UprightAxes
{
  size_t tunnel;
  size_t span;
  size_t vertical;
};

UprightAxes uprightAxes(const vm::axis::type axis)
{
  switch (axis)
  {
  case vm::axis::x:
    return {vm::axis::x, vm::axis::y, vm::axis::z};
  case vm::axis::y:
    return {vm::axis::y, vm::axis::x, vm::axis::z};
  default: // vm::axis::z
    return {vm::axis::z, vm::axis::x, vm::axis::y};
  }
}

// Interpolates the point on segment a->b at vertical coordinate v.
vm::vec2d crossingAtV(const vm::vec2d& a, const vm::vec2d& b, const double v)
{
  const auto t = (v - a.y()) / (b.y() - a.y());
  return vm::vec2d{a.x() + (b.x() - a.x()) * t, v};
}

} // namespace

Result<std::vector<Brush>> BrushBuilder::createArch(
  const vm::bbox3d& bounds,
  const double thickness,
  const CircleShape& circleShape,
  const vm::axis::type axis,
  const std::string& textureName) const
{
  const auto axes = uprightAxes(axis);

  const auto sMin = bounds.min[axes.span];
  const auto sMax = bounds.max[axes.span];
  const auto vMin = bounds.min[axes.vertical];
  const auto vMax = bounds.max[axes.vertical];
  const auto wMin = bounds.min[axes.tunnel];
  const auto wMax = bounds.max[axes.tunnel];
  const auto height = vMax - vMin;
  const auto span = sMax - sMin;

  // The bounds are often degenerate mid-drag; emit no brushes rather than erroring.
  if (height <= 0.0 || span <= 0.0)
  {
    return Result<std::vector<Brush>>{std::vector<Brush>{}};
  }

  // Upper half of an ellipse whose diameter lies on the springing line (v == vMin). Build
  // the full ellipse in a bounds doubled downwards, then keep only the upper half,
  // reusing the cylinder circle-mode machinery.
  const auto circleBounds = vm::bbox2d{{sMin, vMin - height}, {sMax, vMax}};

  const auto outer = makeCircle(circleShape, circleBounds);

  return makeHollowCylinderInnerCircle(outer, thickness, circleShape, circleBounds)
         | kdl::transform([&](const auto& inner) {
             contract_assert(inner.size() == outer.size());
             const auto n = outer.size();

             const auto isUpper = [&](const size_t i) { return outer[i].y() >= vMin; };

             // Start of the contiguous upper run: an upper vertex whose predecessor is
             // below.
             const auto firstUpper = kdl::index_of(
               std::views::iota(0u, n),
               [&](const auto i) { return isUpper(i) && !isUpper((i + n - 1) % n); });

             if (!firstUpper)
             {
               return std::vector<Brush>{};
             }

             const auto upper =
               std::views::iota(0u, n) | std::views::transform([&](const auto i) {
                 return (*firstUpper + i) % n;
               })
               | std::views::take_while(isUpper) | kdl::ranges::to<std::vector>();

             // Cap both ends with a foot on the springing line so the arch sits flat.
             const auto first = upper.front();
             const auto last = upper.back();
             const auto beforeFirst = (first + n - 1) % n;
             const auto afterLast = (last + 1) % n;

             const auto clampToSpring = [&](const auto p) {
               return vm::vec2d{p.x(), std::max(p.y(), vMin)};
             };

             auto outerBoundary = std::vector<vm::vec2d>{};
             auto innerBoundary = std::vector<vm::vec2d>{};
             outerBoundary.push_back(crossingAtV(outer[beforeFirst], outer[first], vMin));
             innerBoundary.push_back(crossingAtV(inner[beforeFirst], inner[first], vMin));
             for (const auto i : upper)
             {
               outerBoundary.push_back(outer[i]);
               innerBoundary.push_back(clampToSpring(inner[i]));
             }
             outerBoundary.push_back(crossingAtV(outer[last], outer[afterLast], vMin));
             innerBoundary.push_back(crossingAtV(inner[last], inner[afterLast], vMin));

             const auto toPoint = [&](const vm::vec2d& p, const double w) {
               auto result = vm::vec3d{};
               result[axes.span] = p.x();
               result[axes.vertical] = p.y();
               result[axes.tunnel] = w;
               return result;
             };

             // Build each voussoir independently, skipping any that are degenerate
             // mid-drag.
             return std::views::iota(0u, outerBoundary.size() - 1)
                    | std::views::transform([&](const auto j) {
                        const auto& o0 = outerBoundary[j];
                        const auto& o1 = outerBoundary[j + 1];
                        const auto& i0 = innerBoundary[j];
                        const auto& i1 = innerBoundary[j + 1];

                        const auto vertices = std::vector{
                          toPoint(o0, wMin),
                          toPoint(o0, wMax),
                          toPoint(o1, wMin),
                          toPoint(o1, wMax),
                          toPoint(i0, wMin),
                          toPoint(i0, wMax),
                          toPoint(i1, wMin),
                          toPoint(i1, wMax),
                        };

                        return createBrush(vertices, textureName);
                      })
                    | kdl::values();
           });
}

namespace
{

struct CorridorBoundaryPoint
{
  vm::vec2d outer;
  vm::vec2d inner;
};

vm::vec2d roundedRectPoint(
  const vm::vec2d& center,
  const vm::vec2d& halfSize,
  const double radius,
  const vm::vec2d& corner,
  const double angle)
{
  const auto cornerCenter = center + corner * (halfSize - vm::vec2d::fill(radius));
  return cornerCenter + vm::vec2d{std::cos(angle), std::sin(angle)} * radius;
}

std::vector<CorridorBoundaryPoint> makeCorridorBoundary(
  const vm::bbox2d& bounds, const CorridorShape& shape, const double radius)
{
  const auto center = bounds.center();
  const auto outerHalfSize = bounds.size() / 2.0;
  const auto innerHalfSize = outerHalfSize - vm::vec2d::fill(shape.wallThickness);
  const auto innerRadius = std::max(radius - shape.wallThickness, 0.0);

  const auto point = [&](const vm::vec2d& corner, const double angle) {
    return CorridorBoundaryPoint{
      roundedRectPoint(center, outerHalfSize, radius, corner, angle),
      roundedRectPoint(center, innerHalfSize, innerRadius, corner, angle),
    };
  };

  auto boundary = std::vector<CorridorBoundaryPoint>{};
  boundary.reserve(
    4u * shape.cornerSegments + 4u + (shape.ceilingRecessDepth > 0.0 ? 4u : 0u)
    + (shape.sideRecessDepth > 0.0 ? 8u : 0u));

  const auto append = [&](const vm::vec2d& outer, const vm::vec2d& inner) {
    boundary.push_back({outer, inner});
  };
  const auto appendCorner =
    [&](const vm::vec2d& corner, const double startAngle, const bool includeEnd) {
      const auto end = includeEnd ? shape.cornerSegments : shape.cornerSegments - 1u;
      for (size_t i = 1u; i <= end; ++i)
      {
        const auto angle =
          startAngle + vm::Cd::half_pi() * double(i) / double(shape.cornerSegments);
        boundary.push_back(point(corner, angle));
      }
    };

  // Start at the upper-right tangent and walk counter-clockwise around the shell.
  boundary.push_back(point({1.0, 1.0}, vm::Cd::half_pi()));

  if (shape.ceilingRecessDepth > 0.0)
  {
    const auto halfWidth = shape.ceilingRecessWidth / 2.0;
    const auto transitionWidth =
      std::min(shape.wallThickness, innerHalfSize.x() - innerRadius - halfWidth);
    const auto outerY = bounds.max.y();
    const auto innerY = bounds.max.y() - shape.wallThickness;
    append(
      {center.x() + halfWidth + transitionWidth, outerY},
      {center.x() + halfWidth, innerY});
    append(
      {center.x() + halfWidth, outerY},
      {center.x() + halfWidth, innerY + shape.ceilingRecessDepth});
    append(
      {center.x() - halfWidth, outerY},
      {center.x() - halfWidth, innerY + shape.ceilingRecessDepth});
    append(
      {center.x() - halfWidth - transitionWidth, outerY},
      {center.x() - halfWidth, innerY});
  }

  boundary.push_back(point({-1.0, 1.0}, vm::Cd::half_pi()));
  appendCorner({-1.0, 1.0}, vm::Cd::half_pi(), true);

  if (shape.sideRecessDepth > 0.0)
  {
    const auto halfHeight = shape.sideRecessHeight / 2.0;
    const auto transitionHeight =
      std::min(shape.wallThickness, innerHalfSize.y() - innerRadius - halfHeight);
    const auto outerX = bounds.min.x();
    const auto innerX = bounds.min.x() + shape.wallThickness;
    append(
      {outerX, center.y() + halfHeight + transitionHeight},
      {innerX, center.y() + halfHeight});
    append(
      {outerX, center.y() + halfHeight},
      {innerX - shape.sideRecessDepth, center.y() + halfHeight});
    append(
      {outerX, center.y() - halfHeight},
      {innerX - shape.sideRecessDepth, center.y() - halfHeight});
    append(
      {outerX, center.y() - halfHeight - transitionHeight},
      {innerX, center.y() - halfHeight});
  }

  boundary.push_back(point({-1.0, -1.0}, vm::Cd::pi()));
  appendCorner({-1.0, -1.0}, vm::Cd::pi(), true);

  boundary.push_back(point({1.0, -1.0}, 3.0 * vm::Cd::half_pi()));
  appendCorner({1.0, -1.0}, 3.0 * vm::Cd::half_pi(), true);

  if (shape.sideRecessDepth > 0.0)
  {
    const auto halfHeight = shape.sideRecessHeight / 2.0;
    const auto transitionHeight =
      std::min(shape.wallThickness, innerHalfSize.y() - innerRadius - halfHeight);
    const auto outerX = bounds.max.x();
    const auto innerX = bounds.max.x() - shape.wallThickness;
    append(
      {outerX, center.y() - halfHeight - transitionHeight},
      {innerX, center.y() - halfHeight});
    append(
      {outerX, center.y() - halfHeight},
      {innerX + shape.sideRecessDepth, center.y() - halfHeight});
    append(
      {outerX, center.y() + halfHeight},
      {innerX + shape.sideRecessDepth, center.y() + halfHeight});
    append(
      {outerX, center.y() + halfHeight + transitionHeight},
      {innerX, center.y() + halfHeight});
  }

  boundary.push_back(point({1.0, 1.0}, 0.0));
  appendCorner({1.0, 1.0}, 0.0, false);

  auto result = std::vector<CorridorBoundaryPoint>{};
  result.reserve(boundary.size());
  for (const auto& boundaryPoint : boundary)
  {
    if (
      result.empty() || boundaryPoint.outer != result.back().outer
      || boundaryPoint.inner != result.back().inner)
    {
      result.push_back(boundaryPoint);
    }
  }
  if (
    result.size() > 1u && result.front().outer == result.back().outer
    && result.front().inner == result.back().inner)
  {
    result.pop_back();
  }

  return result;
}

std::optional<std::string> corridorShapeError(const CorridorShape& corridorShape)
{
  if (corridorShape.wallThickness <= 0.0)
  {
    return "Corridor wall thickness must be greater than zero";
  }
  if (corridorShape.cornerRadius <= 0.0)
  {
    return "Corridor corner radius must be greater than zero";
  }
  if (corridorShape.cornerSegments == 0u)
  {
    return "Corridor corner segments must be greater than zero";
  }
  if (
    corridorShape.ceilingRecessDepth < 0.0
    || corridorShape.ceilingRecessDepth >= corridorShape.wallThickness)
  {
    return "Corridor ceiling recess depth must be non-negative and less than wall "
           "thickness";
  }
  if (corridorShape.ceilingRecessDepth > 0.0 && corridorShape.ceilingRecessWidth <= 0.0)
  {
    return "Corridor ceiling recess width must be greater than zero";
  }
  if (
    corridorShape.sideRecessDepth < 0.0
    || corridorShape.sideRecessDepth >= corridorShape.wallThickness)
  {
    return "Corridor side recess depth must be non-negative and less than wall thickness";
  }
  if (corridorShape.sideRecessDepth > 0.0 && corridorShape.sideRecessHeight <= 0.0)
  {
    return "Corridor side recess height must be greater than zero";
  }

  return std::nullopt;
}

bool corridorShapeFits(
  const CorridorShape& corridorShape,
  const double width,
  const double height,
  const double radius)
{
  const auto innerRadius = std::max(radius - corridorShape.wallThickness, 0.0);
  const auto innerTopHalfWidth = width / 2.0 - corridorShape.wallThickness - innerRadius;
  const auto innerSideHalfHeight =
    height / 2.0 - corridorShape.wallThickness - innerRadius;

  return !(
    (corridorShape.ceilingRecessDepth > 0.0
     && corridorShape.ceilingRecessWidth >= 2.0 * innerTopHalfWidth)
    || (corridorShape.sideRecessDepth > 0.0 && corridorShape.sideRecessHeight >= 2.0 * innerSideHalfHeight));
}

} // namespace

Result<std::vector<Brush>> BrushBuilder::createCorridor(
  const vm::bbox3d& bounds,
  const CorridorShape& corridorShape,
  const vm::axis::type axis,
  const std::string& textureName) const
{
  if (const auto error = corridorShapeError(corridorShape))
  {
    return Error{*error};
  }

  const auto axes = uprightAxes(axis);
  const auto width = bounds.size()[axes.span];
  const auto height = bounds.size()[axes.vertical];
  const auto depth = bounds.size()[axes.tunnel];

  // The bounds pass through degenerate and undersized states while the user drags.
  if (
    width <= 2.0 * corridorShape.wallThickness
    || height <= 2.0 * corridorShape.wallThickness || depth <= 0.0)
  {
    return Result<std::vector<Brush>>{std::vector<Brush>{}};
  }

  const auto radius = std::min({corridorShape.cornerRadius, width / 2.0, height / 2.0});
  if (!corridorShapeFits(corridorShape, width, height, radius))
  {
    return Result<std::vector<Brush>>{std::vector<Brush>{}};
  }

  const auto crossSectionBounds = vm::bbox2d{
    {bounds.min[axes.span], bounds.min[axes.vertical]},
    {bounds.max[axes.span], bounds.max[axes.vertical]},
  };
  const auto boundary = makeCorridorBoundary(crossSectionBounds, corridorShape, radius);

  const auto toPoint = [&](const vm::vec2d& p, const double w) {
    auto result = vm::vec3d{};
    result[axes.span] = p.x();
    result[axes.vertical] = p.y();
    result[axes.tunnel] = w;
    return result;
  };

  auto brushes = std::vector<Result<Brush>>{};
  brushes.reserve(boundary.size());
  for (size_t i = 0u; i < boundary.size(); ++i)
  {
    const auto& current = boundary[i];
    const auto& next = boundary[(i + 1u) % boundary.size()];
    const auto vertices = std::vector{
      toPoint(current.outer, bounds.min[axes.tunnel]),
      toPoint(current.outer, bounds.max[axes.tunnel]),
      toPoint(next.outer, bounds.min[axes.tunnel]),
      toPoint(next.outer, bounds.max[axes.tunnel]),
      toPoint(current.inner, bounds.min[axes.tunnel]),
      toPoint(current.inner, bounds.max[axes.tunnel]),
      toPoint(next.inner, bounds.min[axes.tunnel]),
      toPoint(next.inner, bounds.max[axes.tunnel]),
    };
    brushes.push_back(createBrush(vertices, textureName));
  }

  return brushes | kdl::fold;
}

Result<std::vector<Brush>> BrushBuilder::createCorridorBend(
  const vm::bbox3d& bounds,
  const CorridorShape& corridorShape,
  const vm::axis::type axis,
  const CorridorBendAngle angle,
  const CorridorBendDirection direction,
  const size_t segmentsPer45Degrees,
  const std::string& textureName) const
{
  if (const auto error = corridorShapeError(corridorShape))
  {
    return Error{*error};
  }
  if (axis == vm::axis::z)
  {
    return Error{"Corridor bends require a horizontal X or Y axis"};
  }
  if (segmentsPer45Degrees == 0u)
  {
    return Error{"Corridor bend segments must be greater than zero"};
  }

  const auto axes = uprightAxes(axis);
  const auto width = bounds.size()[axes.span];
  const auto height = bounds.size()[axes.vertical];
  const auto depth = bounds.size()[axes.tunnel];
  if (
    width <= 2.0 * corridorShape.wallThickness
    || height <= 2.0 * corridorShape.wallThickness || depth <= 0.0)
  {
    return Result<std::vector<Brush>>{std::vector<Brush>{}};
  }

  const auto profileRadius =
    std::min({corridorShape.cornerRadius, width / 2.0, height / 2.0});
  if (!corridorShapeFits(corridorShape, width, height, profileRadius))
  {
    return Result<std::vector<Brush>>{std::vector<Brush>{}};
  }

  const auto bendAngle =
    angle == CorridorBendAngle::Deg45 ? vm::Cd::quarter_pi() : vm::Cd::half_pi();
  const auto centerlineRadius = depth / std::sin(bendAngle) - width / 2.0;
  if (centerlineRadius <= width / 2.0)
  {
    return Result<std::vector<Brush>>{std::vector<Brush>{}};
  }

  const auto crossSectionBounds = vm::bbox2d{
    {bounds.min[axes.span], bounds.min[axes.vertical]},
    {bounds.max[axes.span], bounds.max[axes.vertical]},
  };
  const auto boundary =
    makeCorridorBoundary(crossSectionBounds, corridorShape, profileRadius);
  const auto spanCenter = crossSectionBounds.center().x();
  auto turnSign = direction == CorridorBendDirection::Left ? 1.0 : -1.0;
  if (axis == vm::axis::y)
  {
    turnSign = -turnSign;
  }

  const auto toPoint = [&](const vm::vec2d& p, const double theta) {
    const auto u = p.x() - spanCenter;
    auto result = vm::vec3d{};
    result[axes.tunnel] = bounds.min[axes.tunnel] + centerlineRadius * std::sin(theta)
                          - turnSign * u * std::sin(theta);
    result[axes.span] = spanCenter + turnSign * centerlineRadius * (1.0 - std::cos(theta))
                        + u * std::cos(theta);
    result[axes.vertical] = p.y();
    return result;
  };

  const auto numSegments =
    segmentsPer45Degrees * (angle == CorridorBendAngle::Deg90 ? 2u : 1u);
  auto brushes = std::vector<Result<Brush>>{};
  brushes.reserve(boundary.size() * numSegments);
  for (size_t segmentIndex = 0u; segmentIndex < numSegments; ++segmentIndex)
  {
    const auto theta0 = bendAngle * double(segmentIndex) / double(numSegments);
    const auto theta1 = bendAngle * double(segmentIndex + 1u) / double(numSegments);
    for (size_t boundaryIndex = 0u; boundaryIndex < boundary.size(); ++boundaryIndex)
    {
      const auto& current = boundary[boundaryIndex];
      const auto& next = boundary[(boundaryIndex + 1u) % boundary.size()];
      const auto vertices = std::vector{
        toPoint(current.outer, theta0),
        toPoint(current.outer, theta1),
        toPoint(next.outer, theta0),
        toPoint(next.outer, theta1),
        toPoint(current.inner, theta0),
        toPoint(current.inner, theta1),
        toPoint(next.inner, theta0),
        toPoint(next.inner, theta1),
      };
      brushes.push_back(createBrush(vertices, textureName));
    }
  }

  return brushes | kdl::fold;
}

Result<std::vector<Brush>> BrushBuilder::createCorridorTJunction(
  const vm::bbox3d& bounds,
  const CorridorShape& corridorShape,
  const vm::axis::type axis,
  const double corridorWidth,
  const std::string& textureName) const
{
  if (const auto error = corridorShapeError(corridorShape))
  {
    return Error{*error};
  }
  if (axis == vm::axis::z)
  {
    return Error{"Corridor T-junctions require a horizontal X or Y axis"};
  }
  if (corridorWidth <= 0.0)
  {
    return Error{"Corridor T-junction width must be greater than zero"};
  }

  const auto axes = uprightAxes(axis);
  const auto height = bounds.size()[axes.vertical];
  const auto tunnelLength = bounds.size()[axes.tunnel];
  const auto crossbarLength = bounds.size()[axes.span];
  if (
    corridorWidth <= 2.0 * corridorShape.wallThickness
    || height <= 2.0 * corridorShape.wallThickness || tunnelLength <= corridorWidth
    || crossbarLength <= corridorWidth)
  {
    return Result<std::vector<Brush>>{std::vector<Brush>{}};
  }

  const auto profileRadius =
    std::min({corridorShape.cornerRadius, corridorWidth / 2.0, height / 2.0});
  if (!corridorShapeFits(corridorShape, corridorWidth, height, profileRadius))
  {
    return Result<std::vector<Brush>>{std::vector<Brush>{}};
  }

  const auto tunnelMax = bounds.max[axes.tunnel];
  const auto spanMin = bounds.min[axes.span];
  const auto spanMax = bounds.max[axes.span];
  const auto spanCenter = (spanMin + spanMax) / 2.0;
  const auto halfWidth = corridorWidth / 2.0;
  const auto branchPlane = tunnelMax - corridorWidth;
  const auto crossbarAxis = vm::axis::type(axes.span);

  auto stemBounds = bounds;
  stemBounds.max[axes.tunnel] = branchPlane;
  stemBounds.min[axes.span] = spanCenter - halfWidth;
  stemBounds.max[axes.span] = spanCenter + halfWidth;

  auto lowerCrossbarBounds = bounds;
  lowerCrossbarBounds.min[axes.tunnel] = branchPlane;
  lowerCrossbarBounds.max[axes.span] = spanCenter - halfWidth;

  auto centerCrossbarBounds = bounds;
  centerCrossbarBounds.min[axes.tunnel] = branchPlane;
  centerCrossbarBounds.min[axes.span] = spanCenter - halfWidth;
  centerCrossbarBounds.max[axes.span] = spanCenter + halfWidth;

  auto upperCrossbarBounds = bounds;
  upperCrossbarBounds.min[axes.tunnel] = branchPlane;
  upperCrossbarBounds.min[axes.span] = spanCenter + halfWidth;

  auto centerResult =
    createCorridor(centerCrossbarBounds, corridorShape, crossbarAxis, textureName)
    | kdl::transform([&](auto brushes) {
        const auto branchSideLimit = branchPlane + profileRadius + vm::Cd::almost_zero();
        std::erase_if(brushes, [&](const auto& brush) {
          return brush.bounds().max[axes.tunnel] <= branchSideLimit;
        });
        return brushes;
      });

  auto partResults = std::vector<Result<std::vector<Brush>>>{};
  partResults.reserve(4u);
  partResults.push_back(createCorridor(stemBounds, corridorShape, axis, textureName));
  partResults.push_back(
    createCorridor(lowerCrossbarBounds, corridorShape, crossbarAxis, textureName));
  partResults.push_back(std::move(centerResult));
  partResults.push_back(
    createCorridor(upperCrossbarBounds, corridorShape, crossbarAxis, textureName));

  return partResults | kdl::fold | kdl::transform([](auto parts) {
           auto brushes = std::vector<Brush>{};
           auto brushCount = size_t{0u};
           for (const auto& part : parts)
           {
             brushCount += part.size();
           }
           brushes.reserve(brushCount);
           for (auto& part : parts)
           {
             brushes.insert(
               brushes.end(),
               std::make_move_iterator(part.begin()),
               std::make_move_iterator(part.end()));
           }
           return brushes;
         });
}

namespace
{

double chamberForward(const vm::vec2d& point, const vm::axis::type axis)
{
  return point[axis == vm::axis::x ? 0u : 1u];
}

double chamberSpan(const vm::vec2d& point, const vm::axis::type axis)
{
  return point[axis == vm::axis::x ? 1u : 0u];
}

vm::vec2d chamberPoint(const double forward, const double span, const vm::axis::type axis)
{
  return axis == vm::axis::x ? vm::vec2d{forward, span} : vm::vec2d{span, forward};
}

double polygonArea(const std::vector<vm::vec2d>& polygon)
{
  auto twiceArea = 0.0;
  for (size_t i = 0u; i < polygon.size(); ++i)
  {
    const auto& current = polygon[i];
    const auto& next = polygon[(i + 1u) % polygon.size()];
    twiceArea += current.x() * next.y() - next.x() * current.y();
  }
  return twiceArea / 2.0;
}

void appendUnique(std::vector<vm::vec2d>& points, const vm::vec2d& point)
{
  if (points.empty() || !vm::is_equal(points.back(), point, vm::Cd::almost_zero()))
  {
    points.push_back(point);
  }
}

void appendArc(
  std::vector<vm::vec2d>& points,
  const vm::vec2d& center,
  const double radius,
  const double startAngle,
  const double endAngle,
  const size_t segments)
{
  for (size_t i = 0u; i <= segments; ++i)
  {
    const auto angle =
      startAngle + (endAngle - startAngle) * double(i) / double(segments);
    appendUnique(points, center + vm::vec2d{std::cos(angle), std::sin(angle)} * radius);
  }
}

std::vector<vm::vec2d> makeChamberFootprint(
  const vm::bbox2d& bounds, const ChamberShape& shape, const vm::axis::type axis)
{
  const auto forwardMin = axis == vm::axis::x ? bounds.min.x() : bounds.min.y();
  const auto forwardMax = axis == vm::axis::x ? bounds.max.x() : bounds.max.y();
  const auto spanMin = axis == vm::axis::x ? bounds.min.y() : bounds.min.x();
  const auto spanMax = axis == vm::axis::x ? bounds.max.y() : bounds.max.x();
  const auto forwardLength = forwardMax - forwardMin;
  const auto spanLength = spanMax - spanMin;
  const auto spanCenter = (spanMin + spanMax) / 2.0;

  auto local = std::vector<vm::vec2d>{};
  const auto appendLocal = [&](const double forward, const double span) {
    appendUnique(local, {forward, span});
  };

  switch (shape.footprint)
  {
  case ChamberFootprint::Chamfered:
  case ChamberFootprint::Octagonal: {
    const auto forwardCut = shape.footprint == ChamberFootprint::Octagonal
                              ? forwardLength * (1.0 - 1.0 / std::sqrt(2.0))
                              : shape.cornerSize;
    const auto spanCut = shape.footprint == ChamberFootprint::Octagonal
                           ? spanLength * (1.0 - 1.0 / std::sqrt(2.0))
                           : shape.cornerSize;
    const auto cutForward = std::clamp(forwardCut, 0.0, forwardLength / 2.0);
    const auto entranceOuterWidth =
      shape.openEntrance ? shape.entranceWidth + 2.0 * shape.wallThickness : 0.0;
    const auto maximumSpanCut = shape.openEntrance
                                  ? std::max(0.0, (spanLength - entranceOuterWidth) / 2.0)
                                  : spanLength / 2.0;
    const auto cutSpan = std::clamp(spanCut, 0.0, maximumSpanCut);
    appendLocal(forwardMin + cutForward, spanMin);
    appendLocal(forwardMax - cutForward, spanMin);
    appendLocal(forwardMax, spanMin + cutSpan);
    appendLocal(forwardMax, spanMax - cutSpan);
    appendLocal(forwardMax - cutForward, spanMax);
    appendLocal(forwardMin + cutForward, spanMax);
    appendLocal(forwardMin, spanMax - cutSpan);
    appendLocal(forwardMin, spanMin + cutSpan);
    break;
  }
  case ChamberFootprint::Capsule: {
    const auto entranceOuterWidth =
      shape.openEntrance ? shape.entranceWidth + 2.0 * shape.wallThickness : 0.0;
    const auto desiredRadius =
      shape.openEntrance ? (spanLength - entranceOuterWidth) / 2.0 : spanLength / 2.0;
    const auto radius =
      std::clamp(desiredRadius, 0.0, std::min(forwardLength, spanLength) / 2.0);
    if (radius <= vm::Cd::almost_zero())
    {
      appendLocal(forwardMin, spanMin);
      appendLocal(forwardMax, spanMin);
      appendLocal(forwardMax, spanMax);
      appendLocal(forwardMin, spanMax);
      break;
    }

    appendLocal(forwardMin + radius, spanMin);
    appendLocal(forwardMax - radius, spanMin);
    appendArc(
      local,
      {forwardMax - radius, spanMin + radius},
      radius,
      -vm::Cd::half_pi(),
      0.0,
      shape.footprintSegments);
    appendArc(
      local,
      {forwardMax - radius, spanMax - radius},
      radius,
      0.0,
      vm::Cd::half_pi(),
      shape.footprintSegments);
    appendArc(
      local,
      {forwardMin + radius, spanMax - radius},
      radius,
      vm::Cd::half_pi(),
      vm::Cd::pi(),
      shape.footprintSegments);
    appendArc(
      local,
      {forwardMin + radius, spanMin + radius},
      radius,
      vm::Cd::pi(),
      3.0 * vm::Cd::half_pi(),
      shape.footprintSegments);
    break;
  }
  case ChamberFootprint::Wedge: {
    const auto farHalfWidth = spanLength / 2.0;
    const auto entranceOuterWidth = shape.openEntrance
                                      ? shape.entranceWidth + 2.0 * shape.wallThickness
                                      : spanLength * 0.5;
    const auto nearHalfWidth =
      std::clamp(entranceOuterWidth / 2.0, shape.wallThickness, farHalfWidth);
    appendLocal(forwardMin, spanCenter - nearHalfWidth);
    appendLocal(forwardMax, spanMin);
    appendLocal(forwardMax, spanMax);
    appendLocal(forwardMin, spanCenter + nearHalfWidth);
    break;
  }
  case ChamberFootprint::Apse: {
    const auto radius = spanLength / 2.0;
    const auto apseCenter = forwardMax - radius;
    if (apseCenter <= forwardMin)
    {
      return {};
    }
    appendLocal(forwardMin, spanMin);
    appendLocal(apseCenter, spanMin);
    appendArc(
      local,
      {apseCenter, spanCenter},
      radius,
      -vm::Cd::half_pi(),
      vm::Cd::half_pi(),
      2u * shape.footprintSegments);
    appendLocal(forwardMin, spanMax);
    break;
  }
  }

  if (
    local.size() > 1u && vm::is_equal(local.front(), local.back(), vm::Cd::almost_zero()))
  {
    local.pop_back();
  }

  auto result = local | std::views::transform([&](const auto& point) {
                  return chamberPoint(point.x(), point.y(), axis);
                })
                | kdl::ranges::to<std::vector>();
  if (polygonArea(result) < 0.0)
  {
    std::ranges::reverse(result);
  }
  return result;
}

double cross2d(const vm::vec2d& lhs, const vm::vec2d& rhs)
{
  return lhs.x() * rhs.y() - lhs.y() * rhs.x();
}

std::optional<std::vector<vm::vec2d>> insetConvexPolygon(
  const std::vector<vm::vec2d>& polygon, const double amount)
{
  if (polygon.size() < 3u)
  {
    return std::nullopt;
  }

  auto result = std::vector<vm::vec2d>{};
  result.reserve(polygon.size());
  for (size_t i = 0u; i < polygon.size(); ++i)
  {
    const auto& previous = polygon[(i + polygon.size() - 1u) % polygon.size()];
    const auto& current = polygon[i];
    const auto& next = polygon[(i + 1u) % polygon.size()];
    const auto previousDirection = current - previous;
    const auto nextDirection = next - current;
    const auto previousLength = vm::length(previousDirection);
    const auto nextLength = vm::length(nextDirection);
    if (previousLength <= vm::Cd::almost_zero() || nextLength <= vm::Cd::almost_zero())
    {
      return std::nullopt;
    }

    const auto previousNormal =
      vm::vec2d{-previousDirection.y(), previousDirection.x()} / previousLength;
    const auto nextNormal = vm::vec2d{-nextDirection.y(), nextDirection.x()} / nextLength;
    const auto previousLinePoint = previous + previousNormal * amount;
    const auto nextLinePoint = current + nextNormal * amount;
    const auto denominator = cross2d(previousDirection, nextDirection);
    if (std::abs(denominator) <= vm::Cd::almost_zero())
    {
      return std::nullopt;
    }

    const auto distance =
      cross2d(nextLinePoint - previousLinePoint, nextDirection) / denominator;
    const auto point = previousLinePoint + previousDirection * distance;
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
    {
      return std::nullopt;
    }
    result.push_back(point);
  }

  if (polygonArea(result) <= vm::Cd::almost_zero())
  {
    return std::nullopt;
  }
  return result;
}

std::vector<vm::vec2d> clipChamberPolygon(
  const std::vector<vm::vec2d>& polygon,
  const vm::axis::type axis,
  const double boundary,
  const bool keepGreater)
{
  auto result = std::vector<vm::vec2d>{};
  if (polygon.empty())
  {
    return result;
  }

  const auto inside = [&](const vm::vec2d& point) {
    return keepGreater ? chamberSpan(point, axis) >= boundary
                       : chamberSpan(point, axis) <= boundary;
  };
  const auto intersection = [&](const vm::vec2d& a, const vm::vec2d& b) {
    const auto aSpan = chamberSpan(a, axis);
    const auto bSpan = chamberSpan(b, axis);
    const auto t = (boundary - aSpan) / (bSpan - aSpan);
    return a + (b - a) * t;
  };

  for (size_t i = 0u; i < polygon.size(); ++i)
  {
    const auto& current = polygon[i];
    const auto& next = polygon[(i + 1u) % polygon.size()];
    const auto currentInside = inside(current);
    const auto nextInside = inside(next);
    if (currentInside)
    {
      appendUnique(result, current);
    }
    if (currentInside != nextInside)
    {
      appendUnique(result, intersection(current, next));
    }
  }
  if (
    result.size() > 1u
    && vm::is_equal(result.front(), result.back(), vm::Cd::almost_zero()))
  {
    result.pop_back();
  }
  return result;
}

std::vector<vm::vec3d> chamberPrismVertices(
  const std::vector<vm::vec2d>& polygon, const double bottom, const double top)
{
  auto vertices = std::vector<vm::vec3d>{};
  vertices.reserve(2u * polygon.size());
  for (const auto& point : polygon)
  {
    vertices.emplace_back(point, bottom);
    vertices.emplace_back(point, top);
  }
  return vertices;
}

std::vector<vm::vec3d> chamberWallVertices(
  const vm::vec2d& outerStart,
  const vm::vec2d& outerEnd,
  const vm::vec2d& innerStart,
  const vm::vec2d& innerEnd,
  const double bottom,
  const double top)
{
  return {
    {outerStart, bottom},
    {outerStart, top},
    {outerEnd, bottom},
    {outerEnd, top},
    {innerStart, bottom},
    {innerStart, top},
    {innerEnd, bottom},
    {innerEnd, top},
  };
}

} // namespace

Result<std::vector<Brush>> BrushBuilder::createChamberShell(
  const vm::bbox3d& bounds,
  const ChamberShape& chamberShape,
  const vm::axis::type axis,
  const std::string& textureName) const
{
  if (axis == vm::axis::z)
  {
    return Error{"Chamber shells require a horizontal X or Y axis"};
  }
  if (chamberShape.wallThickness <= 0.0)
  {
    return Error{"Chamber wall thickness must be greater than zero"};
  }
  if (chamberShape.cornerSize < 0.0)
  {
    return Error{"Chamber corner size must be non-negative"};
  }
  if (chamberShape.footprintSegments == 0u)
  {
    return Error{"Chamber footprint segments must be greater than zero"};
  }
  if (chamberShape.ceilingRise < 0.0)
  {
    return Error{"Chamber ceiling rise must be non-negative"};
  }
  if (chamberShape.ceilingSegments == 0u)
  {
    return Error{"Chamber ceiling segments must be greater than zero"};
  }
  if (
    chamberShape.openEntrance
    && (chamberShape.entranceWidth <= 0.0 || chamberShape.entranceHeight <= 0.0))
  {
    return Error{"Chamber entrance dimensions must be greater than zero"};
  }

  const auto width = bounds.size().x();
  const auto depth = bounds.size().y();
  const auto height = bounds.size().z();
  const auto ceilingRise =
    chamberShape.ceiling == ChamberCeiling::Flat ? 0.0 : chamberShape.ceilingRise;
  if (
    width <= 2.0 * chamberShape.wallThickness || depth <= 2.0 * chamberShape.wallThickness
    || height <= 2.0 * chamberShape.wallThickness + ceilingRise)
  {
    return Result<std::vector<Brush>>{std::vector<Brush>{}};
  }

  const auto outer = makeChamberFootprint(bounds.xy(), chamberShape, axis);
  const auto inner = insetConvexPolygon(outer, chamberShape.wallThickness);
  if (outer.size() < 3u || !inner || inner->size() != outer.size())
  {
    return Result<std::vector<Brush>>{std::vector<Brush>{}};
  }

  const auto floorTop = bounds.min.z() + chamberShape.wallThickness;
  const auto ceilingSpring = bounds.max.z() - chamberShape.wallThickness - ceilingRise;
  const auto wallTop = chamberShape.ceiling == ChamberCeiling::Flat
                         ? ceilingSpring
                         : bounds.max.z();
  if (ceilingSpring <= floorTop)
  {
    return Result<std::vector<Brush>>{std::vector<Brush>{}};
  }

  auto brushes = std::vector<Result<Brush>>{};
  brushes.push_back(
    createBrush(chamberPrismVertices(outer, bounds.min.z(), floorTop), textureName));

  auto entranceEdge = std::optional<size_t>{};
  if (chamberShape.openEntrance)
  {
    const auto forwardMin = axis == vm::axis::x ? bounds.min.x() : bounds.min.y();
    for (size_t i = 0u; i < outer.size(); ++i)
    {
      const auto& start = outer[i];
      const auto& end = outer[(i + 1u) % outer.size()];
      if (
        std::abs(chamberForward(start, axis) - forwardMin) <= vm::Cd::almost_zero()
        && std::abs(chamberForward(end, axis) - forwardMin) <= vm::Cd::almost_zero())
      {
        entranceEdge = i;
        break;
      }
    }
    if (!entranceEdge)
    {
      return Result<std::vector<Brush>>{std::vector<Brush>{}};
    }
  }

  for (size_t i = 0u; i < outer.size(); ++i)
  {
    const auto next = (i + 1u) % outer.size();
    if (!entranceEdge || i != *entranceEdge)
    {
      brushes.push_back(createBrush(
        chamberWallVertices(
          outer[i], outer[next], (*inner)[i], (*inner)[next], floorTop, wallTop),
        textureName));
      continue;
    }

    const auto spanCenter =
      axis == vm::axis::x ? bounds.xy().center().y() : bounds.xy().center().x();
    const auto openingMin = spanCenter - chamberShape.entranceWidth / 2.0;
    const auto openingMax = spanCenter + chamberShape.entranceWidth / 2.0;
    const auto interpolateAtSpan =
      [&](const vm::vec2d& start, const vm::vec2d& end, const double span) {
        const auto startSpan = chamberSpan(start, axis);
        const auto t = (span - startSpan) / (chamberSpan(end, axis) - startSpan);
        return std::pair{t, start + (end - start) * t};
      };

    const auto [outerMinT, outerMinPoint] =
      interpolateAtSpan(outer[i], outer[next], openingMin);
    const auto [outerMaxT, outerMaxPoint] =
      interpolateAtSpan(outer[i], outer[next], openingMax);
    const auto [innerMinT, innerMinPoint] =
      interpolateAtSpan((*inner)[i], (*inner)[next], openingMin);
    const auto [innerMaxT, innerMaxPoint] =
      interpolateAtSpan((*inner)[i], (*inner)[next], openingMax);
    if (
      outerMinT < 0.0 || outerMinT > 1.0 || outerMaxT < 0.0 || outerMaxT > 1.0
      || innerMinT < 0.0 || innerMinT > 1.0 || innerMaxT < 0.0 || innerMaxT > 1.0)
    {
      return Result<std::vector<Brush>>{std::vector<Brush>{}};
    }

    const auto outerStartPoint = outerMinT < outerMaxT ? outerMinPoint : outerMaxPoint;
    const auto outerEndPoint = outerMinT < outerMaxT ? outerMaxPoint : outerMinPoint;
    const auto innerStartPoint = innerMinT < innerMaxT ? innerMinPoint : innerMaxPoint;
    const auto innerEndPoint = innerMinT < innerMaxT ? innerMaxPoint : innerMinPoint;
    brushes.push_back(createBrush(
      chamberWallVertices(
        outer[i], outerStartPoint, (*inner)[i], innerStartPoint, floorTop, wallTop),
      textureName));
    brushes.push_back(createBrush(
      chamberWallVertices(
        outerEndPoint,
        outer[next],
        innerEndPoint,
        (*inner)[next],
        floorTop,
        wallTop),
      textureName));

    const auto openingTop = floorTop + chamberShape.entranceHeight;
    if (openingTop < wallTop)
    {
      brushes.push_back(createBrush(
        chamberWallVertices(
          outerStartPoint,
          outerEndPoint,
          innerStartPoint,
          innerEndPoint,
          openingTop,
          wallTop),
        textureName));
    }
  }

  if (chamberShape.ceiling == ChamberCeiling::Flat)
  {
    brushes.push_back(createBrush(
      chamberPrismVertices(outer, ceilingSpring, bounds.max.z()), textureName));
  }
  else
  {
    const auto spanMin = axis == vm::axis::x ? bounds.min.y() : bounds.min.x();
    const auto spanMax = axis == vm::axis::x ? bounds.max.y() : bounds.max.x();
    const auto spanCenter = (spanMin + spanMax) / 2.0;
    const auto halfSpan = (spanMax - spanMin) / 2.0;
    const auto segmentCount = 2u * chamberShape.ceilingSegments;
    const auto ceilingHeight = [&](const double span) {
      const auto normalized = std::clamp((span - spanCenter) / halfSpan, -1.0, 1.0);
      const auto factor = chamberShape.ceiling == ChamberCeiling::BarrelVault
                            ? std::sqrt(std::max(0.0, 1.0 - normalized * normalized))
                            : 1.0 - std::abs(normalized);
      return ceilingSpring + ceilingRise * factor;
    };

    for (size_t segment = 0u; segment < segmentCount; ++segment)
    {
      const auto segmentMin =
        spanMin + (spanMax - spanMin) * double(segment) / double(segmentCount);
      const auto segmentMax =
        spanMin + (spanMax - spanMin) * double(segment + 1u) / double(segmentCount);
      auto clipped = clipChamberPolygon(outer, axis, segmentMin, true);
      clipped = clipChamberPolygon(clipped, axis, segmentMax, false);
      if (clipped.size() < 3u)
      {
        continue;
      }

      const auto minHeight = ceilingHeight(segmentMin);
      const auto maxHeight = ceilingHeight(segmentMax);
      auto vertices = std::vector<vm::vec3d>{};
      vertices.reserve(2u * clipped.size());
      for (const auto& point : clipped)
      {
        const auto t =
          (chamberSpan(point, axis) - segmentMin) / (segmentMax - segmentMin);
        vertices.emplace_back(point, minHeight + (maxHeight - minHeight) * t);
        vertices.emplace_back(point, bounds.max.z());
      }
      brushes.push_back(createBrush(vertices, textureName));
    }
  }

  return brushes | kdl::fold;
}

namespace
{
auto setZ(const std::vector<vm::vec2d>& vertices, const double z)
{
  return vertices | std::views::transform([&](const auto& v) { return vm::vec3d{v, z}; })
         | kdl::ranges::to<std::vector>();
}

/** If a scalable cone is stretched, it doesn't have one vertex as the tip. Instead,
 * the tip is an edge.
 */
auto makeScalableConeTip(const vm::bbox3d& boundsXY)
{
  const auto offset = vm::min(boundsXY.xy().size().x(), boundsXY.xy().size().y()) / 2.0;
  return kdl::vec_sort_and_remove_duplicates(std::vector<vm::vec2d>{
    {boundsXY.xy().min.x() + offset, boundsXY.xy().min.y() + offset},
    {boundsXY.xy().min.x() + offset, boundsXY.xy().max.y() - offset},
    {boundsXY.xy().max.x() - offset, boundsXY.xy().min.y() + offset},
    {boundsXY.xy().max.x() - offset, boundsXY.xy().max.y() - offset},
  });
}

auto makeCone(const CircleShape& circleShape, const vm::bbox3d& boundsXY)
{
  return std::visit(
    kdl::overload(
      [&](const ScalableCircle& scalableCircle) {
        return kdl::views::concat(
                 setZ(
                   makeScalableCircle(scalableCircle.precision, boundsXY.xy()),
                   boundsXY.min.z()),
                 setZ(makeScalableConeTip(boundsXY), boundsXY.max.z()))
               | kdl::ranges::to<std::vector>();
      },
      [&](const auto&) {
        return kdl::vec_push_back(
          setZ(makeCircle(circleShape, boundsXY.xy()), boundsXY.min.z()),
          vm::vec3d{boundsXY.xy().center(), boundsXY.max.z()});
      }),
    circleShape);
}
} // namespace

Result<Brush> BrushBuilder::createCone(
  const vm::bbox3d& bounds,
  const CircleShape& circleShape,
  const vm::axis::type axis,
  const std::string& textureName) const
{
  const auto toXY = vm::rotation_matrix(vm::vec3d::axis(axis), vm::vec3d{0, 0, 1});
  const auto fromXY = vm::rotation_matrix(vm::vec3d{0, 0, 1}, vm::vec3d::axis(axis));
  const auto boundsXY = bounds.transform(toXY);

  const auto cone = makeCone(circleShape, boundsXY);
  return createBrush(fromXY * cone, textureName);
}

namespace
{
auto subDivideRatios(const std::vector<double>& ratios)
{
  auto newRatios = std::vector<double>{};
  newRatios.push_back(ratios.front());
  for (size_t j = 1; j < ratios.size(); ++j)
  {
    const auto previousSize = ratios[j - 1];
    const auto currentSize = ratios[j];
    newRatios.push_back((previousSize + currentSize) / 2.0);
  }
  newRatios.push_back(ratios.back());
  return newRatios;
}

auto makeSizeRatiosPerRing(const size_t precision)
{
  auto sizeRatios = std::vector<double>{0.0, 1.0 / 2.0, 7.0 / 8.0, 1.0};
  for (size_t i = 0; i < precision; ++i)
  {
    sizeRatios = subDivideRatios(sizeRatios);
  }

  const auto n = sizeRatios.size();
  for (size_t i = 0; i < n - 1; ++i)
  {
    sizeRatios.push_back(sizeRatios[n - i - 2]);
  }

  return sizeRatios;
}

auto makeZRatiosPerRing(const size_t precision)
{
  auto zRatios = std::vector<double>{1.0, 7.0 / 8.0, 1.0 / 2.0, 0.0};
  for (size_t i = 0; i < precision; ++i)
  {
    zRatios = subDivideRatios(zRatios);
  }

  const auto n = zRatios.size();
  for (size_t i = 0; i < n - 1; ++i)
  {
    zRatios.push_back(-zRatios[n - i - 2]);
  }

  return zRatios;
}

auto makeScalableUvSphere(const vm::bbox3d& boundsXY, const size_t precision)
{
  const auto zRatios = makeZRatiosPerRing(precision);
  const auto getZ = [&](const size_t i) {
    const auto center = boundsXY.center();
    const auto size = boundsXY.size() / 2.0;
    return center.z() + size.z() * zRatios[i];
  };

  const auto sizeRatios = makeSizeRatiosPerRing(precision);
  const auto getBounds = [&](const size_t i) {
    const auto s = vm::min(boundsXY.size().x(), boundsXY.size().y()) / 2.0;
    return boundsXY.xy().expand(-s * (1 - sizeRatios[i]));
  };

  const auto numRings = size_t(std::pow(2, precision)) * 12 / 2 - 1;

  auto vertices = std::vector<vm::vec3d>{};
  kdl::vec_append(vertices, setZ(makeScalableConeTip(boundsXY), getZ(0)));
  for (size_t i = 1; i <= numRings; ++i)
  {
    kdl::vec_append(vertices, setZ(makeScalableCircle(precision, getBounds(i)), getZ(i)));
  }
  kdl::vec_append(vertices, setZ(makeScalableConeTip(boundsXY), getZ(numRings + 1)));

  return vertices;
}

auto makeRing(
  const double angle, const CircleShape& circleShape, const vm::bbox3d& boundsXY)
{
  const auto r = std::sin(angle);
  const auto z = boundsXY.center().z() + std::cos(angle) * boundsXY.size().z() / 2.0;
  const auto t = vm::translation_matrix(boundsXY.xy().center())
                 * vm::scaling_matrix(vm::vec2d{r, r})
                 * vm::translation_matrix(-boundsXY.xy().center());
  const auto circle = t * makeCircle(circleShape, boundsXY.xy());
  return circle | std::views::transform([&](const auto& v) { return vm::vec3d{v, z}; })
         | kdl::ranges::to<std::vector>();
}

auto makeAlignedUvSphere(
  const vm::bbox3d& boundsXY, const CircleShape& circleShape, const size_t numRings)
{
  const auto angleDelta = vm::Cd::pi() / (double(numRings) + 1.0);

  auto vertices = std::vector<vm::vec3d>{};
  vertices.emplace_back(boundsXY.xy().center(), boundsXY.max.z());

  for (size_t i = 0; i < numRings; ++i)
  {
    kdl::vec_append(vertices, makeRing(double(i) * angleDelta, circleShape, boundsXY));
  }

  vertices.emplace_back(boundsXY.xy().center(), boundsXY.min.z());

  // ensure that the sphere fills the bounds when number of rings is equal
  const auto centerRingRadius = std::sin(angleDelta * double(numRings / 2));
  const auto extraScale = numRings % 2 == 0 ? 1.0 / centerRingRadius : 1.0;
  const auto transform = vm::translation_matrix(boundsXY.center())
                         * vm::scaling_matrix(vm::vec3d{extraScale, extraScale, 1.0})
                         * vm::translation_matrix(-boundsXY.center());

  return transform * vertices;
}

} // namespace

Result<Brush> BrushBuilder::createUvSphere(
  const vm::bbox3d& bounds,
  const CircleShape& circleShape,
  const size_t numRings,
  const vm::axis::type axis,
  const std::string& textureName) const
{
  const auto fromXY = vm::rotation_matrix(vm::vec3d{0, 0, 1}, vm::vec3d::axis(axis));
  const auto toXY = vm::rotation_matrix(vm::vec3d::axis(axis), vm::vec3d{0, 0, 1});
  const auto boundsXY = bounds.transform(toXY);

  const auto sphere = std::visit(
    kdl::overload(
      [&](const ScalableCircle& scalable) {
        return makeScalableUvSphere(boundsXY, scalable.precision);
      },
      [&](const auto& edgeOrVertexAligned) {
        return makeAlignedUvSphere(boundsXY, edgeOrVertexAligned, numRings);
      }),
    circleShape);

  return createBrush(fromXY * sphere, textureName);
}

Result<Brush> BrushBuilder::createIcoSphere(
  const vm::bbox3d& bounds, const size_t iterations, const std::string& textureName) const
{
  const auto [sphereVertices_, sphereIndices] = sphereMesh<double>(iterations);

  return sphereIndices
         | std::views::transform(
           [sphereVertices = sphereVertices_, &textureName, this](const auto& face) {
             const auto& p1 = sphereVertices[face[0]];
             const auto& p2 = sphereVertices[face[1]];
             const auto& p3 = sphereVertices[face[2]];
             return BrushFace::create(
               p1,
               p2,
               p3,
               textureName,
               m_defaultUvAttributes,
               m_defaultSurfaceAttributes,
               m_mapFormat);
           })
         | kdl::fold | kdl::and_then([&](auto f) {
             return Brush::create(m_worldBounds, std::move(f));
           })
         | kdl::and_then([&](auto b) {
             const auto transform = vm::translation_matrix(bounds.min)
                                    * vm::scaling_matrix(bounds.size())
                                    * vm::scaling_matrix(vm::vec3d{0.5, 0.5, 0.5})
                                    * vm::translation_matrix(vm::vec3d{1, 1, 1});
             return b.transform(m_worldBounds, transform, false)
                    | kdl::transform([&]() { return std::move(b); });
           });
}

Result<Brush> BrushBuilder::createBrush(
  const std::vector<vm::vec3d>& points, const std::string& materialName) const
{
  return createBrush(Polyhedron3{points}, materialName);
}

Result<Brush> BrushBuilder::createBrush(
  const Polyhedron3& polyhedron, const std::string& materialName) const
{
  if (polyhedron.empty())
  {
    return Error{"Cannot create brush from empty polyhedron"};
  }

  return polyhedron.faces() | std::views::transform([&](const auto* face) {
           const auto& boundary = face->boundary();

           auto bIt = std::begin(boundary);
           const auto* edge1 = *bIt++;
           const auto* edge2 = *bIt++;
           const auto* edge3 = *bIt++;

           const auto& p1 = edge1->origin()->position();
           const auto& p2 = edge2->origin()->position();
           const auto& p3 = edge3->origin()->position();

           return BrushFace::create(
             p1,
             p3,
             p2,
             materialName,
             m_defaultUvAttributes,
             m_defaultSurfaceAttributes,
             m_mapFormat);
         })
         | kdl::fold | kdl::and_then([&](auto faces) {
             return Brush::create(m_worldBounds, std::move(faces));
           });
}
} // namespace tb::mdl
