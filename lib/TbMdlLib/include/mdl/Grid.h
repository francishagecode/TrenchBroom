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

#pragma once

#include "base/Macros.h"
#include "base/Notifier.h"

#include "kd/contracts.h"

#include "vm/intersection.h"
#include "vm/mat.h"
#include "vm/mat_ext.h"
#include "vm/plane.h"
#include "vm/polygon.h"
#include "vm/scalar.h"
#include "vm/segment.h"
#include "vm/vec.h"

#include <array>
#include <optional>

// FIXME: should this be moved to Model?
namespace tb::mdl
{
class BrushFace;

class Grid
{
public:
  static const int MaxSize = 8;
  static const int MinSize = -3;

private:
  int m_size;
  bool m_snap = true;
  bool m_visible = true;
  vm::mat4x4d m_worldToGrid = vm::mat4x4d::identity();
  vm::mat4x4d m_gridToWorld = vm::mat4x4d::identity();

public:
  Notifier<> gridDidChangeNotifier;

public:
  explicit Grid(int size);

  static double actualSize(int size);

  int size() const;
  void setSize(int size);
  void incSize();
  void decSize();
  double actualSize() const;
  /**
   * Snap increment in radians for angle snapping
   */
  double angle() const;

  bool visible() const;
  void toggleVisible();

  bool snap() const;
  void toggleSnap();

  /**
   * Aligns the grid to the plane defined by two distinct parallel edges. The first grid
   * axis follows the edges, the second runs from the first edge to the second, and the
   * third is perpendicular to their plane. The start of the first edge becomes the grid
   * origin.
   *
   * Returns false without changing the grid if the edges are not parallel or if they are
   * collinear.
   */
  bool setAlignment(const vm::segment3d& first, const vm::segment3d& second);
  bool canSetAlignment(const vm::segment3d& first, const vm::segment3d& second) const;

  /**
   * Aligns the grid to the longest pair of distinct parallel boundary edges of the
   * given polygon. The pair with the greatest combined edge length is used.
   *
   * Returns false without changing the grid if the polygon has no such pair.
   */
  bool setAlignment(const vm::polygon3d& polygon);
  bool canSetAlignment(const vm::polygon3d& polygon) const;
  void resetAlignment();
  bool hasCustomAlignment() const;

  const vm::mat4x4d& worldToGridMatrix() const;
  const vm::mat4x4d& gridToWorldMatrix() const;
  vm::vec3d worldToGrid(const vm::vec3d& point) const;
  vm::vec3d gridToWorld(const vm::vec3d& point) const;
  vm::vec3d gridAxis(size_t index) const;

  template <typename T>
  T snapAngle(const T a) const
  {
    return snapAngle(a, angle());
  }

  /**
   * Snaps the given angle `a` to the nearest multiple of `snapAngle`, if grid snapping is
   * enabled.
   */
  template <typename T>
  T snapAngle(const T a, const T snapAngle) const
  {
    return snap() && snapAngle != T(0) ? snapAngle * vm::round(a / snapAngle) : a;
  }

public: // Snap scalars.
  template <typename T>
  T snap(const T f) const
  {
    return snap(f, SnapDir::None);
  }

  template <typename T>
  T offset(const T f) const
  {
    return snap() ? f - snap(f) : T(0);
  }

  template <typename T>
  T snapUp(const T f, const bool skip) const
  {
    return snap(f, SnapDir::Up, skip);
  }

  template <typename T>
  T snapDown(const T f, const bool skip) const
  {
    return snap(f, SnapDir::Down, skip);
  }

private:
  enum class SnapDir
  {
    /**
     * Snap to nearest grid increment (rounding away from 0 if the input is half way
     * between two multiples of the grid size).
     */
    None,
    /**
     * If off-grid, snap to the next larger grid increment.
     */
    Up,
    /**
     * If off-grid, snap to the next smaller grid increment.
     */
    Down
  };

