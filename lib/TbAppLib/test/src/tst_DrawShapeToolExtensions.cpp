/*
 Copyright (C) 2025 Kristian Duske

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

#include "Observer.h"
#include "mdl/BrushBuilder.h"
#include "mdl/CatchConfig.h"
#include "mdl/CircleShape.h"
#include "ui/DrawShapeToolExtensions.h"
#include "ui/DrawShapeToolParameters.h"
#include "ui/MapDocumentFixture.h"

#include "kd/range_fold.h"

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{

TEST_CASE("DrawShapeToolCuboidExtension")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto extension = DrawShapeToolCuboidExtension{document};
  auto parameters = DrawShapeToolParameters{};

  SECTION("createBrushes")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -64}, {64, 64, 64}};
    const auto result = extension.createBrushes(bounds, parameters);
    REQUIRE(result.is_success());
    REQUIRE(result.value().size() == 1);
    const auto& brush = result.value()[0];
    REQUIRE(brush.faceCount() == 6); // Cuboid has 6 faces
  }
}

TEST_CASE("DrawShapeToolCylinderExtension")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto extension = DrawShapeToolCylinderExtension{document};
  auto parameters = DrawShapeToolParameters{};

  SECTION("Create solid cylinder with default circle shape")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -128}, {64, 64, 128}};
    parameters.setCircleShape(mdl::EdgeAlignedCircle{8});
    const auto result = extension.createBrushes(bounds, parameters);
    REQUIRE(result.is_success());
    REQUIRE(result.value().size() == 1);
    const auto& brush = result.value()[0];
    // Cylinder with 8-sided circle has 8 + 2 faces (8 sides + top + bottom)
    REQUIRE(brush.faceCount() == 10);
  }

  SECTION("Create solid cylinder with different face counts")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -128}, {64, 64, 128}};

    SECTION("6-sided circle")
    {
      parameters.setCircleShape(mdl::EdgeAlignedCircle{6});
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
      REQUIRE(result.value()[0].faceCount() == 8); // 6 sides + 2
    }

    SECTION("12-sided circle")
    {
      parameters.setCircleShape(mdl::EdgeAlignedCircle{12});
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
      REQUIRE(result.value()[0].faceCount() == 14); // 12 sides + 2
    }
  }

  SECTION("Create hollow cylinder")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -128}, {64, 64, 128}};
    parameters.setCircleShape(mdl::EdgeAlignedCircle{8});
    parameters.setHollow(true);
    parameters.setThickness(16.0);
    const auto result = extension.createBrushes(bounds, parameters);
    REQUIRE(result.is_success());
    // Hollow cylinder creates multiple brushes (one per side)
    REQUIRE(result.value().size() > 1);
  }

  SECTION("Create hollow cylinder with different thicknesses")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -128}, {64, 64, 128}};
    parameters.setCircleShape(mdl::EdgeAlignedCircle{8});
    parameters.setHollow(true);

    SECTION("Thin wall")
    {
      parameters.setThickness(4.0);
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }

    SECTION("Thick wall")
    {
      parameters.setThickness(32.0);
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }
  }

  SECTION("Create cylinder with different axes")
  {
    SECTION("Z axis")
    {
      parameters.setAxis(vm::axis::z);
      const auto bounds = vm::bbox3d{{-64, -64, -128}, {64, 64, 128}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }

    SECTION("X axis")
    {
      parameters.setAxis(vm::axis::x);
      const auto bounds = vm::bbox3d{{-128, -64, -64}, {128, 64, 64}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }

    SECTION("Y axis")
    {
      parameters.setAxis(vm::axis::y);
      const auto bounds = vm::bbox3d{{-64, -128, -64}, {64, 128, 64}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }
  }
}

TEST_CASE("DrawShapeToolConeExtension")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto extension = DrawShapeToolConeExtension{document};
  auto parameters = DrawShapeToolParameters{};

  SECTION("Create solid cone with default circle shape")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -128}, {64, 64, 128}};
    parameters.setCircleShape(mdl::EdgeAlignedCircle{8});
    const auto result = extension.createBrushes(bounds, parameters);
    REQUIRE(result.is_success());
    REQUIRE(result.value().size() == 1);
    const auto& brush = result.value()[0];
    // Cone with 8-sided base has 8 + 1 faces (8 sides + bottom)
    REQUIRE(brush.faceCount() == 9);
  }

  SECTION("Create cone with different face counts")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -128}, {64, 64, 128}};

    SECTION("4-sided cone")
    {
      parameters.setCircleShape(mdl::EdgeAlignedCircle{4});
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
      REQUIRE(result.value()[0].faceCount() == 5); // 4 sides + 1
    }

    SECTION("16-sided cone")
    {
      parameters.setCircleShape(mdl::EdgeAlignedCircle{16});
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
      REQUIRE(result.value()[0].faceCount() == 17); // 16 sides + 1
    }
  }

  SECTION("Create cone with different axes")
  {
    parameters.setCircleShape(mdl::EdgeAlignedCircle{8});

    SECTION("Z axis")
    {
      parameters.setAxis(vm::axis::z);
      const auto bounds = vm::bbox3d{{-64, -64, -128}, {64, 64, 128}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }

    SECTION("X axis")
    {
      parameters.setAxis(vm::axis::x);
      const auto bounds = vm::bbox3d{{-128, -64, -64}, {128, 64, 64}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }
  }

  SECTION("Create hollow cone")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -128}, {64, 64, 128}};
    parameters.setCircleShape(mdl::EdgeAlignedCircle{8});
    parameters.setHollow(true);
    parameters.setThickness(16.0);
    const auto result = extension.createBrushes(bounds, parameters);
    REQUIRE(result.is_success());
    REQUIRE(result.value().size() == 1);
  }
}

TEST_CASE("DrawShapeToolIcoSphereExtension")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto extension = DrawShapeToolIcoSphereExtension{document};
  auto parameters = DrawShapeToolParameters{};

  SECTION("Create icosphere with default accuracy")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -64}, {64, 64, 64}};
    parameters.setAccuracy(1);
    const auto result = extension.createBrushes(bounds, parameters);
    REQUIRE(result.is_success());
    REQUIRE(result.value().size() == 1);
    REQUIRE(result.value()[0].faceCount() > 0);
  }

  SECTION("Create hollow icosphere")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -64}, {64, 64, 64}};
    parameters.setAccuracy(1);
    parameters.setHollow(true);
    parameters.setThickness(16.0);
    const auto result = extension.createBrushes(bounds, parameters);
    REQUIRE(result.is_success());
    REQUIRE(result.value().size() == 1);
  }
}

TEST_CASE("DrawShapeToolUvSphereExtension")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto extension = DrawShapeToolUvSphereExtension{document};
  auto parameters = DrawShapeToolParameters{};

  SECTION("Create uvsphere with default ring count")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -64}, {64, 64, 64}};
    parameters.setNumRings(8);
    parameters.setCircleShape(mdl::EdgeAlignedCircle{8});
    const auto result = extension.createBrushes(bounds, parameters);
    REQUIRE(result.is_success());
    REQUIRE(result.value().size() == 1);
  }

  SECTION("Create uvsphere with different ring counts")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -64}, {64, 64, 64}};
    parameters.setCircleShape(mdl::EdgeAlignedCircle{8});

    SECTION("4 rings")
    {
      parameters.setNumRings(4);
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }

    SECTION("8 rings")
    {
      parameters.setNumRings(8);
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }
  }

  SECTION("Create uvsphere with different circle shapes")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -64}, {64, 64, 64}};
    parameters.setNumRings(8);

    SECTION("6-sided circle")
    {
      parameters.setCircleShape(mdl::EdgeAlignedCircle{6});
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }

    SECTION("8-sided circle")
    {
      parameters.setCircleShape(mdl::EdgeAlignedCircle{8});
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }
  }

  SECTION("Create hollow uvsphere")
  {
    const auto bounds = vm::bbox3d{{-64, -64, -64}, {64, 64, 64}};
    parameters.setNumRings(8);
    parameters.setCircleShape(mdl::EdgeAlignedCircle{8});
    parameters.setHollow(true);
    parameters.setThickness(16.0);
    const auto result = extension.createBrushes(bounds, parameters);
    REQUIRE(result.is_success());
    REQUIRE(result.value().size() == 1);
  }

  SECTION("Create uvsphere with different axes")
  {
    parameters.setNumRings(8);
    parameters.setCircleShape(mdl::EdgeAlignedCircle{8});

    SECTION("Z axis")
    {
      parameters.setAxis(vm::axis::z);
      const auto bounds = vm::bbox3d{{-64, -64, -64}, {64, 64, 64}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }

    SECTION("X axis")
    {
      parameters.setAxis(vm::axis::x);
      const auto bounds = vm::bbox3d{{-64, -64, -64}, {64, 64, 64}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }
  }
}

TEST_CASE("DrawShapeToolStairsExtension")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto extension = DrawShapeToolStairsExtension{document};
  auto parameters = DrawShapeToolParameters{};

  SECTION("Create stairs with default step height")
  {
    const auto bounds = vm::bbox3d{{-64, -64, 0}, {64, 64, 128}};
    parameters.setStepHeight(16.0);
    const auto result = extension.createBrushes(bounds, parameters);
    REQUIRE(result.is_success());
    // Stairs create multiple brushes (one per step)
    REQUIRE(result.value().size() > 1);
  }

  SECTION("Create stairs with different step heights")
  {
    const auto bounds = vm::bbox3d{{-64, -64, 0}, {64, 64, 128}};

    SECTION("8 unit steps")
    {
      parameters.setStepHeight(8.0);
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
      const auto numSteps = result.value().size();
      REQUIRE(numSteps > 0);
    }

    SECTION("32 unit steps")
    {
      parameters.setStepHeight(32.0);
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
      const auto numSteps = result.value().size();
      REQUIRE(numSteps > 0);
    }
  }

  SECTION("Create stairs with different bounds")
  {
    parameters.setStepHeight(16.0);

    SECTION("Tall stairs")
    {
      const auto bounds = vm::bbox3d{{-64, -64, 0}, {64, 64, 256}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
      // More height means more steps
      const auto numSteps = result.value().size();
      REQUIRE(numSteps > 0);
    }

    SECTION("Short stairs")
    {
      const auto bounds = vm::bbox3d{{-64, -64, 0}, {64, 64, 32}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
      const auto numSteps = result.value().size();
      REQUIRE(numSteps > 0);
    }
  }

  SECTION("Create stairs with different axes")
  {
    parameters.setStepHeight(16.0);

    SECTION("+X direction")
    {
      parameters.setStairDirection(DrawShapeToolParameters::StairDirection::PosX);
      const auto bounds = vm::bbox3d{{-64, -64, 0}, {64, 64, 128}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }

    SECTION("-X direction")
    {
      parameters.setStairDirection(DrawShapeToolParameters::StairDirection::NegX);
      const auto bounds = vm::bbox3d{{0, -64, -64}, {128, 64, 64}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }

    SECTION("+Y direction")
    {
      parameters.setStairDirection(DrawShapeToolParameters::StairDirection::PosY);
      const auto bounds = vm::bbox3d{{-64, 0, -64}, {64, 128, 64}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }

    SECTION("-Y direction")
    {
      parameters.setStairDirection(DrawShapeToolParameters::StairDirection::NegY);
      const auto bounds = vm::bbox3d{{-64, 0, -64}, {64, 128, 64}};
      const auto result = extension.createBrushes(bounds, parameters);
      REQUIRE(result.is_success());
    }
  }
}

TEST_CASE("DrawShapeToolArchExtension")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto extension = DrawShapeToolArchExtension{document};
  auto parameters = DrawShapeToolParameters{};

  SECTION("createBrushes wires parameters to the brush builder")
  {
    const auto bounds = vm::bbox3d{{-128, -64, 0}, {128, 64, 64}};
    parameters.setAxis(vm::axis::y);
    parameters.setCircleShape(mdl::EdgeAlignedCircle{8});
    parameters.setThickness(16.0);

    extension.createBrushes(bounds, parameters)
      | kdl::transform([&](const auto& brushes) {
          CHECK(brushes.size() == 5u);

          CHECK(
            kdl::fold_left_first(
              brushes
                | std::views::transform([](const auto& brush) { return brush.bounds(); }),
              [](const auto& lhs, const auto& rhs) { return vm::merge(lhs, rhs); })
            == bounds);
        })
      | kdl::transform_error([](const auto& e) { FAIL(e); });
  }
}

TEST_CASE("DrawShapeToolCorridorExtension")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto extension = DrawShapeToolCorridorExtension{document};
  auto parameters = DrawShapeToolParameters{};

  SECTION("createBrushes wires corridor-specific parameters to the brush builder")
  {
    const auto bounds = vm::bbox3d{{-128, -128, 0}, {128, 128, 160}};

    extension.createBrushes(bounds, parameters)
      | kdl::transform([&](const auto& brushes) {
          CHECK(brushes.size() == 24u);
          CHECK(
            kdl::fold_left_first(
              brushes
                | std::views::transform([](const auto& brush) { return brush.bounds(); }),
              [](const auto& lhs, const auto& rhs) { return vm::merge(lhs, rhs); })
            == bounds);
        })
      | kdl::transform_error([](const auto& e) { FAIL(e); });
  }

  CHECK(extension.name() == "Corridor");
  CHECK(extension.iconPath() == "ShapeTool_Corridor.svg");
}

TEST_CASE("DrawShapeToolCorridorBendExtension")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto extension = DrawShapeToolCorridorBendExtension{document};
  auto parameters = DrawShapeToolParameters{};
  const auto bounds = vm::bbox3d{{-256, -128, 0}, {256, 128, 160}};

  SECTION("45 degree bend")
  {
    extension.createBrushes(bounds, parameters)
      | kdl::transform([&](const auto& brushes) { CHECK(brushes.size() == 72u); })
      | kdl::transform_error([](const auto& e) { FAIL(e); });
  }

  SECTION("90 degree bend")
  {
    parameters.setCorridorBendAngle(mdl::CorridorBendAngle::Deg90);
    parameters.setCorridorBendDirection(mdl::CorridorBendDirection::Right);
    extension.createBrushes(bounds, parameters)
      | kdl::transform([&](const auto& brushes) { CHECK(brushes.size() == 144u); })
      | kdl::transform_error([](const auto& e) { FAIL(e); });
  }

  CHECK(extension.name() == "Corridor Bend");
  CHECK(extension.iconPath() == "ShapeTool_CorridorBend.svg");
}

TEST_CASE("DrawShapeToolCorridorTJunctionExtension")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto extension = DrawShapeToolCorridorTJunctionExtension{document};
  auto parameters = DrawShapeToolParameters{};
  const auto bounds = vm::bbox3d{{-256, -384, 0}, {256, 384, 160}};

  extension.createBrushes(bounds, parameters) | kdl::transform([&](const auto& brushes) {
    CHECK(brushes.size() == 87u);
    CHECK(
      kdl::fold_left_first(
        brushes | std::views::transform([](const auto& brush) { return brush.bounds(); }),
        [](const auto& lhs, const auto& rhs) { return vm::merge(lhs, rhs); })
      == bounds);
  }) | kdl::transform_error([](const auto& e) { FAIL(e); });

  CHECK(extension.name() == "Corridor T");
  CHECK(extension.iconPath() == "ShapeTool_CorridorT.svg");
}

TEST_CASE("DrawShapeToolChamberExtension")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  auto extension = DrawShapeToolChamberExtension{document};
  auto parameters = DrawShapeToolParameters{};
  const auto bounds = vm::bbox3d{{-384, -256, 0}, {384, 256, 256}};

  SECTION("Default chamfered chamber")
  {
    extension.createBrushes(bounds, parameters)
      | kdl::transform([&](const auto& brushes) {
          CHECK(brushes.size() == 12u);
          CHECK(
            kdl::fold_left_first(
              brushes
                | std::views::transform([](const auto& brush) { return brush.bounds(); }),
              [](const auto& lhs, const auto& rhs) { return vm::merge(lhs, rhs); })
            == bounds);
        })
      | kdl::transform_error([](const auto& e) { FAIL(e); });
  }

  SECTION("Apse with barrel vault")
  {
    auto shape = parameters.chamberShape();
    shape.footprint = mdl::ChamberFootprint::Apse;
    shape.ceiling = mdl::ChamberCeiling::BarrelVault;
    parameters.setChamberShape(shape);

    extension.createBrushes(bounds, parameters)
      | kdl::transform([&](const auto& brushes) {
          CHECK(brushes.size() == 20u);
          CHECK(
            kdl::fold_left_first(
              brushes
                | std::views::transform([](const auto& brush) { return brush.bounds(); }),
              [](const auto& lhs, const auto& rhs) { return vm::merge(lhs, rhs); })
            == bounds);
        })
      | kdl::transform_error([](const auto& e) { FAIL(e); });
  }

  CHECK(extension.name() == "Chamber");
  CHECK(extension.iconPath() == "ShapeTool_Chamber.svg");
}

TEST_CASE("DrawShapeToolParameters")
{
  auto parameters = DrawShapeToolParameters{};

  SECTION("Default values")
  {
    REQUIRE(parameters.axis() == vm::axis::z);
    REQUIRE(parameters.hollow() == false);
    REQUIRE(parameters.thickness() == 16.0);
    REQUIRE(parameters.numRings() == 8);
    REQUIRE(parameters.accuracy() == 1);
    REQUIRE(parameters.stepHeight() == 16.0);
    REQUIRE(parameters.stairDirection() == DrawShapeToolParameters::StairDirection::PosX);
    REQUIRE(parameters.corridorAxis() == vm::axis::x);
    REQUIRE(
      parameters.corridorShape()
      == mdl::CorridorShape{
        .wallThickness = 16.0,
        .cornerRadius = 32.0,
        .cornerSegments = 2u,
        .ceilingRecessWidth = 64.0,
        .ceilingRecessDepth = 8.0,
        .sideRecessHeight = 48.0,
        .sideRecessDepth = 8.0,
      });
    REQUIRE(parameters.corridorBendAngle() == mdl::CorridorBendAngle::Deg45);
    REQUIRE(parameters.corridorBendDirection() == mdl::CorridorBendDirection::Left);
    REQUIRE(parameters.corridorBendSegments() == 3u);
    REQUIRE(parameters.corridorJunctionWidth() == 256.0);
    REQUIRE(parameters.chamberAxis() == vm::axis::x);
    REQUIRE(
      parameters.chamberShape()
      == mdl::ChamberShape{
        .footprint = mdl::ChamberFootprint::Chamfered,
        .ceiling = mdl::ChamberCeiling::Flat,
        .wallThickness = 16.0,
        .cornerSize = 64.0,
        .footprintSegments = 3u,
        .ceilingRise = 64.0,
        .ceilingSegments = 4u,
        .openEntrance = true,
        .entranceWidth = 224.0,
        .entranceHeight = 128.0,
      });
  }

  SECTION("Axis modifications")
  {
    auto parametersDidChange = Observer<>{parameters.parametersDidChangeNotifier};

    parameters.setAxis(vm::axis::z);
    CHECK(parametersDidChange.notifications.empty());

    parameters.setAxis(vm::axis::x);
    REQUIRE(parameters.axis() == vm::axis::x);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setAxis(vm::axis::x);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setAxis(vm::axis::y);
    REQUIRE(parameters.axis() == vm::axis::y);
    CHECK(parametersDidChange.notifications.size() == 2u);
  }

  SECTION("Hollow modifications")
  {
    auto parametersDidChange = Observer<>{parameters.parametersDidChangeNotifier};

    REQUIRE(parameters.hollow() == false);

    parameters.setHollow(false);
    CHECK(parametersDidChange.notifications.empty());

    parameters.setHollow(true);
    REQUIRE(parameters.hollow() == true);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setHollow(true);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setHollow(false);
    REQUIRE(parameters.hollow() == false);
    CHECK(parametersDidChange.notifications.size() == 2u);
  }

  SECTION("Thickness modifications")
  {
    auto parametersDidChange = Observer<>{parameters.parametersDidChangeNotifier};

    parameters.setThickness(16.0);
    CHECK(parametersDidChange.notifications.empty());

    parameters.setThickness(32.0);
    REQUIRE(parameters.thickness() == 32.0);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setThickness(32.0);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setThickness(8.0);
    REQUIRE(parameters.thickness() == 8.0);
    CHECK(parametersDidChange.notifications.size() == 2u);
  }

  SECTION("NumRings modifications")
  {
    auto parametersDidChange = Observer<>{parameters.parametersDidChangeNotifier};

    parameters.setNumRings(8);
    CHECK(parametersDidChange.notifications.empty());

    parameters.setNumRings(16);
    REQUIRE(parameters.numRings() == 16);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setNumRings(16);
    CHECK(parametersDidChange.notifications.size() == 1u);
  }

  SECTION("Accuracy modifications")
  {
    auto parametersDidChange = Observer<>{parameters.parametersDidChangeNotifier};

    parameters.setAccuracy(1);
    CHECK(parametersDidChange.notifications.empty());

    parameters.setAccuracy(2);
    REQUIRE(parameters.accuracy() == 2);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setAccuracy(2);
    CHECK(parametersDidChange.notifications.size() == 1u);
  }

  SECTION("StepHeight modifications")
  {
    auto parametersDidChange = Observer<>{parameters.parametersDidChangeNotifier};

    parameters.setStepHeight(16.0);
    CHECK(parametersDidChange.notifications.empty());

    parameters.setStepHeight(8.0);
    REQUIRE(parameters.stepHeight() == 8.0);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setStepHeight(8.0);
    CHECK(parametersDidChange.notifications.size() == 1u);
  }

  SECTION("StairDirection modifications")
  {
    auto parametersDidChange = Observer<>{parameters.parametersDidChangeNotifier};

    parameters.setStairDirection(DrawShapeToolParameters::StairDirection::PosX);
    CHECK(parametersDidChange.notifications.empty());

    parameters.setStairDirection(DrawShapeToolParameters::StairDirection::NegX);
    REQUIRE(parameters.stairDirection() == DrawShapeToolParameters::StairDirection::NegX);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setStairDirection(DrawShapeToolParameters::StairDirection::NegX);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setStairDirection(DrawShapeToolParameters::StairDirection::PosY);
    REQUIRE(parameters.stairDirection() == DrawShapeToolParameters::StairDirection::PosY);
    CHECK(parametersDidChange.notifications.size() == 2u);
  }

  SECTION("Corridor axis modifications")
  {
    auto parametersDidChange = Observer<>{parameters.parametersDidChangeNotifier};

    parameters.setCorridorAxis(vm::axis::x);
    CHECK(parametersDidChange.notifications.empty());

    parameters.setCorridorAxis(vm::axis::y);
    REQUIRE(parameters.corridorAxis() == vm::axis::y);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setCorridorAxis(vm::axis::y);
    CHECK(parametersDidChange.notifications.size() == 1u);
  }

  SECTION("Corridor shape modifications")
  {
    auto parametersDidChange = Observer<>{parameters.parametersDidChangeNotifier};

    const auto defaultShape = parameters.corridorShape();
    parameters.setCorridorShape(defaultShape);
    CHECK(parametersDidChange.notifications.empty());

    auto changedShape = defaultShape;
    changedShape.cornerRadius = 48.0;
    changedShape.cornerSegments = 3u;
    parameters.setCorridorShape(changedShape);
    REQUIRE(parameters.corridorShape() == changedShape);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setCorridorShape(changedShape);
    CHECK(parametersDidChange.notifications.size() == 1u);
  }

  SECTION("Corridor connector modifications")
  {
    auto parametersDidChange = Observer<>{parameters.parametersDidChangeNotifier};

    parameters.setCorridorBendAngle(mdl::CorridorBendAngle::Deg45);
    parameters.setCorridorBendDirection(mdl::CorridorBendDirection::Left);
    parameters.setCorridorBendSegments(3u);
    parameters.setCorridorJunctionWidth(256.0);
    CHECK(parametersDidChange.notifications.empty());

    parameters.setCorridorBendAngle(mdl::CorridorBendAngle::Deg90);
    REQUIRE(parameters.corridorBendAngle() == mdl::CorridorBendAngle::Deg90);
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setCorridorBendDirection(mdl::CorridorBendDirection::Right);
    REQUIRE(parameters.corridorBendDirection() == mdl::CorridorBendDirection::Right);
    CHECK(parametersDidChange.notifications.size() == 2u);

    parameters.setCorridorBendSegments(5u);
    REQUIRE(parameters.corridorBendSegments() == 5u);
    CHECK(parametersDidChange.notifications.size() == 3u);

    parameters.setCorridorJunctionWidth(320.0);
    REQUIRE(parameters.corridorJunctionWidth() == 320.0);
    CHECK(parametersDidChange.notifications.size() == 4u);

    parameters.setCorridorJunctionWidth(1.0);
    REQUIRE(parameters.corridorJunctionWidth() == 33.0);
    CHECK(parametersDidChange.notifications.size() == 5u);

    auto thickShape = parameters.corridorShape();
    thickShape.wallThickness = 32.0;
    parameters.setCorridorShape(thickShape);
    REQUIRE(parameters.corridorJunctionWidth() == 65.0);
    CHECK(parametersDidChange.notifications.size() == 6u);
  }

  SECTION("Chamber modifications")
  {
    auto parametersDidChange = Observer<>{parameters.parametersDidChangeNotifier};

    parameters.setChamberAxis(vm::axis::x);
    parameters.setChamberShape(parameters.chamberShape());
    CHECK(parametersDidChange.notifications.empty());

    parameters.setChamberAxis(vm::axis::y);
    REQUIRE(parameters.chamberAxis() == vm::axis::y);
    CHECK(parametersDidChange.notifications.size() == 1u);

    auto shape = parameters.chamberShape();
    shape.footprint = mdl::ChamberFootprint::Apse;
    shape.ceiling = mdl::ChamberCeiling::BarrelVault;
    shape.ceilingRise = 96.0;
    parameters.setChamberShape(shape);
    REQUIRE(parameters.chamberShape() == shape);
    CHECK(parametersDidChange.notifications.size() == 2u);

    parameters.setChamberShape(shape);
    CHECK(parametersDidChange.notifications.size() == 2u);
  }

  SECTION("CircleShape modifications")
  {
    auto parametersDidChange = Observer<>{parameters.parametersDidChangeNotifier};

    const auto defaultShape = parameters.circleShape();
    parameters.setCircleShape(defaultShape);
    CHECK(parametersDidChange.notifications.empty());

    parameters.setCircleShape(mdl::EdgeAlignedCircle{12});
    const auto& shape = parameters.circleShape();
    // Verify the circle shape was set (exact comparison depends on CircleShape
    // implementation)
    REQUIRE(std::holds_alternative<mdl::EdgeAlignedCircle>(shape));
    CHECK(parametersDidChange.notifications.size() == 1u);

    parameters.setCircleShape(mdl::EdgeAlignedCircle{12});
    CHECK(parametersDidChange.notifications.size() == 1u);
  }
}

} // namespace tb::ui
