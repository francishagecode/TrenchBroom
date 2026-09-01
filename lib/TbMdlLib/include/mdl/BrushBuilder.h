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

#include "base/Result.h"
#include "mdl/CircleShape.h"
#include "mdl/Polyhedron3.h"
#include "mdl/SurfaceAttributes.h"
#include "mdl/UvAttributes.h"

#include "vm/bbox.h"
#include "vm/util.h"

#include <string>
#include <vector>

namespace tb::mdl
{
class Brush;
class ModelFactory;
enum class MapFormat;

struct CorridorShape
{
  double wallThickness;
  double cornerRadius;
  size_t cornerSegments;
  double ceilingRecessWidth;
  double ceilingRecessDepth;
  double sideRecessHeight;
  double sideRecessDepth;

  bool operator==(const CorridorShape&) const = default;
};

enum class CorridorBendAngle
{
  Deg45,
  Deg90,
};

enum class CorridorBendDirection
{
  Left,
  Right,
};

enum class ChamberFootprint
{
  Chamfered,
  Octagonal,
  Capsule,
  Wedge,
  Apse,
};

enum class ChamberCeiling
{
  Flat,
  BarrelVault,
  RaisedSpine,
};

struct ChamberShape
{
  ChamberFootprint footprint;
  ChamberCeiling ceiling;
  double wallThickness;
  double cornerSize;
  size_t footprintSegments;
  double ceilingRise;
  size_t ceilingSegments;
  bool openEntrance;
  double entranceWidth;
  double entranceHeight;

  bool operator==(const ChamberShape&) const = default;
};

struct TorusShape
{
  size_t ringSegments;
  size_t tubeSegments;
  /** Ratio of the inner hole diameter to the outer torus diameter. */
  double holeSize;

  bool operator==(const TorusShape&) const = default;
};

class BrushBuilder
{
private:
  MapFormat m_mapFormat;
  const vm::bbox3d m_worldBounds;
  const UvAttributes m_defaultUvAttributes;
  const SurfaceAttributes m_defaultSurfaceAttributes;

public:
  BrushBuilder(MapFormat mapFormat, const vm::bbox3d& worldBounds);
  BrushBuilder(
    MapFormat mapFormat,
    const vm::bbox3d& worldBounds,
    UvAttributes defaultUvAttributes,
    SurfaceAttributes defaultSurfaceAttributes);

  Result<Brush> createCube(double size, const std::string& materialName) const;
  Result<Brush> createCube(
    double size,
    const std::string& leftMaterial,
    const std::string& rightMaterial,
    const std::string& frontMaterial,
    const std::string& backMaterial,
    const std::string& topMaterial,
    const std::string& bottomMaterial) const;

  Result<Brush> createCuboid(
    const vm::vec3d& size, const std::string& materialName) const;
  Result<Brush> createCuboid(
    const vm::vec3d& size,
    const std::string& leftMaterial,
    const std::string& rightMaterial,
    const std::string& frontMaterial,
    const std::string& backMaterial,
    const std::string& topMaterial,
    const std::string& bottomMaterial) const;

  Result<Brush> createCuboid(
    const vm::bbox3d& bounds, const std::string& materialName) const;
  Result<Brush> createCuboid(
    const vm::bbox3d& bounds,
    const std::string& leftMaterial,
    const std::string& rightMaterial,
    const std::string& frontMaterial,
    const std::string& backMaterial,
    const std::string& topMaterial,
    const std::string& bottomMaterial) const;

  Result<Brush> createCylinder(
    const vm::bbox3d& bounds,
    const CircleShape& circleShape,
    vm::axis::type axis,
    const std::string& textureName) const;

  Result<std::vector<Brush>> createHollowCylinder(
    const vm::bbox3d& bounds,
    double thickness,
    const CircleShape& circleShape,
    vm::axis::type axis,
    const std::string& textureName) const;

  /**
   * Creates a solid torus as a ring of convex brush fragments. `axis` is normal to the
   * hole. `ringSegments` controls the number of brushes around the ring, while
   * `tubeSegments` controls the polygonal cross-section of each brush. `holeSize` is the
   * ratio of the inner hole diameter to the outer diameter and must be between zero and
   * one. The torus is scaled to fill `bounds`.
   */
  Result<std::vector<Brush>> createTorus(
    const vm::bbox3d& bounds,
    const TorusShape& torusShape,
    vm::axis::type axis,
    const std::string& textureName) const;

