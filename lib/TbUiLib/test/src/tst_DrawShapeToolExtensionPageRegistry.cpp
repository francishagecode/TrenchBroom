/*
 Copyright (C) 2026 Kristian Duske

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

#include <QWidget>

#include "ui/DrawShapeToolExtensionPageRegistry.h"
#include "ui/DrawShapeToolExtensionRegistry.h"
#include "ui/DrawShapeToolParameters.h"
#include "ui/MapDocumentFixture.h"

#include <catch2/catch_test_macros.hpp>

namespace tb::ui
{

TEST_CASE("DrawShapeToolExtensionPageRegistry")
{
  auto fixture = MapDocumentFixture{};
  auto& document = fixture.create();
  const auto extensionRegistry = createDrawShapeToolExtensionRegistry();
  const auto pageRegistry = createDrawShapeToolExtensionPageRegistry();

  SECTION("provides a native parameter page for every registered extension")
  {
    REQUIRE(pageRegistry.descriptors().size() == extensionRegistry.descriptors().size());

    auto parent = QWidget{};
    for (const auto& extensionDescriptor : extensionRegistry.descriptors())
    {
      CAPTURE(extensionDescriptor.id);
      REQUIRE(pageRegistry.find(extensionDescriptor.id) != nullptr);

      auto parameters = DrawShapeToolParameters{};
      auto page =
        pageRegistry.create(extensionDescriptor.id, document, parameters, &parent);
      REQUIRE(page != nullptr);
      parameters.parametersDidChangeNotifier();
    }
  }
}

} // namespace tb::ui
