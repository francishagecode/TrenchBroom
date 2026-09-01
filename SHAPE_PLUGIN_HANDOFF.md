# User-Defined Shape Generators: Design and Session Handoff

Date: 2026-09-01

## Purpose

This document carries the design context for a future session. The long-term idea is to
let users install new procedural shapes by placing a definition package in a TrenchBroom
resource directory. The shape should then appear automatically in the Simple Shape Tool.

The important architectural boundary is:

- A Quake **brush** remains a normal convex polyhedron understood by TrenchBroom and the
  target map format.
- A **shape generator** creates one or more ordinary brushes from drag bounds and user
  parameters.
- Installing or removing a generator must not change the map format. Once created, its
  output remains normal editable map geometry and does not depend on the generator still
  being installed.

For this reason, the proposed feature should be called something like **user-defined
procedural shape generators**, rather than custom brush types or brush plugins.

## User Goal

TrenchBroom ships with a useful but necessarily limited selection of shapes. Users should
be able to share additional shapes without compiling a custom TrenchBroom fork. A future
workflow might look like this:

1. Download a shape package.
2. Place it in the user's TrenchBroom `shapes` directory.
3. Start TrenchBroom or reload the shape registry.
4. Select the new shape in the Simple Shape Tool.
5. Configure its automatically generated controls and draw it normally.

A package should probably be a directory rather than literally one file so that it can
carry an icon and, later, documentation or other assets:

```text
shapes/
  community.torus/
    shape.tbshape
    icon.svg
```

## Feasibility Finding

The feature is feasible, and the current code already contains several useful seams:

- `ui::DrawShapeToolExtension` is a polymorphic shape-generator interface. Its
  `createBrushes` method returns `Result<std::vector<mdl::Brush>>`.
- `ui::DrawShapeToolExtensionManager` already owns a collection of extensions and exposes
  the currently selected extension.
- `mdl::BrushBuilder::createBrush` accepts arbitrary points, constructs their convex hull,
  applies material and format defaults, and returns a validated brush.
- `ui::SystemPaths::findResourceDirectories` already finds bundled, portable, and
  user-data resource directories. The same mechanism could discover `shapes` directories.

The main limitation is not geometry construction. Registration, parameters, controls, and
algorithms are all compiled into C++:

- `DrawShapeToolExtensionKind` is a fixed enum.
- Extension construction switches over every enum value.
- `DrawShapeToolParameters` is one fixed C++ structure containing every built-in shape's
  settings.
- Each shape has a dedicated Qt extension page.
- Procedural algorithms live directly in `BrushBuilder.cpp`.

## Recommended Architecture

### 1. Runtime shape registry

Introduce a registry containing shape descriptors rather than using a fixed enum as the
source of truth. Each descriptor should contain at least:

```text
stable ID
display name
icon
parameter schema
generator implementation
definition format/API version
origin path (built-in or external)
```

Initially, existing C++ generators can be adapted into this registry. That provides a safe
refactoring step before loading external files.

### 2. Per-shape parameter values

Replace the global, shape-specific parameter structure with a parameter-value store keyed
by stable parameter IDs. Each shape descriptor declares its own schema.

Required parameter types will probably include:

- boolean;
- integer;
- decimal number;
- world-space distance;
- percentage;
- enum/choice;
- axis;
- possibly material or direction types later.

Definitions should provide defaults, ranges, steps, labels, tooltips, and optional
visibility/enabled conditions. Parameter values should be kept separately per shape so
switching shapes does not unexpectedly overwrite another shape's settings.

### 3. Schema-driven Qt controls

Add a generic extension page that builds Qt controls from the parameter schema. This
removes the requirement for one C++ page class per shape.

The existing shape pages should not all be rewritten at once. First prove that a generic
page can reproduce a small built-in shape's controls, then migrate incrementally.

### 4. Safe generator output boundary

An external generator should produce geometry descriptions, not construct arbitrary model
objects or mutate the document. A useful boundary is:

```text
drag bounds + parameter values
              |
              v
list of convex point sets
              |
              v
core BrushBuilder validation
              |
              v
ordinary mdl::Brush objects
```

Core TrenchBroom must remain responsible for:

- map-format compatibility;
- current material and default UV/surface attributes;
- convex-hull and brush validity;
- finite coordinates;
- world bounds;
- diagnostic errors.