  /**
   * Snaps a scalar to the grid.
   *
   * @tparam T scalar type
   * @param f scalar to snap
   * @param snapDir snap direction, see SnapDir
   * @param skip If true, SnapDir::Up/SnapDir::Down snap to the next larger/smaller grid
   * increment even if the input is already on-grid (within almost_zero()). If false,
   * on-grid inputs stay at the same grid increment.
   * @return snapped scalar
   */
  template <typename T>
  T snap(const T f, const SnapDir snapDir, const bool skip = false) const
  {
    if (!snap())
    {
      return f;
    }

    const auto actSize = T(actualSize());
    switch (snapDir)
    {
    case SnapDir::None:
      return vm::snap(f, actSize);
    case SnapDir::Up: {
      const auto s = actSize * std::ceil(f / actSize);
      return (skip && vm::is_equal(s, f, vm::constants<T>::almost_zero()))
               ? s + T(actualSize())
               : s;
    }
    case SnapDir::Down: {
      const auto s = actSize * std::floor(f / actSize);
      return (skip && vm::is_equal(s, f, vm::constants<T>::almost_zero()))
               ? s - T(actualSize())
               : s;
    }
      switchDefault();
    }
  }

public: // Snap vectors.
  /**
   * Snap each component to the nearest grid increment.
   */
  template <typename T, size_t S>
  vm::vec<T, S> snap(const vm::vec<T, S>& p) const
  {
    return snap(p, SnapDir::None);
  }

  template <typename T, size_t S>
  vm::vec<T, S> offset(const vm::vec<T, S>& p) const
  {
    return snap() ? p - snap(p) : vm::vec<T, S>::zero();
  }

  template <typename T, size_t S>
  vm::vec<T, S> snapUp(const vm::vec<T, S>& p, const bool skip = false) const
  {
    return snap(p, SnapDir::Up, skip);
  }

  template <typename T, size_t S>
  vm::vec<T, S> snapDown(const vm::vec<T, S>& p, const bool skip = false) const
  {
    return snap(p, SnapDir::Down, skip);
  }

  /**
   * Snaps a displacement to the grid axes without applying the grid origin.
   */
  vm::vec3d snapDelta(const vm::vec3d& delta) const
  {
    if (!snap())
    {
      return delta;
    }

    const auto toGrid = vm::strip_translation(m_worldToGrid);
    const auto toWorld = vm::strip_translation(m_gridToWorld);
    const auto local = toGrid * delta;
    return toWorld * vm::vec3d{snap(local.x()), snap(local.y()), snap(local.z())};
  }

private:
  template <typename T, size_t S>
  vm::vec<T, S> snap(
    const vm::vec<T, S>& p, const SnapDir snapDir, const bool skip = false) const
  {
    if (!snap())
    {
      return p;
    }

    auto local = p;
    if constexpr (S == 3)
    {
      local = vm::vec<T, S>{m_worldToGrid * vm::vec3d{p}};
    }

    auto result = vm::vec<T, S>{};
    for (size_t i = 0; i < S; ++i)
    {
      result[i] = snap(local[i], snapDir, skip);
    }

    if constexpr (S == 3)
    {
      result = vm::vec<T, S>{m_gridToWorld * vm::vec3d{result}};
    }
    return result;
  }

public: // Snap towards an arbitrary direction.
  template <typename T, size_t S>
  vm::vec<T, S> snapTowards(
    const vm::vec<T, S>& p, const vm::vec<T, S>& d, const bool skip = false) const
  {
    if (!snap())
    {
      return p;
    }

    auto localPoint = p;
    auto localDirection = d;
    if constexpr (S == 3)
    {
      localPoint = vm::vec<T, S>{m_worldToGrid * vm::vec3d{p}};
      localDirection = vm::vec<T, S>{vm::strip_translation(m_worldToGrid) * vm::vec3d{d}};
    }

    auto result = vm::vec<T, S>{};
    for (size_t i = 0; i < S; ++i)
    {
      result[i] = localDirection[i] > T(0)   ? snap(localPoint[i], SnapDir::Up, skip)
                  : localDirection[i] < T(0) ? snap(localPoint[i], SnapDir::Down, skip)
                                             : snap(localPoint[i], SnapDir::None, skip);
    }

    if constexpr (S == 3)
    {
      result = vm::vec<T, S>{m_gridToWorld * vm::vec3d{result}};
    }
    return result;
  }

public: // Snapping on a plane.
  template <typename T>
  vm::vec<T, 3> snap(const vm::vec<T, 3>& p, const vm::plane<T, 3>& onPlane) const
  {
    return snap(p, onPlane, SnapDir::None, false);
  }

