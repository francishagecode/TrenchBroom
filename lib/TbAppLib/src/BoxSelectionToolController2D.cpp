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

#include "ui/BoxSelectionToolController2D.h"

#include "gl/Camera.h"
#include "render/RenderBatch.h"
#include "render/RenderContext.h"
#include "ui/BoxSelectionTool.h"
#include "ui/GestureTracker.h"
#include "ui/InputState.h"
#include "ui/Lasso.h"

#include "vm/bbox.h"
#include "vm/vec.h"

namespace tb::ui
{
namespace
{

BoxSelectionMode selectionMode(const InputState& inputState)
{
  if (inputState.modifierKeysDown(ModifierKeys::Alt))
  {
    return BoxSelectionMode::Subtract;
  }
  if (inputState.modifierKeysDown(ModifierKeys::CtrlCmd))
  {
    return BoxSelectionMode::Add;
  }
  return BoxSelectionMode::Replace;
}

class BoxSelectionDragTracker : public GestureTracker
{
private:
  static constexpr auto LassoDistance = 64.0;

  BoxSelectionTool& m_tool;
  const gl::Camera& m_camera;
  const vm::vec3d m_startPoint;
  vm::vec3d m_currentPoint;
  const float m_startMouseX;
  float m_currentMouseX;
  Lasso m_lasso;

public:
  BoxSelectionDragTracker(BoxSelectionTool& tool, const InputState& inputState)
    : m_tool{tool}
    , m_camera{inputState.camera()}
    , m_startPoint{inputState.defaultPointUnderMouse()}
    , m_currentPoint{m_startPoint}
    , m_startMouseX{inputState.mouseX()}
    , m_currentMouseX{m_startMouseX}
    , m_lasso{m_camera, LassoDistance, m_startPoint}
  {
  }

  bool update(const InputState& inputState) override
  {
    m_currentPoint = inputState.defaultPointUnderMouse();
    m_currentMouseX = inputState.mouseX();
    m_lasso.update(m_currentPoint);
    m_tool.refreshViews();
    return true;
  }

  void end(const InputState& inputState) override
  {
    update(inputState);
    m_tool.select(selectionBounds(), boundsMode(), selectionMode(inputState));
  }

  void cancel() override { m_tool.refreshViews(); }

  void modifierKeyChange(const InputState&) override { m_tool.refreshViews(); }

  void render(
    const InputState&,
    render::RenderContext& renderContext,
    render::RenderBatch& renderBatch) const override
  {
    if (boundsMode() == BoxSelectionBoundsMode::Contain)
    {
      m_lasso.render(
        renderContext,
        renderBatch,
        RgbaF{0.2f, 0.55f, 1.0f, 1.0f},
        RgbaF{0.2f, 0.55f, 1.0f, 0.2f});
    }
    else
    {
      m_lasso.render(
        renderContext,
        renderBatch,
        RgbaF{0.2f, 0.9f, 0.4f, 1.0f},
        RgbaF{0.2f, 0.9f, 0.4f, 0.2f});
    }
  }

private:
  BoxSelectionBoundsMode boundsMode() const
  {
    return m_currentMouseX < m_startMouseX ? BoxSelectionBoundsMode::Intersect
                                           : BoxSelectionBoundsMode::Contain;
  }

  vm::bbox3d selectionBounds() const
  {
    auto min = vm::min(m_startPoint, m_currentPoint);
    auto max = vm::max(m_startPoint, m_currentPoint);

    const auto worldBounds = m_tool.worldBounds().expand(-1.0);
    const auto viewAxis = vm::find_abs_max_component(m_camera.direction());
    min[viewAxis] = worldBounds.min[viewAxis];
    max[viewAxis] = worldBounds.max[viewAxis];

    return vm::intersect(vm::bbox3d{min, max}, worldBounds);
  }
};

} // namespace

BoxSelectionToolController2D::BoxSelectionToolController2D(BoxSelectionTool& tool)
  : m_tool{tool}
{
}

Tool& BoxSelectionToolController2D::tool()
{
  return m_tool;
}

const Tool& BoxSelectionToolController2D::tool() const
{
  return m_tool;
}

std::unique_ptr<GestureTracker> BoxSelectionToolController2D::acceptMouseDrag(
  const InputState& inputState)
{
  if (
    !inputState.mouseButtonsPressed(MouseButtons::Left)
    || !inputState.camera().orthographicProjection())
  {
    return nullptr;
  }

  return std::make_unique<BoxSelectionDragTracker>(m_tool, inputState);
}

} // namespace tb::ui