### 5. Shape package discovery

Search resource directories named `shapes` through the existing `SystemPaths` machinery.
The loader needs deterministic precedence rules for duplicate stable IDs. User packages
should not silently replace built-ins unless an explicit override mechanism is designed.

Malformed packages should be disabled individually and reported with their filename and,
where possible, source location. One broken package must not prevent TrenchBroom from
starting or loading other shapes.

## Definition Language Options

### Static templates

A file could merely list normalized points and transforms. This is easy and sufficient for
wedges, diamonds, fixed prisms, and prefab-like shapes, but it cannot express variable
topology such as toruses, stairs, sweeps, or segmented arches.

### Native shared-library plugins

Loading `.dll`, `.so`, or `.dylib` generators would provide unlimited capability, but it
is not recommended as the first design. It introduces:

- an unstable C++/Qt ABI across compilers and TrenchBroom releases;
- separate binaries for every supported platform;
- arbitrary code execution inside the editor;
- crashes and memory corruption affecting the entire process;
- difficult distribution and review requirements.

If native plugins were ever supported, they would need a narrow versioned C ABI and a
clear trust model. They are unnecessary for the initial goal.

### Sandboxed procedural recipes (recommended)

Use a constrained, versioned geometry recipe rather than arbitrary native code. Useful
operations could include:

- `convexHull`;
- `cuboid` or `prism`;
- `transform`;
- `repeat`;
- `extrude`;
- `loft`;
- `sweep` along a line, arc, or ellipse;
- `shell` with inward thickness;
- simple conditionals and parameter expressions.

TrenchBroom's existing expression language can represent values, arrays, maps, arithmetic,
ranges, and conditions, but it currently has no general functions, loops, or trigonometric
calls. It is not sufficient by itself to express the torus algorithm. It could still be
reused for parameter expressions inside a higher-level recipe evaluator.

An embedded scripting runtime such as Lua or WebAssembly is another possibility, but it
would add a dependency, sandboxing work, resource accounting, and a larger security
surface. A geometric recipe language should be explored first.

## Illustrative Definition

The eventual syntax is deliberately undecided. A data-driven torus might conceptually look
like this:

```json
{
  "formatVersion": 1,
  "id": "community.torus",
  "name": "Torus",
  "icon": "icon.svg",
  "parameters": [
    { "id": "axis", "type": "axis", "default": "z" },
    { "id": "ringSegments", "type": "integer", "default": 16,
      "min": 3, "max": 64 },
    { "id": "tubeSegments", "type": "integer", "default": 8,
      "min": 3, "max": 32 },
    { "id": "holeSize", "type": "percentage", "default": 0.5,
      "min": 0.05, "max": 0.95 },
    { "id": "hollow", "type": "boolean", "default": false },
    { "id": "thickness", "type": "distance", "default": 16,
      "enabledWhen": "$hollow" }
  ],
  "generator": {
    "operation": "sweep",
    "path": { "type": "ellipse", "segments": "$ringSegments" },
    "profile": { "type": "ellipse", "segments": "$tubeSegments" },
    "shell": { "enabled": "$hollow", "thickness": "$thickness" }
  }
}
```

This is an example of the desired responsibilities, not an agreed file format.

## Validation and Security Requirements

Every external definition must be treated as untrusted input. At minimum, enforce:

- finite numeric values and coordinates;
- declared parameter ranges;
- maximum generated brushes, faces, and vertices;
- maximum loop/repeat counts;
- world-bound checks;
- no unrestricted filesystem, process, or network access;
- package-relative asset paths that cannot escape the package directory;
- a versioned definition format;
- deterministic evaluation;
- cancellation or execution limits if the evaluator can perform substantial work;
- clear diagnostics without crashing the editor.

## Suggested Incremental Implementation

Each stage should compile, pass its focused tests, and leave the editor working, in line
with `CONTRIBUTING.md` and `AGENTS.md`.

1. **Shape descriptor and registry**
   - Register existing native shapes through descriptors.
   - Preserve current ordering and behavior.
   - Remove the fixed enum only when the registry fully replaces it.

2. **Generic parameter schema and value store**
   - Support the parameter types required by one simple existing shape.
   - Keep native generators while changing how their values are supplied.

3. **Generic Qt parameter page**
   - Generate controls from schemas.
   - Test defaults, notifications, constraints, enabled conditions, and Apply behavior.

