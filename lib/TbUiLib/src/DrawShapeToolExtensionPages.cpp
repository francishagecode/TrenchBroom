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

#include "ui/DrawShapeToolExtensionPages.h"

#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>

#include "mdl/Grid.h"
#include "mdl/Map.h"
#include "ui/BitmapButton.h"
#include "ui/DrawShapeToolParameters.h"
#include "ui/MapDocument.h"
#include "ui/ViewConstants.h"

#include <algorithm>
#include <array>

namespace tb::ui
{

namespace
{

using StairDirection = DrawShapeToolParameters::StairDirection;

size_t stairDirectionToIndex(const StairDirection direction)
{
  switch (direction)
  {
  case StairDirection::PosX:
    return 0u;
  case StairDirection::NegX:
    return 1u;
  case StairDirection::PosY:
    return 2u;
  case StairDirection::NegY:
    return 3u;
  }

  return 0u;
}

StairDirection indexToStairDirection(const size_t index)
{
  static constexpr auto directions = std::array{
    StairDirection::PosX,
    StairDirection::NegX,
    StairDirection::PosY,
    StairDirection::NegY};
  return directions[std::min(index, directions.size() - 1u)];
}

} // namespace

DrawShapeToolAxisAlignedShapeExtensionPage::DrawShapeToolAxisAlignedShapeExtensionPage(
  DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolExtensionPage{parent}
  , m_parameters{parameters}
{
  auto* axisLabel = new QLabel{tr("Axis: ")};
  auto* axisComboBox = new QComboBox{};
  axisComboBox->addItems({tr("X"), tr("Y"), tr("Z")});

  connect(
    axisComboBox,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    this,
    [&](const auto index) { m_parameters.setAxis(vm::axis::type(index)); });

  addWidget(axisLabel);
  addWidget(axisComboBox);

  m_notifierConnection += m_parameters.parametersDidChangeNotifier.connect(
    [=, this]() { axisComboBox->setCurrentIndex(int(m_parameters.axis())); });
}

DrawShapeToolCircularShapeExtensionPage::DrawShapeToolCircularShapeExtensionPage(
  DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolAxisAlignedShapeExtensionPage{parameters, parent}
  , m_parameters{parameters}
{
  auto* numSidesLabel = new QLabel{tr("Number of Sides: ")};
  auto* numSidesBox = new QSpinBox{};
  numSidesBox->setRange(3, 96);

  auto* precisionBox = new QComboBox{};
  precisionBox->addItems({"12", "24", "48", "96"});

  auto* numSidesWidget = new QStackedWidget{};
  numSidesWidget->addWidget(numSidesBox);
  numSidesWidget->addWidget(precisionBox);

  auto* edgeAlignedCircleButton =
    createBitmapToggleButton("CircleEdgeAligned.svg", tr("Align edge to bounding box"));
  edgeAlignedCircleButton->setIconSize({24, 24});
  edgeAlignedCircleButton->setObjectName("toolButton_withBorder");

  auto* vertexAlignedCircleButton = createBitmapToggleButton(
    "CircleVertexAligned.svg", tr("Align vertices to bounding box"));
  vertexAlignedCircleButton->setIconSize({24, 24});
  vertexAlignedCircleButton->setObjectName("toolButton_withBorder");

  auto* scalableCircleButton =
    createBitmapToggleButton("CircleScalable.svg", tr("Scalable circle shape"));
  scalableCircleButton->setIconSize({24, 24});
  scalableCircleButton->setObjectName("toolButton_withBorder");

  auto* radiusModeButtonGroup = new QButtonGroup{};
  radiusModeButtonGroup->addButton(edgeAlignedCircleButton);
  radiusModeButtonGroup->addButton(vertexAlignedCircleButton);
  radiusModeButtonGroup->addButton(scalableCircleButton);

  connect(
    numSidesBox,
    QOverload<int>::of(&QSpinBox::valueChanged),
    this,
    [&](const auto numSides) {
      m_parameters.setCircleShape(
        std::visit(
          kdl::overload(
            [&](const mdl::EdgeAlignedCircle&) -> mdl::CircleShape {
              return mdl::EdgeAlignedCircle{size_t(numSides)};
            },
            [&](const mdl::VertexAlignedCircle&) -> mdl::CircleShape {
              return mdl::VertexAlignedCircle{size_t(numSides)};
            },
            [&](const mdl::ScalableCircle& circleShape) -> mdl::CircleShape {
              return circleShape;
            }),
          m_parameters.circleShape()));
    });
  connect(
    precisionBox,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    this,
    [&](const auto precision) {
      m_parameters.setCircleShape(
        std::visit(
          kdl::overload(
            [&](const mdl::ScalableCircle&) -> mdl::CircleShape {
              return mdl::ScalableCircle{size_t(precision)};
            },
            [](const auto& circleShape) -> mdl::CircleShape { return circleShape; }),
          m_parameters.circleShape()));
    });
  connect(edgeAlignedCircleButton, &QToolButton::clicked, this, [=, this]() {
    m_parameters.setCircleShape(
      mdl::convertCircleShape<mdl::EdgeAlignedCircle>(m_parameters.circleShape()));
  });
  connect(vertexAlignedCircleButton, &QToolButton::clicked, this, [=, this]() {
    m_parameters.setCircleShape(
      mdl::convertCircleShape<mdl::VertexAlignedCircle>(m_parameters.circleShape()));
  });
  connect(scalableCircleButton, &QToolButton::clicked, this, [=, this]() {
    m_parameters.setCircleShape(
      mdl::convertCircleShape<mdl::ScalableCircle>(m_parameters.circleShape()));
  });

  addWidget(numSidesLabel);
  addWidget(numSidesWidget);
  addWidget(edgeAlignedCircleButton);
  addWidget(vertexAlignedCircleButton);
  addWidget(scalableCircleButton);

  m_notifierConnection += m_parameters.parametersDidChangeNotifier.connect([=, this]() {
    std::visit(
      kdl::overload(
        [&](const mdl::EdgeAlignedCircle& circleShape) {
          numSidesBox->setValue(int(circleShape.numSides));
          numSidesWidget->setCurrentWidget(numSidesBox);
        },
        [&](const mdl::VertexAlignedCircle& circleShape) {
          numSidesBox->setValue(int(circleShape.numSides));
          numSidesWidget->setCurrentWidget(numSidesBox);
        },
        [&](const mdl::ScalableCircle& circleShape) {
          precisionBox->setCurrentIndex(int(circleShape.precision));
          numSidesWidget->setCurrentWidget(precisionBox);
        }),
      m_parameters.circleShape());

    edgeAlignedCircleButton->setChecked(
      std::holds_alternative<mdl::EdgeAlignedCircle>(m_parameters.circleShape()));
    vertexAlignedCircleButton->setChecked(
      std::holds_alternative<mdl::VertexAlignedCircle>(m_parameters.circleShape()));
    scalableCircleButton->setChecked(
      std::holds_alternative<mdl::ScalableCircle>(m_parameters.circleShape()));
  });
}

DrawShapeToolCylinderShapeExtensionPage::DrawShapeToolCylinderShapeExtensionPage(
  MapDocument& document, DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolCircularShapeExtensionPage{parameters, parent}
  , m_parameters{parameters}
{
  auto* hollowCheckBox = new QCheckBox{tr("Hollow")};

  auto* thicknessLabel = new QLabel{tr("Thickness: ")};
  auto* thicknessBox = new QDoubleSpinBox{};
  thicknessBox->setEnabled(parameters.hollow());
  thicknessBox->setRange(1, 128);

  connect(hollowCheckBox, &QCheckBox::toggled, this, [&](const auto hollow) {
    m_parameters.setHollow(hollow);
  });
  connect(
    thicknessBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto thickness) { m_parameters.setThickness(thickness); });

  addWidget(hollowCheckBox);
  addWidget(thicknessLabel);
  addWidget(thicknessBox);
  addApplyButton(document);

  m_notifierConnection += m_parameters.parametersDidChangeNotifier.connect([=, this]() {
    hollowCheckBox->setChecked(m_parameters.hollow());
    thicknessBox->setEnabled(m_parameters.hollow());
    thicknessBox->setValue(m_parameters.thickness());
  });
}

DrawShapeToolConeShapeExtensionPage::DrawShapeToolConeShapeExtensionPage(
  MapDocument& document, DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolCircularShapeExtensionPage{parameters, parent}
  , m_parameters{parameters}
{
  addApplyButton(document);
}

DrawShapeToolTorusShapeExtensionPage::DrawShapeToolTorusShapeExtensionPage(
  MapDocument& document, DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolAxisAlignedShapeExtensionPage{parameters, parent}
  , m_parameters{parameters}
{
  auto* ringSegmentsLabel = new QLabel{tr("Ring Segments: ")};
  auto* ringSegmentsBox = new QSpinBox{};
  ringSegmentsBox->setRange(3, 64);
  ringSegmentsBox->setSingleStep(4);
  ringSegmentsBox->setToolTip(
    tr("Number of convex brushes arranged around the torus ring."));

  auto* tubeSegmentsLabel = new QLabel{tr("Tube Segments: ")};
  auto* tubeSegmentsBox = new QSpinBox{};
  tubeSegmentsBox->setRange(3, 32);
  tubeSegmentsBox->setSingleStep(4);
  tubeSegmentsBox->setToolTip(tr("Number of sides in the torus tube cross-section."));

  auto* holeSizeLabel = new QLabel{tr("Hole Size: ")};
  auto* holeSizeBox = new QDoubleSpinBox{};
  holeSizeBox->setRange(5.0, 95.0);
  holeSizeBox->setDecimals(0);
  holeSizeBox->setSingleStep(5.0);
  holeSizeBox->setSuffix(tr("%"));
  holeSizeBox->setToolTip(
    tr("Inner hole diameter as a percentage of the outer torus diameter."));

  auto* hollowCheckBox = new QCheckBox{tr("Hollow")};

  auto* thicknessLabel = new QLabel{tr("Thickness: ")};
  auto* thicknessBox = new QDoubleSpinBox{};
  thicknessBox->setEnabled(parameters.hollow());
  thicknessBox->setRange(1, 128);
  thicknessBox->setToolTip(tr("Wall thickness of the hollow torus tube."));

  connect(
    ringSegmentsBox,
    QOverload<int>::of(&QSpinBox::valueChanged),
    this,
    [&](const auto ringSegments) {
      auto shape = m_parameters.torusShape();
      shape.ringSegments = size_t(ringSegments);
      m_parameters.setTorusShape(shape);
    });
  connect(
    tubeSegmentsBox,
    QOverload<int>::of(&QSpinBox::valueChanged),
    this,
    [&](const auto tubeSegments) {
      auto shape = m_parameters.torusShape();
      shape.tubeSegments = size_t(tubeSegments);
      m_parameters.setTorusShape(shape);
    });
  connect(
    holeSizeBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto holeSizePercent) {
      auto shape = m_parameters.torusShape();
      shape.holeSize = holeSizePercent / 100.0;
      m_parameters.setTorusShape(shape);
    });
  connect(hollowCheckBox, &QCheckBox::toggled, this, [&](const auto hollow) {
    m_parameters.setHollow(hollow);
  });
  connect(
    thicknessBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto thickness) { m_parameters.setThickness(thickness); });

  addWidget(ringSegmentsLabel);
  addWidget(ringSegmentsBox);
  addWidget(tubeSegmentsLabel);
  addWidget(tubeSegmentsBox);
  addWidget(holeSizeLabel);
  addWidget(holeSizeBox);
  addWidget(hollowCheckBox);
  addWidget(thicknessLabel);
  addWidget(thicknessBox);
  addApplyButton(document);

  m_notifierConnection += m_parameters.parametersDidChangeNotifier.connect([=, this]() {
    const auto& shape = m_parameters.torusShape();
    ringSegmentsBox->setValue(int(shape.ringSegments));
    tubeSegmentsBox->setValue(int(shape.tubeSegments));
    holeSizeBox->setValue(shape.holeSize * 100.0);
    hollowCheckBox->setChecked(m_parameters.hollow());
    thicknessBox->setEnabled(m_parameters.hollow());
    thicknessBox->setValue(m_parameters.thickness());
  });
}

DrawShapeToolIcoSphereShapeExtensionPage::DrawShapeToolIcoSphereShapeExtensionPage(
  MapDocument& document, DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolExtensionPage{parent}
  , m_parameters{parameters}
{
  auto* accuracyLabel = new QLabel{tr("Accuracy: ")};
  auto* accuracyBox = new QSpinBox{};
  accuracyBox->setRange(1, 4);

  connect(
    accuracyBox,
    QOverload<int>::of(&QSpinBox::valueChanged),
    this,
    [&](const auto accuracy) { m_parameters.setAccuracy(size_t(accuracy)); });

  addWidget(accuracyLabel);
  addWidget(accuracyBox);
  addApplyButton(document);

  m_notifierConnection += m_parameters.parametersDidChangeNotifier.connect(
    [=, this]() { accuracyBox->setValue(int(m_parameters.accuracy())); });
}

DrawShapeToolUvSphereShapeExtensionPage::DrawShapeToolUvSphereShapeExtensionPage(
  MapDocument& document, DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolCircularShapeExtensionPage{parameters, parent}
  , m_parameters{parameters}
{
  auto* numRingsLabel = new QLabel{tr("Number of Rings: ")};
  auto* numRingsBox = new QSpinBox{};
  numRingsBox->setRange(1, 256);

  auto* numRingsLayout = new QHBoxLayout{};
  numRingsLayout->setContentsMargins(QMargins{});
  numRingsLayout->setSpacing(LayoutConstants::MediumHMargin);
  numRingsLayout->addWidget(numRingsLabel);
  numRingsLayout->addWidget(numRingsBox);

  auto* numRingsWidget = new QWidget{};
  numRingsWidget->setLayout(numRingsLayout);

  connect(
    numRingsBox,
    QOverload<int>::of(&QSpinBox::valueChanged),
    this,
    [&](const auto numRings) { m_parameters.setNumRings(size_t(numRings)); });

  addWidget(numRingsWidget);
  addApplyButton(document);

  m_notifierConnection += m_parameters.parametersDidChangeNotifier.connect([=, this]() {
    numRingsWidget->setVisible(
      !std::holds_alternative<mdl::ScalableCircle>(m_parameters.circleShape()));
    numRingsBox->setValue(int(m_parameters.numRings()));
  });
}

DrawShapeToolStairsExtensionPage::DrawShapeToolStairsExtensionPage(
  MapDocument& document, DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolExtensionPage{parent}
  , m_parameters{parameters}
{
  auto* stepHeightLabel = new QLabel{tr("Step Height: ")};
  auto* stepHeightBox = new QDoubleSpinBox{};
  stepHeightBox->setRange(1.0, 1024.0);
  stepHeightBox->setDecimals(0);

  auto* directionLabel = new QLabel{tr("Direction: ")};
  auto* directionComboBox = new QComboBox{};
  directionComboBox->addItems({tr("+X"), tr("-X"), tr("+Y"), tr("-Y")});

  connect(
    stepHeightBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto stepHeight) { m_parameters.setStepHeight(stepHeight); });

  connect(
    directionComboBox,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    this,
    [&](const auto index) {
      m_parameters.setStairDirection(indexToStairDirection(size_t(index)));
    });

  addWidget(stepHeightLabel);
  addWidget(stepHeightBox);
  addWidget(directionLabel);
  addWidget(directionComboBox);
  addApplyButton(document);

  m_notifierConnection += m_parameters.parametersDidChangeNotifier.connect([=, this]() {
    const auto direction = m_parameters.stairDirection();
    stepHeightBox->setValue(std::max(1.0, std::abs(m_parameters.stepHeight())));
    directionComboBox->setCurrentIndex(int(stairDirectionToIndex(direction)));
  });
}

DrawShapeToolArchShapeExtensionPage::DrawShapeToolArchShapeExtensionPage(
  MapDocument& document, DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolCircularShapeExtensionPage{parameters, parent}
  , m_parameters{parameters}
{
  auto* thicknessLabel = new QLabel{tr("Thickness: ")};
  auto* thicknessBox = new QDoubleSpinBox{};
  thicknessBox->setRange(1, 1024);

  connect(
    thicknessBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto thickness) { m_parameters.setThickness(thickness); });

  addWidget(thicknessLabel);
  addWidget(thicknessBox);
  addApplyButton(document);

  m_notifierConnection += m_parameters.parametersDidChangeNotifier.connect(
    [=, this]() { thicknessBox->setValue(m_parameters.thickness()); });
}

DrawShapeToolCorridorProfileExtensionPage::DrawShapeToolCorridorProfileExtensionPage(
  MapDocument& document,
  DrawShapeToolParameters& parameters,
  const bool horizontalOnly,
  QWidget* parent)
  : DrawShapeToolExtensionPage{parent}
  , m_parameters{parameters}
{
  if (horizontalOnly && m_parameters.corridorAxis() == vm::axis::z)
  {
    m_parameters.setCorridorAxis(vm::axis::x);
  }

  auto* axisBox = new QComboBox{};
  axisBox->addItems(
    horizontalOnly ? QStringList{tr("X"), tr("Y")}
                   : QStringList{tr("X"), tr("Y"), tr("Z")});

  const auto makeDimensionBox = []() {
    auto* box = new QDoubleSpinBox{};
    box->setRange(0.0, 65536.0);
    box->setDecimals(3);
    box->setAccelerated(true);
    box->setToolTip(tr("Arrow-key and button increments follow the active map grid."));
    return box;
  };

  auto* wallThicknessBox = makeDimensionBox();
  wallThicknessBox->setMinimum(0.001);
  auto* cornerRadiusBox = makeDimensionBox();
  cornerRadiusBox->setMinimum(0.001);
  auto* cornerSegmentsBox = new QSpinBox{};
  cornerSegmentsBox->setRange(1, 16);
  auto* ceilingRecessWidthBox = makeDimensionBox();
  auto* ceilingRecessDepthBox = makeDimensionBox();
  auto* sideRecessHeightBox = makeDimensionBox();
  auto* sideRecessDepthBox = makeDimensionBox();

  const auto dimensionBoxes = std::array{
    wallThicknessBox,
    cornerRadiusBox,
    ceilingRecessWidthBox,
    ceilingRecessDepthBox,
    sideRecessHeightBox,
    sideRecessDepthBox,
  };
  const auto updateGridIncrements = [=, &document]() {
    const auto increment = document.map().grid().actualSize();
    for (auto* box : dimensionBoxes)
    {
      box->setSingleStep(increment);
    }
  };
  updateGridIncrements();
  m_notifierConnection += document.gridDidChangeNotifier.connect(updateGridIncrements);

  auto* controlsLayout = new QGridLayout{};
  controlsLayout->setContentsMargins(QMargins{});
  controlsLayout->setHorizontalSpacing(LayoutConstants::MediumHMargin);
  controlsLayout->setVerticalSpacing(LayoutConstants::NarrowVMargin);

  controlsLayout->addWidget(new QLabel{tr("Axis:")}, 0, 0);
  controlsLayout->addWidget(axisBox, 0, 1);
  controlsLayout->addWidget(new QLabel{tr("Wall:")}, 0, 2);
  controlsLayout->addWidget(wallThicknessBox, 0, 3);
  controlsLayout->addWidget(new QLabel{tr("Radius:")}, 0, 4);
  controlsLayout->addWidget(cornerRadiusBox, 0, 5);
  controlsLayout->addWidget(new QLabel{tr("Corner steps:")}, 0, 6);
  controlsLayout->addWidget(cornerSegmentsBox, 0, 7);
  controlsLayout->addWidget(new QLabel{tr("Ceiling width/depth:")}, 1, 0, 1, 2);
  controlsLayout->addWidget(ceilingRecessWidthBox, 1, 2);
  controlsLayout->addWidget(ceilingRecessDepthBox, 1, 3);
  controlsLayout->addWidget(new QLabel{tr("Side height/depth:")}, 1, 4, 1, 2);
  controlsLayout->addWidget(sideRecessHeightBox, 1, 6);
  controlsLayout->addWidget(sideRecessDepthBox, 1, 7);

  auto* controlsWidget = new QWidget{};
  controlsWidget->setLayout(controlsLayout);

  connect(
    axisBox,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    this,
    [&](const auto index) { m_parameters.setCorridorAxis(vm::axis::type(index)); });
  connect(
    wallThicknessBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto wallThickness) {
      auto shape = m_parameters.corridorShape();
      shape.wallThickness = wallThickness;
      const auto maxRecessDepth = std::max(0.0, wallThickness - 0.001);
      shape.ceilingRecessDepth = std::min(shape.ceilingRecessDepth, maxRecessDepth);
      shape.sideRecessDepth = std::min(shape.sideRecessDepth, maxRecessDepth);
      m_parameters.setCorridorShape(shape);
    });
  connect(
    cornerRadiusBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto cornerRadius) {
      auto shape = m_parameters.corridorShape();
      shape.cornerRadius = cornerRadius;
      m_parameters.setCorridorShape(shape);
    });
  connect(
    cornerSegmentsBox,
    QOverload<int>::of(&QSpinBox::valueChanged),
    this,
    [&](const auto cornerSegments) {
      auto shape = m_parameters.corridorShape();
      shape.cornerSegments = size_t(cornerSegments);
      m_parameters.setCorridorShape(shape);
    });
  connect(
    ceilingRecessWidthBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto ceilingRecessWidth) {
      auto shape = m_parameters.corridorShape();
      shape.ceilingRecessWidth = ceilingRecessWidth;
      m_parameters.setCorridorShape(shape);
    });
  connect(
    ceilingRecessDepthBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto ceilingRecessDepth) {
      auto shape = m_parameters.corridorShape();
      shape.ceilingRecessDepth = ceilingRecessDepth;
      m_parameters.setCorridorShape(shape);
    });
  connect(
    sideRecessHeightBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto sideRecessHeight) {
      auto shape = m_parameters.corridorShape();
      shape.sideRecessHeight = sideRecessHeight;
      m_parameters.setCorridorShape(shape);
    });
  connect(
    sideRecessDepthBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto sideRecessDepth) {
      auto shape = m_parameters.corridorShape();
      shape.sideRecessDepth = sideRecessDepth;
      m_parameters.setCorridorShape(shape);
    });

  addWidget(controlsWidget);

  m_notifierConnection += m_parameters.parametersDidChangeNotifier.connect([=, this]() {
    const auto& shape = m_parameters.corridorShape();
    const auto maxRecessDepth = std::max(0.0, shape.wallThickness - 0.001);

    axisBox->setCurrentIndex(int(m_parameters.corridorAxis()));
    wallThicknessBox->setValue(shape.wallThickness);
    cornerRadiusBox->setValue(shape.cornerRadius);
    cornerSegmentsBox->setValue(int(shape.cornerSegments));
    ceilingRecessWidthBox->setValue(shape.ceilingRecessWidth);
    ceilingRecessDepthBox->setMaximum(maxRecessDepth);
    ceilingRecessDepthBox->setValue(shape.ceilingRecessDepth);
    sideRecessHeightBox->setValue(shape.sideRecessHeight);
    sideRecessDepthBox->setMaximum(maxRecessDepth);
    sideRecessDepthBox->setValue(shape.sideRecessDepth);
  });
}

