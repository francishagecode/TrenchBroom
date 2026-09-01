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

#include "ui/DrawShapeToolPage.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QStackedLayout>
#include <QToolButton>

#include "ui/BitmapButton.h"
#include "ui/DrawShapeToolExtensionManager.h"
#include "ui/DrawShapeToolExtensionPageRegistry.h"
#include "ui/ImageUtils.h"
#include "ui/ViewConstants.h"

namespace tb::ui
{

DrawShapeToolPage::DrawShapeToolPage(
  DrawShapeToolExtensionManager& extensionManager,
  const DrawShapeToolExtensionPageRegistry& extensionPageRegistry,
  QWidget* parent)
  : QWidget{parent}
  , m_extensionManager{extensionManager}
  , m_extensionPageRegistry{extensionPageRegistry}
{
  createGui();
  m_notifierConnection += m_extensionManager.currentExtensionDidChangeNotifier.connect(
    this, &DrawShapeToolPage::currentExtensionDidChange);
}

void DrawShapeToolPage::createGui()
{
  auto* label = new QLabel{tr("Shape")};
  const auto& currentExtensionInfo = m_extensionManager.currentExtensionInfo();
  m_extensionButton =
    createBitmapButton(currentExtensionInfo.iconPath, tr("Click to select a shape"));
  m_extensionButton->setObjectName("toolButton_withBorder");

  auto* extensionMenu = new QMenu{m_extensionButton};
  const auto& extensionInfos = m_extensionManager.extensionInfos();
  for (const auto& extensionInfo : extensionInfos)
  {
    auto icon = loadSVGIcon(extensionInfo.iconPath);

    auto* action = extensionMenu->addAction(
      icon,
      QString::fromStdString(extensionInfo.name),
      this,
      [this, id = extensionInfo.id]() { m_extensionManager.setCurrentExtension(id); });
    action->setIconVisibleInMenu(true);
  }
  m_extensionButton->setMenu(extensionMenu);
  m_extensionButton->setPopupMode(QToolButton::InstantPopup);

  m_extensionPages = new QStackedLayout{};
  for (auto i = size_t{0}; i < extensionInfos.size(); ++i)
  {
    auto extensionPage = m_extensionPageRegistry.create(
      extensionInfos[i].id,
      m_extensionManager.document(),
      m_extensionManager.parameters(i),
      this);
    m_notifierConnection +=
      extensionPage->applyParametersNotifier.connect(applyParametersNotifier);
    m_extensionPages->addWidget(extensionPage.release());
    m_extensionManager.parameters(i).parametersDidChangeNotifier();
  }

  auto* layout = new QHBoxLayout();
  layout->setContentsMargins(QMargins{});
  layout->setSpacing(LayoutConstants::MediumHMargin);

  layout->addWidget(label, 0, Qt::AlignVCenter);
  layout->addWidget(m_extensionButton, 0, Qt::AlignVCenter);
  layout->addLayout(m_extensionPages);
  layout->addStretch(2);

  setLayout(layout);
}

void DrawShapeToolPage::currentExtensionDidChange(const size_t index)
{
  auto icon = loadSVGIcon(m_extensionManager.currentExtensionInfo().iconPath);
  m_extensionButton->setIcon(icon);
  m_extensionPages->setCurrentIndex(int(index));
}

} // namespace tb::ui