4. **External package parser and discovery**
   - Add versioned descriptors, icon resolution, stable IDs, precedence rules, and
     diagnostics.
   - Initially allow a very small static convex-hull recipe.

5. **Procedural recipe evaluator**
   - Add a few composable geometric operations with strict limits.
   - Avoid trying to anticipate every possible shape.

6. **Proof shape**
   - Express one genuinely procedural shape externally. The torus is a strong test because
     it requires two independent segment counts, axis handling, bounds fitting, and an
     optional shell.

7. **Migration and documentation**
   - Migrate built-ins only where doing so improves maintainability.
   - Document package installation, authoring, validation errors, and compatibility.

## Upstream Strategy

This is an architectural feature and should begin with an upstream design issue, not a
large unsolicited pull request. `CONTRIBUTING.md` explicitly asks contributors to align
with the maintainer before implementing code.

A suitable issue title is:

> User-defined procedural shape generators for the Simple Shape Tool

The issue should focus on the user problem and the staged registry/schema approach. Avoid
committing prematurely to JSON, Lua, WebAssembly, or a particular DSL. The first question
for the maintainer is whether runtime-discovered, data-defined creation tools fit the
project's desired scope.

If the direction is accepted, propose the registry refactor as the first small PR. Do not
combine the complete file format, evaluator, UI rewrite, and built-in migration into one
change.

## Current Working Tree: Torus Feature

The current uncommitted working tree already contains a separate Torus Simple Shape Tool
feature. It should be preserved while beginning any plugin work.

Implemented torus behavior:

- solid torus formed from one convex brush per ring segment;
- hollow torus formed from a closed grid of convex wall cells;
- ring segments, tube segments, hole-size percentage, axis, Hollow, and Thickness controls;
- inward wall thickness so outer drag bounds remain exact;
- safe thickness fitting for small drag previews;
- arbitrary supported axes and non-quarter-aligned segment counts;
- `ShapeTool_Torus.svg` icon.

Relevant modified files include:

```text
lib/TbMdlLib/include/mdl/BrushBuilder.h
lib/TbMdlLib/src/BrushBuilder.cpp
lib/TbMdlLib/test/src/tst_BrushBuilder.cpp
lib/TbAppLib/include/ui/DrawShapeToolExtensionKind.h
lib/TbAppLib/include/ui/DrawShapeToolExtensions.h
lib/TbAppLib/include/ui/DrawShapeToolParameters.h
lib/TbAppLib/src/DrawShapeToolExtensions.cpp
lib/TbAppLib/src/DrawShapeToolParameters.cpp
lib/TbAppLib/test/src/tst_DrawShapeToolExtensions.cpp
lib/TbUiLib/include/ui/DrawShapeToolExtensionPages.h
lib/TbUiLib/src/DrawShapeToolExtensionPages.cpp
app/TrenchBroom/resources/graphics/images/ShapeTool_Torus.svg
```

Validation already completed successfully:

```text
cmake --build build --target TbMdlLibTest TbAppLibTest TbUiLibTest TrenchBroom -j 2
ctest --test-dir build/lib/TbMdlLib/test -R '^BrushBuilder$' -j --output-on-failure
ctest --test-dir build/lib/TbAppLib/test \
  -R '^(DrawShapeToolTorusExtension|DrawShapeToolParameters)$' \
  -j --output-on-failure
ctest --test-dir build/lib/TbUiLib/test -j --output-on-failure
```

Results at the end of the session:

- focused model tests passed;
- focused application tests passed;
- all 36 UI tests passed;
- the TrenchBroom application built successfully;
- `git diff --check` was clean;
- the torus SVG was copied into application and UI-test build resources;
- no commit was created.

## Recommended Start for the Next Session

1. Read `AGENTS.md`, `CONTRIBUTING.md`, and this document.
2. Run `git status --short` and preserve the uncommitted torus changes.
3. Decide whether the immediate deliverable is:
   - an upstream design issue draft; or
   - a fork-only registry prototype used to test the architecture.
4. If prototyping, begin only with `ShapeDescriptor` plus `ShapeRegistry`, adapting the
   existing native shapes without changing their geometry or UI behavior.
5. Do not choose a scripting language or finalize `.tbshape` syntax until the registry,
   parameter model, and threat boundary have been agreed.