  template <typename T>
  vm::vec<T, 3> snapUp(
    const vm::vec<T, 3>& p, const vm::plane<T, 3>& onPlane, const bool skip = false) const
  {
    return snap(p, onPlane, SnapDir::Up, skip);
  }

  template <typename T>
  vm::vec<T, 3> snapDown(
    const vm::vec<T, 3>& p, const vm::plane<T, 3>& onPlane, const bool skip = false) const
  {
    return snap(p, onPlane, SnapDir::Down, skip);
  }

  template <typename T, size_t S>
  vm::vec<T, S> snapTowards(
    const vm::vec<T, S>& p,
    const vm::plane<T, 3>& onPlane,
    const vm::vec<T, S>& d,
    const bool skip = false) const
  {

    auto localDirection = d;
    if constexpr (S == 3)
    {
      localDirection = vm::vec<T, S>{vm::strip_translation(m_worldToGrid) * vm::vec3d{d}};
    }

    SnapDir snapDirs[S];
    for (size_t i = 0; i < S; ++i)
    {
      snapDirs[i] =
        (localDirection[i] < 0.0   ? SnapDir::Down
         : localDirection[i] > 0.0 ? SnapDir::Up
                                   : SnapDir::None);
    }

    return snap(p, onPlane, snapDirs, skip);
  }

private:
  template <typename T, size_t S>
  vm::vec<T, 3> snap(
    const vm::vec<T, S>& p,
    const vm::plane<T, S>& onPlane,
    const SnapDir snapDir,
    const bool skip = false) const
  {
    SnapDir snapDirs[S];
    for (size_t i = 0; i < S; ++i)
    {
      snapDirs[i] = snapDir;
    }

    return snap(p, onPlane, snapDirs, skip);
  }

  /**
   * Snaps p to grid on the two axes that aren't onPlane's major axis, then projects
   * these two coordinates onto the plane to get the third axis. The resulting point will
   * be on the plane and have two axes snapped to grid.
   */
  template <typename T, size_t S>
  vm::vec<T, S> snap(
    const vm::vec<T, S>& p,
    const vm::plane<T, 3>& onPlane,
    const SnapDir snapDirs[],
    const bool skip = false) const
  {

    auto localPoint = vm::vec<T, S>{m_worldToGrid * vm::vec3d{p}};
    const auto localPlane = onPlane.transform(vm::mat<T, 4, 4>{m_worldToGrid});

    auto result = vm::vec<T, 3>{};
    switch (vm::find_abs_max_component(localPlane.normal))
    {
    case vm::axis::x:
      result[1] = snap(localPoint.y(), snapDirs[1], skip);
      result[2] = snap(localPoint.z(), snapDirs[2], skip);
      result[0] = localPlane.xAt(result.yz());
      break;
    case vm::axis::y:
      result[0] = snap(localPoint.x(), snapDirs[0], skip);
      result[2] = snap(localPoint.z(), snapDirs[2], skip);
      result[1] = localPlane.yAt(result.xz());
      break;
    case vm::axis::z:
      result[0] = snap(localPoint.x(), snapDirs[0], skip);
      result[1] = snap(localPoint.y(), snapDirs[1], skip);
      result[2] = localPlane.zAt(result.xy());
      break;
    }
    return vm::vec<T, S>{m_gridToWorld * vm::vec3d{result}};
  }

public:
  // Snapping on an a line means finding the closest point on a line such that at least
  // one coordinate is on the grid, ignoring a coordinate if the line direction is
  // identical to the corresponding axis.
  template <typename T>
  vm::vec<T, 3> snap(const vm::vec<T, 3>& p, const vm::line<T, 3> line) const
  {
    const auto localPoint = vm::vec<T, 3>{m_worldToGrid * vm::vec3d{p}};
    const auto localLine = line.transform(vm::mat<T, 4, 4>{m_worldToGrid});

    // Project the point onto the line.
    const auto pr = vm::project_point(localLine, localPoint);
    const auto prDist = vm::distance_to_projected_point(localLine, pr);

    auto result = pr;
    auto bestDiff = std::numeric_limits<T>::max();
    for (size_t i = 0; i < 3; ++i)
    {
      if (line.direction[i] != 0.0)
      {
        const auto v = std::array<T, 2>{
          {snapDown(pr[i], false) - localLine.point[i],
           snapUp(pr[i], false) - localLine.point[i]}};
        for (size_t j = 0; j < 2; ++j)
        {
          const auto s = v[j] / localLine.direction[i];
          const auto diff = vm::abs_difference(s, prDist);
          if (diff < bestDiff)
          {
            result = vm::point_at_distance(localLine, s);
            bestDiff = diff;
          }
        }
      }
    }

    return vm::vec<T, 3>{m_gridToWorld * vm::vec3d{result}};
  }