  /**
   * Creates a torus whose tube is a closed shell of convex brush fragments.
   * `thickness` is measured inward from the outer surface and is fitted to constrained
   * bounds so that drag previews remain valid.
   */
  Result<std::vector<Brush>> createHollowTorus(
    const vm::bbox3d& bounds,
    double thickness,
    const TorusShape& torusShape,
    vm::axis::type axis,
    const std::string& textureName) const;

  Result<Brush> createScalableCylinder(
    const vm::bbox3d& bounds,
    size_t precision,
    vm::axis::type axis,
    const std::string& textureName) const;

  /**
   * Creates a semicircular arch as a band of voussoir (wedge) brushes. The arch is the
   * upper half of a hollow cylinder: its flat springing line sits on the bottom of the
   * bounds and it rises to fill them. `axis` is the tunnel (extrusion) direction; the
   * arch rises along the more vertical of the two remaining axes. `thickness` is the wall
   * thickness of the band. The circle shape selects the same modes as the cylinder
   * shapes.
   */
  Result<std::vector<Brush>> createArch(
    const vm::bbox3d& bounds,
    double thickness,
    const CircleShape& circleShape,
    vm::axis::type axis,
    const std::string& textureName) const;

  /**
   * Creates an open-ended corridor shell from convex brush fragments. `axis` is the
   * corridor's extrusion direction. The cross-section remains upright in world Z where
   * possible and has rounded corners, a flat floor, and optional geometric recesses in
   * the ceiling and side walls.
   */
  Result<std::vector<Brush>> createCorridor(
    const vm::bbox3d& bounds,
    const CorridorShape& corridorShape,
    vm::axis::type axis,
    const std::string& textureName) const;

  /**
   * Sweeps a corridor profile through a horizontal 45 or 90 degree arc. The profile
   * starts on the minimum plane of `axis`; the bounds' axis length determines the bend
   * radius. `segmentsPer45Degrees` controls the angular subdivision.
   */
  Result<std::vector<Brush>> createCorridorBend(
    const vm::bbox3d& bounds,
    const CorridorShape& corridorShape,
    vm::axis::type axis,
    CorridorBendAngle angle,
    CorridorBendDirection direction,
    size_t segmentsPer45Degrees,
    const std::string& textureName) const;

  /**
   * Creates a horizontal T connector with three open ends. The stem begins on the
   * minimum plane of `axis`, and the crossbar spans the other horizontal axis at the
   * maximum end of the stem. `corridorWidth` is shared by all three openings.
   */
  Result<std::vector<Brush>> createCorridorTJunction(
    const vm::bbox3d& bounds,
    const CorridorShape& corridorShape,
    vm::axis::type axis,
    double corridorWidth,
    const std::string& textureName) const;

  /**
   * Creates a room shell with a non-rectangular horizontal footprint. The shell has a
   * floor slab, inset perimeter walls, and either a flat, barrel-vaulted, or raised-spine
   * ceiling. `axis` points from the centered entrance toward the far end of the chamber.
   */
  Result<std::vector<Brush>> createChamberShell(
    const vm::bbox3d& bounds,
    const ChamberShape& chamberShape,
    vm::axis::type axis,
    const std::string& textureName) const;

  Result<Brush> createCone(
    const vm::bbox3d& bounds,
    const CircleShape& circleShape,
    vm::axis::type axis,
    const std::string& textureName) const;

  Result<Brush> createUvSphere(
    const vm::bbox3d& bounds,
    const CircleShape& circleShape,
    size_t numRings,
    vm::axis::type axis,
    const std::string& textureName) const;

  Result<Brush> createIcoSphere(
    const vm::bbox3d& bounds, size_t iterations, const std::string& textureName) const;

  Result<Brush> createBrush(
    const std::vector<vm::vec3d>& points, const std::string& materialName) const;
  Result<Brush> createBrush(
    const Polyhedron3& polyhedron, const std::string& materialName) const;
};

} // namespace tb::mdl
