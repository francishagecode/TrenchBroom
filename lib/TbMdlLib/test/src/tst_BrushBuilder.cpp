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

#include "mdl/Brush.h"
#include "mdl/BrushBuilder.h"
#include "mdl/BrushFace.h"
#include "mdl/CatchConfig.h"
#include "mdl/MapFormat.h"
#include "mdl/Matchers.h"
#include "mdl/Polyhedron3.h"

#include "kd/range_fold.h"
#include "kd/ranges/to.h"
#include "kd/result.h"

#include "vm/approx.h"
#include "vm/constants.h"

#include <algorithm>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_contains.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

namespace tb::mdl
{
namespace
{

auto makeFace(const std::tuple<vm::vec3d, vm::vec3d, vm::vec3d>& face)
{
  return BrushFace::create(
           std::get<0>(face),
           std::get<1>(face),
           std::get<2>(face),
           "someName",
           UvAttributes{},
           SurfaceAttributes{},
           MapFormat::Standard)
         | kdl::value();
};

auto makeBrush(const std::vector<std::tuple<vm::vec3d, vm::vec3d, vm::vec3d>>& faces)
{
  return Brush::create(
           vm::bbox3d{8192.0},
           faces | std::views::transform(makeFace) | kdl::ranges::to<std::vector>())
         | kdl::value();
};

auto getMergedBounds(const std::vector<Brush>& brushes)
{
  return kdl::fold_left_first(
    brushes | std::views::transform([](const auto& brush) { return brush.bounds(); }),
    [](const auto& lhs, const auto& rhs) { return vm::merge(lhs, rhs); });
}

} // namespace

TEST_CASE("BrushBuilder")
{
  using Catch::Matchers::Contains;
  using Catch::Matchers::RangeEquals;

  const auto worldBounds = vm::bbox3d{8192.0};
  const auto vertexEpsilon = vm::Cd::almost_zero();

  SECTION("createCube")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};