  template <typename T>
  std::optional<vm::vec<T, 3>> snap(
    const vm::vec<T, 3>& p, const vm::segment<T, 3> edge) const
  {
    const auto v = edge.end() - edge.start();
    const auto len = length(v);

    const auto orig = edge.start();
    const auto dir = v / len;

    const auto snapped = snap(p, vm::line<T, 3>(orig, dir));
    const auto dist = vm::dot(dir, snapped - orig);

    return dist >= 0.0 && dist <= len ? std::optional{snapped} : std::nullopt;
  }

  template <typename T>
  std::optional<vm::vec<T, 3>> snap(
    const vm::vec<T, 3>& p,
    const vm::polygon<T, 3>& polygon,
    const vm::vec<T, 3>& normal) const
  {
    contract_pre(polygon.vertexCount() >= 3);

    const auto plane = vm::plane<T, 3>{polygon.vertices().front(), normal};
    auto ps = std::optional{snap(p, plane)};
    auto err = vm::squared_length(p - *ps);

    if (!vm::polygon_contains_point(
          *ps, plane.normal, polygon.vertices().begin(), polygon.vertices().end()))
    {
      ps = std::nullopt;
      err = std::numeric_limits<T>::max();
    }

    auto last = polygon.vertices().begin();
    auto cur = std::next(last);
    auto end = polygon.vertices().end();

    while (cur != end)
    {
      if (const auto cand = snap(p, vm::segment<T, 3>{*last, *cur}))
      {
        const auto cerr = vm::squared_length(p - *cand);
        if (cerr < err)
        {
          ps = cand;
          err = cerr;
        }
      }

      last = cur;
      ++cur;
    }

    return ps;
  }

public:
  double intersectWithRay(const vm::ray3d& ray, size_t skip) const;

  /**
   * Returns a copy of `delta` that snaps the result to grid, if the grid snapping moves
   * the result in the same direction as delta (tested on each axis). Otherwise, returns
   * the original point for that axis.
   */
  vm::vec3d moveDeltaForPoint(const vm::vec3d& point, const vm::vec3d& delta) const;
  vm::vec3d moveDeltaForBounds(
    const vm::plane3d& targetPlane,
    const vm::bbox3d& bounds,
    const vm::bbox3d& worldBounds,
    const vm::ray3d& ray) const;

  /**
   * Given a line and a point X on the line (via the distance from the line's origin),
   * returns the distance to a point Y on the line such that Y is on the intersection of
   * the line with a grid plane, and the distance between X and Y is minimal among all
   * such points.
   */
  double snapToGridPlane(const vm::line3d& line, double distance) const;

  double snapMoveDistanceForFace(const BrushFace& face, double moveDistance) const;

  vm::vec3d referencePoint(const vm::bbox3d& bounds) const;
};

} // namespace tb::mdl