DrawShapeToolCorridorShapeExtensionPage::DrawShapeToolCorridorShapeExtensionPage(
  MapDocument& document, DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolCorridorProfileExtensionPage{document, parameters, false, parent}
{
  addApplyButton(document);
}

DrawShapeToolCorridorBendExtensionPage::DrawShapeToolCorridorBendExtensionPage(
  MapDocument& document, DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolCorridorProfileExtensionPage{document, parameters, true, parent}
{
  auto* angleLabel = new QLabel{tr("Bend:")};
  auto* angleBox = new QComboBox{};
  angleBox->addItems({tr("45°"), tr("90°")});

  auto* directionLabel = new QLabel{tr("Turn:")};
  auto* directionBox = new QComboBox{};
  directionBox->addItems({tr("Left"), tr("Right")});

  auto* segmentsLabel = new QLabel{tr("Steps / 45°:")};
  auto* segmentsBox = new QSpinBox{};
  segmentsBox->setRange(1, 16);

  connect(
    angleBox,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    this,
    [&](const auto index) {
      m_parameters.setCorridorBendAngle(
        index == 0 ? mdl::CorridorBendAngle::Deg45 : mdl::CorridorBendAngle::Deg90);
    });
  connect(
    directionBox,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    this,
    [&](const auto index) {
      m_parameters.setCorridorBendDirection(
        index == 0 ? mdl::CorridorBendDirection::Left
                   : mdl::CorridorBendDirection::Right);
    });
  connect(
    segmentsBox,
    QOverload<int>::of(&QSpinBox::valueChanged),
    this,
    [&](const auto segments) { m_parameters.setCorridorBendSegments(size_t(segments)); });

  addWidget(angleLabel);
  addWidget(angleBox);
  addWidget(directionLabel);
  addWidget(directionBox);
  addWidget(segmentsLabel);
  addWidget(segmentsBox);
  addApplyButton(document);

  m_notifierConnection += m_parameters.parametersDidChangeNotifier.connect([=, this]() {
    angleBox->setCurrentIndex(
      m_parameters.corridorBendAngle() == mdl::CorridorBendAngle::Deg45 ? 0 : 1);
    directionBox->setCurrentIndex(
      m_parameters.corridorBendDirection() == mdl::CorridorBendDirection::Left ? 0 : 1);
    segmentsBox->setValue(int(m_parameters.corridorBendSegments()));
  });
}

DrawShapeToolCorridorTJunctionExtensionPage::DrawShapeToolCorridorTJunctionExtensionPage(
  MapDocument& document, DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolCorridorProfileExtensionPage{document, parameters, true, parent}
{
  auto* widthLabel = new QLabel{tr("Corridor width:")};
  auto* widthBox = new QDoubleSpinBox{};
  widthBox->setRange(0.001, 65536.0);
  widthBox->setDecimals(3);
  widthBox->setAccelerated(true);
  widthBox->setSingleStep(document.map().grid().actualSize());
  widthBox->setToolTip(tr("Arrow-key and button increments follow the active map grid."));
  widthBox->setValue(m_parameters.corridorJunctionWidth());

  connect(
    widthBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto width) { m_parameters.setCorridorJunctionWidth(width); });

  addWidget(widthLabel);
  addWidget(widthBox);
  addApplyButton(document);

  m_notifierConnection += m_parameters.parametersDidChangeNotifier.connect(
    [=, this]() { widthBox->setValue(m_parameters.corridorJunctionWidth()); });
  m_notifierConnection += document.gridDidChangeNotifier.connect(
    [=, &document]() { widthBox->setSingleStep(document.map().grid().actualSize()); });
}

DrawShapeToolChamberExtensionPage::DrawShapeToolChamberExtensionPage(
  MapDocument& document, DrawShapeToolParameters& parameters, QWidget* parent)
  : DrawShapeToolExtensionPage{parent}
  , m_parameters{parameters}
{
  auto* footprintBox = new QComboBox{};
  footprintBox->addItems(
    {tr("Chamfered"), tr("Octagonal"), tr("Capsule"), tr("Wedge"), tr("Apse")});

  auto* axisBox = new QComboBox{};
  axisBox->addItems({tr("X"), tr("Y")});

  const auto makeDimensionBox = []() {
    auto* box = new QDoubleSpinBox{};
    box->setRange(0.0, 65536.0);
    box->setDecimals(3);
    box->setAccelerated(true);
    box->setToolTip(tr("Arrow-key and button increments follow the active map grid."));
    return box;
  };

  auto* wallBox = makeDimensionBox();
  wallBox->setMinimum(0.001);
  auto* cornerBox = makeDimensionBox();
  auto* footprintSegmentsBox = new QSpinBox{};
  footprintSegmentsBox->setRange(1, 16);

  auto* ceilingBox = new QComboBox{};
  ceilingBox->addItems({tr("Flat"), tr("Barrel vault"), tr("Raised spine")});
  auto* ceilingRiseBox = makeDimensionBox();
  auto* ceilingSegmentsBox = new QSpinBox{};
  ceilingSegmentsBox->setRange(1, 16);

  auto* entranceBox = new QCheckBox{tr("Open entrance")};
  auto* entranceWidthBox = makeDimensionBox();
  entranceWidthBox->setMinimum(0.001);
  auto* entranceHeightBox = makeDimensionBox();
  entranceHeightBox->setMinimum(0.001);

  const auto dimensionBoxes =
    std::array{wallBox, cornerBox, ceilingRiseBox, entranceWidthBox, entranceHeightBox};
  const auto updateGridIncrements = [=, &document]() {
    const auto increment = document.map().grid().actualSize();
    for (auto* box : dimensionBoxes)
    {
      box->setSingleStep(increment);
    }
  };
  updateGridIncrements();
  m_notifierConnection += document.gridDidChangeNotifier.connect(updateGridIncrements);

  const auto& initialShape = m_parameters.chamberShape();
  footprintBox->setCurrentIndex(int(initialShape.footprint));
  axisBox->setCurrentIndex(int(m_parameters.chamberAxis()));
  wallBox->setValue(initialShape.wallThickness);
  cornerBox->setValue(initialShape.cornerSize);
  footprintSegmentsBox->setValue(int(initialShape.footprintSegments));
  ceilingBox->setCurrentIndex(int(initialShape.ceiling));
  ceilingRiseBox->setValue(initialShape.ceilingRise);
  ceilingSegmentsBox->setValue(int(initialShape.ceilingSegments));
  entranceBox->setChecked(initialShape.openEntrance);
  entranceWidthBox->setValue(initialShape.entranceWidth);
  entranceHeightBox->setValue(initialShape.entranceHeight);

  auto* controlsLayout = new QGridLayout{};
  controlsLayout->setContentsMargins(QMargins{});
  controlsLayout->setHorizontalSpacing(LayoutConstants::MediumHMargin);
  controlsLayout->setVerticalSpacing(LayoutConstants::NarrowVMargin);
  controlsLayout->addWidget(new QLabel{tr("Footprint:")}, 0, 0);
  controlsLayout->addWidget(footprintBox, 0, 1);
  controlsLayout->addWidget(new QLabel{tr("Axis:")}, 0, 2);
  controlsLayout->addWidget(axisBox, 0, 3);
  controlsLayout->addWidget(new QLabel{tr("Wall:")}, 0, 4);
  controlsLayout->addWidget(wallBox, 0, 5);
  controlsLayout->addWidget(new QLabel{tr("Corner:")}, 0, 6);
  controlsLayout->addWidget(cornerBox, 0, 7);
  controlsLayout->addWidget(new QLabel{tr("Curve steps:")}, 0, 8);
  controlsLayout->addWidget(footprintSegmentsBox, 0, 9);
  controlsLayout->addWidget(new QLabel{tr("Ceiling:")}, 1, 0);
  controlsLayout->addWidget(ceilingBox, 1, 1);
  controlsLayout->addWidget(new QLabel{tr("Rise:")}, 1, 2);
  controlsLayout->addWidget(ceilingRiseBox, 1, 3);
  controlsLayout->addWidget(new QLabel{tr("Steps / side:")}, 1, 4);
  controlsLayout->addWidget(ceilingSegmentsBox, 1, 5);
  controlsLayout->addWidget(entranceBox, 1, 6, 1, 2);
  controlsLayout->addWidget(new QLabel{tr("Opening W/H:")}, 1, 8);
  auto* entranceSizeLayout = new QHBoxLayout{};
  entranceSizeLayout->setContentsMargins(QMargins{});
  entranceSizeLayout->addWidget(entranceWidthBox);
  entranceSizeLayout->addWidget(entranceHeightBox);
  controlsLayout->addLayout(entranceSizeLayout, 1, 9);

  auto* controlsWidget = new QWidget{};
  controlsWidget->setLayout(controlsLayout);

  connect(
    footprintBox,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    this,
    [&](const auto index) {
      auto shape = m_parameters.chamberShape();
      shape.footprint = mdl::ChamberFootprint(index);
      m_parameters.setChamberShape(shape);
    });
  connect(
    axisBox,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    this,
    [&](const auto index) { m_parameters.setChamberAxis(vm::axis::type(index)); });
  connect(
    wallBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto value) {
      auto shape = m_parameters.chamberShape();
      shape.wallThickness = value;
      m_parameters.setChamberShape(shape);
    });
  connect(
    cornerBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto value) {
      auto shape = m_parameters.chamberShape();
      shape.cornerSize = value;
      m_parameters.setChamberShape(shape);
    });
  connect(
    footprintSegmentsBox,
    QOverload<int>::of(&QSpinBox::valueChanged),
    this,
    [&](const auto value) {
      auto shape = m_parameters.chamberShape();
      shape.footprintSegments = size_t(value);
      m_parameters.setChamberShape(shape);
    });
  connect(
    ceilingBox,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    this,
    [&](const auto index) {
      auto shape = m_parameters.chamberShape();
      shape.ceiling = mdl::ChamberCeiling(index);
      m_parameters.setChamberShape(shape);
    });
  connect(
    ceilingRiseBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto value) {
      auto shape = m_parameters.chamberShape();
      shape.ceilingRise = value;
      m_parameters.setChamberShape(shape);
    });
  connect(
    ceilingSegmentsBox,
    QOverload<int>::of(&QSpinBox::valueChanged),
    this,
    [&](const auto value) {
      auto shape = m_parameters.chamberShape();
      shape.ceilingSegments = size_t(value);
      m_parameters.setChamberShape(shape);
    });
  connect(entranceBox, &QCheckBox::toggled, this, [&](const auto checked) {
    auto shape = m_parameters.chamberShape();
    shape.openEntrance = checked;
    m_parameters.setChamberShape(shape);
  });
  connect(
    entranceWidthBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto value) {
      auto shape = m_parameters.chamberShape();
      shape.entranceWidth = value;
      m_parameters.setChamberShape(shape);
    });
  connect(
    entranceHeightBox,
    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
    this,
    [&](const auto value) {
      auto shape = m_parameters.chamberShape();
      shape.entranceHeight = value;
      m_parameters.setChamberShape(shape);
    });

  addWidget(controlsWidget);
  addApplyButton(document);

  m_notifierConnection += m_parameters.parametersDidChangeNotifier.connect([=, this]() {
    const auto& shape = m_parameters.chamberShape();
    footprintBox->setCurrentIndex(int(shape.footprint));
    axisBox->setCurrentIndex(int(m_parameters.chamberAxis()));
    wallBox->setValue(shape.wallThickness);
    cornerBox->setValue(shape.cornerSize);
    footprintSegmentsBox->setValue(int(shape.footprintSegments));
    ceilingBox->setCurrentIndex(int(shape.ceiling));
    ceilingRiseBox->setValue(shape.ceilingRise);
    ceilingSegmentsBox->setValue(int(shape.ceilingSegments));
    entranceBox->setChecked(shape.openEntrance);
    entranceWidthBox->setValue(shape.entranceWidth);
    entranceHeightBox->setValue(shape.entranceHeight);

    cornerBox->setEnabled(shape.footprint == mdl::ChamberFootprint::Chamfered);
    footprintSegmentsBox->setEnabled(
      shape.footprint == mdl::ChamberFootprint::Capsule
      || shape.footprint == mdl::ChamberFootprint::Apse);
    const auto shapedCeiling = shape.ceiling != mdl::ChamberCeiling::Flat;
    ceilingRiseBox->setEnabled(shapedCeiling);
    ceilingSegmentsBox->setEnabled(shapedCeiling);
    entranceWidthBox->setEnabled(shape.openEntrance);
    entranceHeightBox->setEnabled(shape.openEntrance);
  });
}

} // namespace tb::ui