    builder.createCube(128.0, "someName") | kdl::transform([](const auto& cube) {
      CHECK(cube.fullySpecified());
      CHECK(cube.bounds() == vm::bbox3d{-64.0, +64.0});

      CHECK_THAT(
        cube.faces()
          | std::views::transform([](const auto& face) { return face.materialName(); }),
        RangeEquals(std::vector<std::string>{6u, "someName"}));
    }) | kdl::transform_error([](const auto& e) { FAIL(e); });
  }

  SECTION("createCubeDefaults")
  {
    const auto defaultUvAttributes = UvAttributes{
      .offset = {0.5f, 0.5f},
      .scale = {0.5f, 0.5f},
      .rotation = 45.0f,
    };
    const auto defaultSurfaceAttributes = SurfaceAttributes{
      .contents = 1,
      .flags = 2,
      .value = 0.1f,
      .color = RgbB{255, 255, 255},
    };

    auto builder = BrushBuilder{
      MapFormat::Standard, worldBounds, defaultUvAttributes, defaultSurfaceAttributes};

    builder.createCube(128.0, "someName") | kdl::transform([&](const auto& cube) {
      CHECK(cube.fullySpecified());
      CHECK(cube.bounds() == vm::bbox3d{-64.0, +64.0});

      CHECK_THAT(
        cube.faces()
          | std::views::transform([](const auto& face) { return face.materialName(); }),
        RangeEquals(std::vector<std::string>{6u, "someName"}));
      CHECK_THAT(
        cube.faces()
          | std::views::transform([](const auto& face) { return face.uvAttributes(); }),
        RangeEquals(std::vector{6u, defaultUvAttributes}));
      CHECK_THAT(
        cube.faces() | std::views::transform([](const auto& face) {
          return face.surfaceAttributes();
        }),
        RangeEquals(std::vector{6u, defaultSurfaceAttributes}));
    }) | kdl::transform_error([](const auto& e) { FAIL(e); });
  }

  SECTION("createBrushDefaults")
  {
    const auto defaultUvAttributes = UvAttributes{
      .offset = {0.5f, 0.5f},
      .scale = {0.5f, 0.5f},
      .rotation = 45.0f,
    };
    const auto defaultSurfaceAttributes = SurfaceAttributes{
      .contents = 1,
      .flags = 2,
      .value = 0.1f,
      .color = RgbB{255, 255, 255},
    };

    auto builder = BrushBuilder{
      MapFormat::Standard, worldBounds, defaultUvAttributes, defaultSurfaceAttributes};

    builder.createBrush(
      Polyhedron3{
        vm::vec3d{-64, -64, -64},
        vm::vec3d{-64, -64, +64},
        vm::vec3d{-64, +64, -64},
        vm::vec3d{-64, +64, +64},
        vm::vec3d{+64, -64, -64},
        vm::vec3d{+64, -64, +64},
        vm::vec3d{+64, +64, -64},
        vm::vec3d{+64, +64, +64},
      },
      "someName")
      | kdl::transform([&](const auto& brush) {
          CHECK(brush.fullySpecified());
          CHECK(brush.bounds() == vm::bbox3d{-64.0, +64.0});

          CHECK_THAT(
            brush.faces() | std::views::transform([](const auto& face) {
              return face.materialName();
            }),
            RangeEquals(std::vector<std::string>{6u, "someName"}));
          CHECK_THAT(
            brush.faces() | std::views::transform([](const auto& face) {
              return face.uvAttributes();
            }),
            RangeEquals(std::vector{6u, defaultUvAttributes}));
          CHECK_THAT(
            brush.faces() | std::views::transform([](const auto& face) {
              return face.surfaceAttributes();
            }),
            RangeEquals(std::vector{6u, defaultSurfaceAttributes}));
        })
      | kdl::transform_error([](const auto& e) { FAIL(e); });
  }

  SECTION("createCylinder")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};

    SECTION("Edge aligned cylinder")
    {
      builder.createCylinder(
        vm::bbox3d{{-32, -32, -32}, {32, 32, 32}},
        EdgeAlignedCircle{4},
        vm::axis::z,
        "someName")
        | kdl::transform([](const auto& cylinder) {
            CHECK(cylinder.bounds() == vm::bbox3d{{-32, -32, -32}, {32, 32, 32}});

            CHECK(
              cylinder
              == makeBrush({
                {{-32, -32, 32}, {-32, 32, -32}, {-32, 32, 32}},
                {{32, -32, 32}, {-32, -32, -32}, {-32, -32, 32}},
                {{32, 32, -32}, {-32, -32, -32}, {32, -32, -32}},
                {{32, 32, 32}, {-32, -32, 32}, {-32, 32, 32}},
                {{32, 32, 32}, {-32, 32, -32}, {32, 32, -32}},
                {{32, 32, 32}, {32, -32, -32}, {32, -32, 32}},
              }));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Scalable Cylinder")
    {
      SECTION("In square bounds")
      {
        builder.createCylinder(
          vm::bbox3d{{-32, -32, -32}, {32, 32, 32}},
          ScalableCircle{0},
          vm::axis::z,
          "someName")
          | kdl::transform([](const auto& cylinder) {
              CHECK(cylinder.bounds() == vm::bbox3d{{-32, -32, -32}, {32, 32, 32}});

              CHECK(
                cylinder
                == makeBrush({
                  {{-32, -8, 32}, {-32, 8, -32}, {-32, 8, 32}},
                  {{-24, -24, 32}, {-32, -8, -32}, {-32, -8, 32}},
                  {{-24, 24, 32}, {-32, 8, -32}, {-24, 24, -32}},
                  {{-8, -32, 32}, {-24, -24, -32}, {-24, -24, 32}},
                  {{-8, 32, 32}, {-24, 24, -32}, {-8, 32, -32}},
                  {{8, -32, 32}, {-8, -32, -32}, {-8, -32, 32}},
                  {{32, 8, -32}, {24, -24, -32}, {32, -8, -32}},
                  {{32, 8, 32}, {8, 32, 32}, {24, 24, 32}},
                  {{8, 32, 32}, {-8, 32, -32}, {8, 32, -32}},
                  {{24, -24, 32}, {8, -32, -32}, {8, -32, 32}},
                  {{24, 24, 32}, {8, 32, -32}, {24, 24, -32}},
                  {{32, -8, 32}, {24, -24, -32}, {24, -24, 32}},
                  {{32, 8, 32}, {24, 24, -32}, {32, 8, -32}},
                  {{32, 8, 32}, {32, -8, -32}, {32, -8, 32}},
                }));
            })
          | kdl::transform_error([](const auto& e) { FAIL(e); });
      }

      SECTION("In rectangular bounds")
      {
        builder.createCylinder(
          vm::bbox3d{{-64, -32, -32}, {64, 32, 32}},
          ScalableCircle{0},
          vm::axis::z,
          "someName")
          | kdl::transform([](const auto& cylinder) {
              CHECK(cylinder.bounds() == vm::bbox3d{{-64, -32, -32}, {64, 32, 32}});

              CHECK(
                cylinder
                == makeBrush({
                  {{-64, -8, 32}, {-64, 8, -32}, {-64, 8, 32}},
                  {{-56, -24, 32}, {-64, -8, -32}, {-64, -8, 32}},
                  {{-56, 24, 32}, {-64, 8, -32}, {-56, 24, -32}},
                  {{-40, -32, 32}, {-56, -24, -32}, {-56, -24, 32}},
                  {{-40, 32, 32}, {-56, 24, -32}, {-40, 32, -32}},
                  {{40, -32, 32}, {-40, -32, -32}, {-40, -32, 32}},
                  {{64, 8, -32}, {56, -24, -32}, {64, -8, -32}},
                  {{64, 8, 32}, {40, 32, 32}, {56, 24, 32}},
                  {{40, 32, 32}, {-40, 32, -32}, {40, 32, -32}},
                  {{56, -24, 32}, {40, -32, -32}, {40, -32, 32}},
                  {{56, 24, 32}, {40, 32, -32}, {56, 24, -32}},
                  {{64, -8, 32}, {56, -24, -32}, {56, -24, 32}},
                  {{64, 8, 32}, {56, 24, -32}, {64, 8, -32}},
                  {{64, 8, 32}, {64, -8, -32}, {64, -8, 32}},
                }));
            })
          | kdl::transform_error([](const auto& e) { FAIL(e); });
      }
    }
  }

  SECTION("createHollowCylinder")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};

    const auto bounds = vm::bbox3d{{-32, -32, -32}, {32, 32, 32}};
    builder.createHollowCylinder(
      bounds, 8.0, EdgeAlignedCircle{8}, vm::axis::z, "someName")
      | kdl::transform([&](const auto& brushes) {
          REQUIRE(brushes.size() == 8u);
          CHECK(getMergedBounds(brushes) == bounds);

          // Check only one brush to avoid clutter.
          const auto outerOffset = 13.254833995939043;
          const auto innerMin = -9.9411254969542853;
          const auto innerMax = 9.9411254969542835;
          const auto expectedBrush = makeBrush({
            {{24, innerMin, 32}, {24, innerMax, -32}, {24, innerMax, 32}},
            {{24, innerMax, -32}, {32, outerOffset, 32}, {24, innerMax, 32}},
            {{32, -outerOffset, 32}, {24, innerMin, -32}, {24, innerMin, 32}},
            {{32, outerOffset, -32}, {24, innerMin, -32}, {32, -outerOffset, -32}},
            {{32, outerOffset, 32}, {24, innerMin, 32}, {24, innerMax, 32}},
            {{32, outerOffset, 32}, {32, -outerOffset, -32}, {32, -outerOffset, 32}},
          });

          CHECK_THAT(
            brushes, Contains(MatchesBrushVertices(expectedBrush, vertexEpsilon)));
        })
      | kdl::transform_error([](const auto& e) { FAIL(e); });
  }

  SECTION("createArch")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};

    SECTION("Arch with different circle modes")
    {
      const auto bounds = vm::bbox3d{{-128, -64, 0}, {128, 64, 64}};

      SECTION("Edge aligned")
      {
        builder.createArch(bounds, 16.0, EdgeAlignedCircle{16}, vm::axis::y, "someName")
          | kdl::transform([&](const auto& brushes) {
              REQUIRE(brushes.size() == 9u);
              CHECK(getMergedBounds(brushes) == bounds);

              // Check only one brush to avoid clutter.
              const auto expectedBrush = makeBrush({
                {{112, -64, 6.9638421181958083},
                 {112, 64, 0},
                 {112, 64, 6.9638421181958083}},
                {{128, 64, 12.730391512298111},
                 {112, -64, 6.9638421181958083},
                 {112, 64, 6.9638421181958083}},
                {{128, -64, 12.730391512298111},
                 {112, -64, 0},
                 {112, -64, 6.9638421181958083}},
                {{128, 64, 0}, {112, -64, 0}, {128, -64, 0}},
                {{128, 64, 12.730391512298111}, {112, 64, 0}, {128, 64, 0}},
                {{128, 64, 12.730391512298111},
                 {128, -64, 0},
                 {128, -64, 12.730391512298111}},
              });

              CHECK_THAT(
                brushes, Contains(MatchesBrushVertices(expectedBrush, vertexEpsilon)));
            })
          | kdl::transform_error([](const auto& e) { FAIL(e); });
      }

      SECTION("Vertex aligned")
      {
        builder.createArch(bounds, 16.0, VertexAlignedCircle{16}, vm::axis::y, "someName")
          | kdl::transform([&](const auto& brushes) {
              REQUIRE(brushes.size() == 8u);
              CHECK(getMergedBounds(brushes) == bounds);

              // Check only one brush to avoid clutter.
              const auto expectedBrush = makeBrush({
                {{105.05776674599576, 64, 14.384730312563047},
                 {110.78036826717545, -64, 0},
                 {110.78036826717545, 64, 0}},
                {{118.2565801614447, 64, 24.491739671365746},
                 {105.05776674599576, -64, 14.384730312563047},
                 {105.05776674599576, 64, 14.384730312563047}},
                {{128, -64, 0},
                 {105.05776674599576, -64, 14.384730312563047},
                 {118.2565801614447, -64, 24.491739671365746}},
                {{128, 64, 0}, {110.78036826717545, -64, 0}, {128, -64, 0}},
                {{128, 64, 0},
                 {105.05776674599576, 64, 14.384730312563047},
                 {110.78036826717545, 64, 0}},
                {{128, 64, 0},
                 {118.2565801614447, -64, 24.491739671365746},
                 {118.2565801614447, 64, 24.491739671365746}},
              });

              CHECK_THAT(
                brushes, Contains(MatchesBrushVertices(expectedBrush, vertexEpsilon)));
            })
          | kdl::transform_error([](const auto& e) { FAIL(e); });
      }

      SECTION("Scalable")
      {
        builder.createArch(bounds, 16.0, ScalableCircle{0}, vm::axis::y, "someName")
          | kdl::transform([&](const auto& brushes) {
              REQUIRE(brushes.size() == 7u);
              CHECK(getMergedBounds(brushes) == bounds);

              // Check only one brush to avoid clutter.
              const auto expectedBrush = makeBrush({
                {{112, -64, 12}, {112, 64, 0}, {112, 64, 12}},
                {{128, 64, 16}, {112, -64, 12}, {112, 64, 12}},
                {{128, -64, 16}, {112, -64, 0}, {112, -64, 12}},
                {{128, 64, 0}, {112, -64, 0}, {128, -64, 0}},
                {{128, 64, 16}, {112, 64, 0}, {128, 64, 0}},
                {{128, 64, 16}, {128, -64, 0}, {128, -64, 16}},
              });

              CHECK(std::ranges::find(brushes, expectedBrush) != brushes.end());
            })
          | kdl::transform_error([](const auto& e) { FAIL(e); });
      }
    }

    SECTION("Arch with different tunnel axes")
    {
      SECTION("X axis")
      {
        const auto bounds = vm::bbox3d{{-64, -128, 0}, {64, 128, 64}};
        builder.createArch(bounds, 16.0, EdgeAlignedCircle{8}, vm::axis::x, "someName")
          | kdl::transform([&](const auto& brushes) {
              REQUIRE(brushes.size() == 5u);
              CHECK(getMergedBounds(brushes) == bounds);

              // Check only one brush to avoid clutter.
              const auto expectedBrush = makeBrush({
                {{-64, 112, 16.621124171879767},
                 {-64, 128, 0},
                 {-64, 128, 26.509667991878086}},
                {{64, 112, 16.621124171879767},
                 {-64, 112, 0},
                 {-64, 112, 16.621124171879767}},
                {{64, 128, 26.509667991878086},
                 {-64, 112, 16.621124171879767},
                 {-64, 128, 26.509667991878086}},
                {{64, 128, 0}, {-64, 112, 0}, {64, 112, 0}},
                {{64, 128, 26.509667991878086}, {-64, 128, 0}, {64, 128, 0}},
                {{64, 128, 26.509667991878086},
                 {64, 112, 0},
                 {64, 112, 16.621124171879767}},
              });

              CHECK_THAT(
                brushes, Contains(MatchesBrushVertices(expectedBrush, vertexEpsilon)));
            })
          | kdl::transform_error([](const auto& e) { FAIL(e); });
      }

      SECTION("Y axis")
      {
        const auto bounds = vm::bbox3d{{-128, -64, 0}, {128, 64, 64}};
        builder.createArch(bounds, 16.0, EdgeAlignedCircle{8}, vm::axis::y, "someName")
          | kdl::transform([&](const auto& brushes) {
              REQUIRE(brushes.size() == 5u);
              CHECK(getMergedBounds(brushes) == bounds);

              // Check only one brush to avoid clutter.
              const auto expectedBrush = makeBrush({
                {{112, -64, 16.621124171879767},
                 {112, 64, 0},
                 {112, 64, 16.621124171879767}},
                {{128, 64, 26.509667991878086},
                 {112, -64, 16.621124171879767},
                 {112, 64, 16.621124171879767}},
                {{128, -64, 26.509667991878086},
                 {112, -64, 0},
                 {112, -64, 16.621124171879767}},
                {{128, 64, 0}, {112, -64, 0}, {128, -64, 0}},
                {{128, 64, 26.509667991878086}, {112, 64, 0}, {128, 64, 0}},
                {{128, 64, 26.509667991878086},
                 {128, -64, 0},
                 {128, -64, 26.509667991878086}},
              });

              CHECK_THAT(
                brushes, Contains(MatchesBrushVertices(expectedBrush, vertexEpsilon)));
            })
          | kdl::transform_error([](const auto& e) { FAIL(e); });
      }
    }

    SECTION("Degenerate bounds do not error")
    {
      SECTION("Zero height")
      {
        CHECK(
          builder.createArch(
            vm::bbox3d{{-128, -64, 0}, {128, 64, 0}},
            16.0,
            EdgeAlignedCircle{12},
            vm::axis::x,
            "someName")
          == Result<std::vector<Brush>>{std::vector<Brush>{}});
      }

      SECTION("Zero span")
      {
        CHECK(
          builder.createArch(
            vm::bbox3d{{-128, 0, 0}, {128, 0, 64}},
            16.0,
            EdgeAlignedCircle{12},
            vm::axis::x,
            "someName")
          == Result<std::vector<Brush>>{std::vector<Brush>{}});
      }

      SECTION("Zero extrusion depth")
      {
        CHECK(
          builder.createArch(
            vm::bbox3d{{0, -64, 0}, {0, 64, 64}},
            16.0,
            EdgeAlignedCircle{12},
            vm::axis::x,
            "someName")
          == Result<std::vector<Brush>>{std::vector<Brush>{}});
      }
    }
  }

  SECTION("createCorridor")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};
    const auto bounds = vm::bbox3d{{-128, -96, 0}, {128, 96, 128}};
    const auto basicShape = CorridorShape{
      .wallThickness = 16.0,
      .cornerRadius = 48.0,
      .cornerSegments = 2u,
      .ceilingRecessWidth = 0.0,
      .ceilingRecessDepth = 0.0,
      .sideRecessHeight = 0.0,
      .sideRecessDepth = 0.0,
    };

    SECTION("Rounded shell")
    {
      builder.createCorridor(bounds, basicShape, vm::axis::x, "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(brushes.size() == 12u);
            CHECK(getMergedBounds(brushes) == bounds);
            CHECK(std::ranges::all_of(brushes, [](const auto& brush) {
              return brush.fullySpecified();
            }));
            CHECK(std::ranges::all_of(brushes, [](const auto& brush) {
              return std::ranges::all_of(brush.faces(), [](const auto& face) {
                return face.materialName() == "someName";
              });
            }));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Ceiling and side recesses")
    {
      const auto recessedShape = CorridorShape{
        .wallThickness = 16.0,
        .cornerRadius = 32.0,
        .cornerSegments = 2u,
        .ceilingRecessWidth = 64.0,
        .ceilingRecessDepth = 8.0,
        .sideRecessHeight = 48.0,
        .sideRecessDepth = 8.0,
      };

      builder.createCorridor(bounds, recessedShape, vm::axis::x, "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(brushes.size() == 24u);
            CHECK(getMergedBounds(brushes) == bounds);
            CHECK(std::ranges::any_of(brushes, [](const auto& brush) {
              return brush.hasVertex({-128, 32, 120});
            }));
            CHECK(std::ranges::any_of(brushes, [](const auto& brush) {
              return brush.hasVertex({-128, -88, 88});
            }));
            CHECK(std::ranges::any_of(brushes, [](const auto& brush) {
              return brush.hasVertex({-128, 88, 40});
            }));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Tunnel axis")
    {
      const auto yBounds = vm::bbox3d{{-96, -128, 0}, {96, 128, 128}};
      builder.createCorridor(yBounds, basicShape, vm::axis::y, "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(brushes.size() == 12u);
            CHECK(getMergedBounds(brushes) == yBounds);
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Radius is clamped to bounds without degenerate fragments")
    {
      auto largeRadiusShape = basicShape;
      largeRadiusShape.cornerRadius = 128.0;
      const auto narrowBounds = vm::bbox3d{{-128, -64, 0}, {128, 64, 256}};

      builder.createCorridor(narrowBounds, largeRadiusShape, vm::axis::x, "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(brushes.size() == 10u);
            CHECK(getMergedBounds(brushes) == narrowBounds);
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Degenerate and undersized bounds do not error")
    {
      CHECK(
        builder.createCorridor(
          vm::bbox3d{{0, -96, 0}, {0, 96, 128}}, basicShape, vm::axis::x, "someName")
        == Result<std::vector<Brush>>{std::vector<Brush>{}});
      CHECK(
        builder.createCorridor(
          vm::bbox3d{{-128, -8, 0}, {128, 8, 128}}, basicShape, vm::axis::x, "someName")
        == Result<std::vector<Brush>>{std::vector<Brush>{}});
    }

    SECTION("Invalid shape parameters error")
    {
      auto invalidShape = basicShape;
      invalidShape.wallThickness = 0.0;
      CHECK(
        builder.createCorridor(bounds, invalidShape, vm::axis::x, "someName").is_error());

      invalidShape = basicShape;
      invalidShape.cornerRadius = 0.0;
      CHECK(
        builder.createCorridor(bounds, invalidShape, vm::axis::x, "someName").is_error());

      invalidShape = basicShape;
      invalidShape.cornerSegments = 0u;
      CHECK(
        builder.createCorridor(bounds, invalidShape, vm::axis::x, "someName").is_error());

      invalidShape = basicShape;
      invalidShape.ceilingRecessWidth = 64.0;
      invalidShape.ceilingRecessDepth = 16.0;
      CHECK(
        builder.createCorridor(bounds, invalidShape, vm::axis::x, "someName").is_error());
    }
  }

  SECTION("createCorridorBend")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};
    const auto bounds = vm::bbox3d{{-128, -96, 0}, {128, 96, 128}};
    const auto shape = CorridorShape{
      .wallThickness = 16.0,
      .cornerRadius = 48.0,
      .cornerSegments = 2u,
      .ceilingRecessWidth = 0.0,
      .ceilingRecessDepth = 0.0,
      .sideRecessHeight = 0.0,
      .sideRecessDepth = 0.0,
    };

    SECTION("45 degree left and right bends")
    {
      const auto bendAngle = vm::Cd::quarter_pi();
      const auto bendRadius =
        bounds.size().x() / std::sin(bendAngle) - bounds.size().y() / 2.0;
      const auto lateralExtent = bendRadius * (1.0 - std::cos(bendAngle))
                                 + bounds.size().y() / 2.0 * std::cos(bendAngle);

      builder.createCorridorBend(
        bounds,
        shape,
        vm::axis::x,
        CorridorBendAngle::Deg45,
        CorridorBendDirection::Left,
        3u,
        "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(brushes.size() == 36u);
            CHECK(
              vm::approx{
                vm::bbox3d{{-128, -96, 0}, {128, lateralExtent, 128}}, vertexEpsilon}
              == getMergedBounds(brushes));
            CHECK(std::ranges::all_of(brushes, [](const auto& brush) {
              return brush.fullySpecified();
            }));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });

      builder.createCorridorBend(
        bounds,
        shape,
        vm::axis::x,
        CorridorBendAngle::Deg45,
        CorridorBendDirection::Right,
        3u,
        "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(brushes.size() == 36u);
            CHECK(
              vm::approx{
                vm::bbox3d{{-128, -lateralExtent, 0}, {128, 96, 128}}, vertexEpsilon}
              == getMergedBounds(brushes));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("90 degree bend")
    {
      builder.createCorridorBend(
        bounds,
        shape,
        vm::axis::x,
        CorridorBendAngle::Deg90,
        CorridorBendDirection::Left,
        3u,
        "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(brushes.size() == 72u);
            CHECK(
              vm::approx{vm::bbox3d{{-128, -96, 0}, {128, 160, 128}}, vertexEpsilon}
              == getMergedBounds(brushes));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Y axis left turn follows world-space left")
    {
      const auto yBounds = vm::bbox3d{{-96, -128, 0}, {96, 128, 128}};
      const auto bendAngle = vm::Cd::quarter_pi();
      const auto bendRadius =
        yBounds.size().y() / std::sin(bendAngle) - yBounds.size().x() / 2.0;
      const auto lateralExtent = bendRadius * (1.0 - std::cos(bendAngle))
                                 + yBounds.size().x() / 2.0 * std::cos(bendAngle);

      builder.createCorridorBend(
        yBounds,
        shape,
        vm::axis::y,
        CorridorBendAngle::Deg45,
        CorridorBendDirection::Left,
        3u,
        "someName")
        | kdl::transform([&](const auto& brushes) {
            CHECK(
              vm::approx{
                vm::bbox3d{{-lateralExtent, -128, 0}, {96, 128, 128}}, vertexEpsilon}
              == getMergedBounds(brushes));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Invalid or undersized bend settings")
    {
      CHECK(builder
              .createCorridorBend(
                bounds,
                shape,
                vm::axis::z,
                CorridorBendAngle::Deg45,
                CorridorBendDirection::Left,
                3u,
                "someName")
              .is_error());
      CHECK(builder
              .createCorridorBend(
                bounds,
                shape,
                vm::axis::x,
                CorridorBendAngle::Deg45,
                CorridorBendDirection::Left,
                0u,
                "someName")
              .is_error());
      CHECK(
        builder.createCorridorBend(
          vm::bbox3d{{-64, -96, 0}, {64, 96, 128}},
          shape,
          vm::axis::x,
          CorridorBendAngle::Deg90,
          CorridorBendDirection::Left,
          3u,
          "someName")
        == Result<std::vector<Brush>>{std::vector<Brush>{}});
    }
  }

  SECTION("createCorridorTJunction")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};
    const auto bounds = vm::bbox3d{{-256, -384, 0}, {256, 384, 160}};
    const auto shape = CorridorShape{
      .wallThickness = 16.0,
      .cornerRadius = 32.0,
      .cornerSegments = 2u,
      .ceilingRecessWidth = 64.0,
      .ceilingRecessDepth = 8.0,
      .sideRecessHeight = 48.0,
      .sideRecessDepth = 8.0,
    };

    SECTION("Three open ends and branch opening")
    {
      builder.createCorridorTJunction(bounds, shape, vm::axis::x, 256.0, "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(brushes.size() == 87u);
            CHECK(getMergedBounds(brushes) == bounds);
            CHECK(std::ranges::any_of(brushes, [](const auto& brush) {
              return brush.hasVertex({-256, 32, 152});
            }));
            CHECK(std::ranges::any_of(brushes, [](const auto& brush) {
              return brush.hasVertex({160, -384, 152});
            }));
            CHECK(std::ranges::any_of(brushes, [](const auto& brush) {
              return brush.hasVertex({160, 384, 152});
            }));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Y axis")
    {
      const auto yBounds = vm::bbox3d{{-384, -256, 0}, {384, 256, 160}};
      builder.createCorridorTJunction(yBounds, shape, vm::axis::y, 256.0, "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(brushes.size() == 87u);
            CHECK(getMergedBounds(brushes) == yBounds);
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Invalid or undersized junction settings")
    {
      CHECK(builder.createCorridorTJunction(bounds, shape, vm::axis::z, 256.0, "someName")
              .is_error());
      CHECK(builder.createCorridorTJunction(bounds, shape, vm::axis::x, 0.0, "someName")
              .is_error());
      CHECK(
        builder.createCorridorTJunction(
          vm::bbox3d{{-128, -128, 0}, {128, 128, 160}},
          shape,
          vm::axis::x,
          256.0,
          "someName")
        == Result<std::vector<Brush>>{std::vector<Brush>{}});
    }
  }

  SECTION("createChamberShell")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};
    const auto bounds = vm::bbox3d{{-384, -256, 0}, {384, 256, 256}};
    const auto shape = ChamberShape{
      .footprint = ChamberFootprint::Chamfered,
      .ceiling = ChamberCeiling::Flat,
      .wallThickness = 16.0,
      .cornerSize = 64.0,
      .footprintSegments = 3u,
      .ceilingRise = 64.0,
      .ceilingSegments = 4u,
      .openEntrance = true,
      .entranceWidth = 224.0,
      .entranceHeight = 128.0,
    };

    SECTION("Non-square footprints")
    {
      const auto footprint = GENERATE(
        ChamberFootprint::Chamfered,
        ChamberFootprint::Octagonal,
        ChamberFootprint::Capsule,
        ChamberFootprint::Wedge,
        ChamberFootprint::Apse);
      CAPTURE(footprint);

      auto footprintShape = shape;
      footprintShape.footprint = footprint;
      builder.createChamberShell(bounds, footprintShape, vm::axis::x, "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(!brushes.empty());
            CHECK(getMergedBounds(brushes) == bounds);
            CHECK(std::ranges::none_of(brushes, [](const auto& brush) {
              return brush.containsPoint({-376, 0, 80});
            }));
            CHECK(std::ranges::none_of(brushes, [](const auto& brush) {
              return brush.containsPoint({0, 0, 80});
            }));
            CHECK(std::ranges::all_of(brushes, [](const auto& brush) {
              return std::ranges::all_of(brush.faces(), [](const auto& face) {
                return face.materialName() == "someName";
              });
            }));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Ceiling profiles")
    {
      const auto ceiling = GENERATE(
        ChamberCeiling::Flat, ChamberCeiling::BarrelVault, ChamberCeiling::RaisedSpine);
      CAPTURE(ceiling);

      auto ceilingShape = shape;
      ceilingShape.ceiling = ceiling;
      builder.createChamberShell(bounds, ceilingShape, vm::axis::x, "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(!brushes.empty());
            CHECK(getMergedBounds(brushes) == bounds);
            if (ceiling == ChamberCeiling::Flat)
            {
              CHECK(brushes.size() == 12u);
            }
            else
            {
              CHECK(brushes.size() == 19u);
              CHECK(std::ranges::none_of(brushes, [](const auto& brush) {
                return brush.containsPoint({0, 0, 230});
              }));
              CHECK(std::ranges::any_of(brushes, [](const auto& brush) {
                return brush.containsPoint({0, 0, 248});
              }));
              CHECK(std::ranges::any_of(brushes, [](const auto& brush) {
                return brush.containsPoint({376, 0, 230});
              }));
            }
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Default corridor-sized entrance")
    {
      const auto corridorSizedBounds = vm::bbox3d{{0, 0, 0}, {256, 256, 160}};
      builder.createChamberShell(corridorSizedBounds, shape, vm::axis::x, "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(!brushes.empty());
            CHECK(getMergedBounds(brushes) == corridorSizedBounds);
            CHECK(std::ranges::none_of(brushes, [](const auto& brush) {
              return brush.containsPoint({8, 128, 80});
            }));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Closed shell")
    {
      auto closedShape = shape;
      closedShape.openEntrance = false;
      builder.createChamberShell(bounds, closedShape, vm::axis::x, "someName")
        | kdl::transform([&](const auto& brushes) {
            CHECK(brushes.size() == 10u);
            CHECK(std::ranges::any_of(brushes, [](const auto& brush) {
              return brush.containsPoint({-376, 0, 80});
            }));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Y axis")
    {
      builder.createChamberShell(bounds, shape, vm::axis::y, "someName")
        | kdl::transform([&](const auto& brushes) {
            REQUIRE(!brushes.empty());
            CHECK(getMergedBounds(brushes) == bounds);
            CHECK(std::ranges::none_of(brushes, [](const auto& brush) {
              return brush.containsPoint({0, -248, 80});
            }));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("Invalid settings and constrained drag previews")
    {
      CHECK(
        builder.createChamberShell(bounds, shape, vm::axis::z, "someName").is_error());

      auto invalidShape = shape;
      invalidShape.wallThickness = 0.0;
      CHECK(builder.createChamberShell(bounds, invalidShape, vm::axis::x, "someName")
              .is_error());

      invalidShape = shape;
      invalidShape.footprintSegments = 0u;
      CHECK(builder.createChamberShell(bounds, invalidShape, vm::axis::x, "someName")
              .is_error());

      for (const auto& constrainedBounds : {
             vm::bbox3d{{-16, -16, 0}, {16, 16, 32}},
             vm::bbox3d{{-8, -8, 0}, {8, 8, 16}},
           })
      {
        builder.createChamberShell(constrainedBounds, shape, vm::axis::x, "someName")
          | kdl::transform([&](const auto& brushes) {
              REQUIRE(!brushes.empty());
              CHECK(getMergedBounds(brushes) == constrainedBounds);
            })
          | kdl::transform_error([](const auto& e) { FAIL(e); });
      }
    }

    SECTION("Grid-sized drag bounds")
    {
      const auto footprints = std::array{
        ChamberFootprint::Chamfered,
        ChamberFootprint::Octagonal,
        ChamberFootprint::Capsule,
        ChamberFootprint::Wedge,
        ChamberFootprint::Apse,
      };
      const auto ceilings = std::array{
        ChamberCeiling::Flat,
        ChamberCeiling::BarrelVault,
        ChamberCeiling::RaisedSpine,
      };
      const auto axes = std::array{vm::axis::x, vm::axis::y};
      const auto dragSizes = std::array{
        vm::vec3d{16, 16, 16},
        vm::vec3d{64, 96, 48},
        vm::vec3d{256, 256, 160},
        vm::vec3d{512, 384, 320},
      };

      for (const auto footprint : footprints)
      {
        for (const auto ceiling : ceilings)
        {
          for (const auto axis : axes)
          {
            for (const auto& dragSize : dragSizes)
            {
              auto dragShape = shape;
              dragShape.footprint = footprint;
              dragShape.ceiling = ceiling;
              const auto dragBounds = vm::bbox3d{{0, 0, 0}, dragSize};
              CAPTURE(footprint, ceiling, axis, dragSize);

              builder.createChamberShell(dragBounds, dragShape, axis, "someName")
                | kdl::transform([&](const auto& brushes) {
                    REQUIRE(!brushes.empty());
                    CHECK(getMergedBounds(brushes) == dragBounds);
                  })
                | kdl::transform_error([](const auto& e) { FAIL(e); });
            }
          }
        }
      }
    }
  }

  SECTION("createCuboid")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};

    SECTION("from a size")
    {
      // the cuboid is centered at the origin
      builder.createCuboid(vm::vec3d{64, 128, 32}, "someName")
        | kdl::transform([](const auto& cuboid) {
            CHECK(cuboid.fullySpecified());
            CHECK(cuboid.bounds() == vm::bbox3d{{-32, -64, -16}, {32, 64, 16}});

            CHECK_THAT(
              cuboid.faces() | std::views::transform([](const auto& face) {
                return face.materialName();
              }),
              RangeEquals(std::vector<std::string>{6u, "someName"}));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("from bounds")
    {
      const auto bounds = vm::bbox3d{{-16, -32, -64}, {32, 64, 128}};
      builder.createCuboid(bounds, "someName") | kdl::transform([&](const auto& cuboid) {
        CHECK(cuboid.fullySpecified());
        CHECK(cuboid.bounds() == bounds);
      }) | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("with a material per face")
    {
      const auto materialAt = [](const auto& cuboid, const vm::vec3d& normal) {
        const auto faceIndex = cuboid.findFace(normal);
        REQUIRE(faceIndex);
        return cuboid.face(*faceIndex).materialName();
      };

      const auto checkMaterials = [&](const auto& cuboid) {
        CHECK(materialAt(cuboid, vm::vec3d{-1, 0, 0}) == "left");
        CHECK(materialAt(cuboid, vm::vec3d{+1, 0, 0}) == "right");
        CHECK(materialAt(cuboid, vm::vec3d{0, -1, 0}) == "front");
        CHECK(materialAt(cuboid, vm::vec3d{0, +1, 0}) == "back");
        CHECK(materialAt(cuboid, vm::vec3d{0, 0, +1}) == "top");
        CHECK(materialAt(cuboid, vm::vec3d{0, 0, -1}) == "bottom");
      };

      SECTION("from a size")
      {
        builder.createCuboid(
          vm::vec3d{64, 64, 64}, "left", "right", "front", "back", "top", "bottom")
          | kdl::transform(checkMaterials)
          | kdl::transform_error([](const auto& e) { FAIL(e); });
      }

      SECTION("from bounds")
      {
        builder.createCuboid(
          vm::bbox3d{{-16, -32, -64}, {32, 64, 128}},
          "left",
          "right",
          "front",
          "back",
          "top",
          "bottom")
          | kdl::transform(checkMaterials)
          | kdl::transform_error([](const auto& e) { FAIL(e); });
      }
    }
  }

  SECTION("createCone")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};

    const auto bounds = vm::bbox3d{{-32, -32, -32}, {32, 32, 32}};

    SECTION("edge aligned")
    {
      builder.createCone(bounds, EdgeAlignedCircle{4}, vm::axis::z, "someName")
        | kdl::transform([&](const auto& cone) {
            CHECK(cone.fullySpecified());
            CHECK(cone.bounds() == bounds);

            // one face per side of the base circle, plus the base itself
            CHECK(cone.faceCount() == 5u);

            const auto expectedCone = makeBrush({
              {{-32, -32, -32}, {-32, 32, -32}, {0, 0, 32}},
              {{-32, 32, -32}, {32, 32, -32}, {0, 0, 32}},
              {{32, 32, -32}, {32, -32, -32}, {0, 0, 32}},
              {{32, -32, -32}, {-32, -32, -32}, {0, 0, 32}},
              {{-32, -32, -32}, {32, -32, -32}, {32, 32, -32}},
            });
            CHECK_THAT(cone, MatchesBrushVertices(expectedCone, vertexEpsilon));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("along each axis")
    {
      const auto axis = GENERATE(vm::axis::x, vm::axis::y, vm::axis::z);
      CAPTURE(axis);

      builder.createCone(bounds, VertexAlignedCircle{6}, axis, "someName")
        | kdl::transform([&](const auto& cone) {
            CHECK(cone.fullySpecified());
            CHECK(cone.faceCount() == 7u);
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }
  }

  SECTION("createUvSphere")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};

    const auto bounds = vm::bbox3d{{-32, -32, -32}, {32, 32, 32}};

    SECTION("aligned")
    {
      const auto circleShape =
        GENERATE(CircleShape{EdgeAlignedCircle{8}}, CircleShape{VertexAlignedCircle{8}});

      builder.createUvSphere(bounds, circleShape, 2, vm::axis::z, "someName")
        | kdl::transform([&](const auto& sphere) {
            CHECK(sphere.fullySpecified());
            CHECK(vm::approx{bounds, vertexEpsilon} == sphere.bounds());

            CHECK_THAT(
              sphere.faces() | std::views::transform([](const auto& face) {
                return face.materialName();
              }),
              RangeEquals(std::vector<std::string>{sphere.faceCount(), "someName"}));
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("scalable")
    {
      builder.createUvSphere(bounds, ScalableCircle{0}, 2, vm::axis::z, "someName")
        | kdl::transform([&](const auto& sphere) {
            CHECK(sphere.fullySpecified());
            CHECK(vm::approx{bounds, vertexEpsilon} == sphere.bounds());
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }
  }

  SECTION("createIcoSphere")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};

    const auto bounds = vm::bbox3d{{-32, -32, -32}, {32, 32, 32}};

    // 1 is the least number of iterations sphereMesh accepts; the base icosahedron
    // (0 iterations) is unreachable through this API, see BasicShapes.h
    SECTION("with one iteration")
    {
      builder.createIcoSphere(bounds, 1, "someName")
        | kdl::transform([&](const auto& sphere) {
            CHECK(sphere.fullySpecified());
            CHECK(vm::approx{bounds, vertexEpsilon} == sphere.bounds());

            // every one of the icosahedron's 20 triangles is split into four
            CHECK(sphere.faceCount() == 80u);
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }
  }

  SECTION("createBrush")
  {
    auto builder = BrushBuilder{MapFormat::Standard, worldBounds};

    SECTION("from points")
    {
      builder.createBrush(
        std::vector<vm::vec3d>{
          {-64, -64, -64},
          {-64, -64, +64},
          {-64, +64, -64},
          {-64, +64, +64},
          {+64, -64, -64},
          {+64, -64, +64},
          {+64, +64, -64},
          {+64, +64, +64},
        },
        "someName")
        | kdl::transform([](const auto& brush) {
            CHECK(brush.fullySpecified());
            CHECK(brush.bounds() == vm::bbox3d{-64.0, +64.0});
            CHECK(brush.faceCount() == 6u);
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("ignores points inside the hull")
    {
      builder.createBrush(
        std::vector<vm::vec3d>{
          {-64, -64, -64},
          {-64, -64, +64},
          {-64, +64, -64},
          {-64, +64, +64},
          {+64, -64, -64},
          {+64, -64, +64},
          {+64, +64, -64},
          {+64, +64, +64},
          {0, 0, 0},
        },
        "someName")
        | kdl::transform([](const auto& brush) {
            CHECK(brush.bounds() == vm::bbox3d{-64.0, +64.0});
            CHECK(brush.faceCount() == 6u);
          })
        | kdl::transform_error([](const auto& e) { FAIL(e); });
    }

    SECTION("fails for a degenerate point set")
    {
      const auto points = GENERATE(
        std::vector<vm::vec3d>{},
        std::vector<vm::vec3d>{{0, 0, 0}},
        std::vector<vm::vec3d>{{0, 0, 0}, {64, 0, 0}},
        std::vector<vm::vec3d>{{0, 0, 0}, {64, 0, 0}, {0, 64, 0}});

      CHECK(builder.createBrush(points, "someName").is_error());
    }

    SECTION("fails for an empty polyhedron")
    {
      CHECK(builder.createBrush(Polyhedron3{}, "someName").is_error());
    }
  }
}

} // namespace tb::mdl
