#pragma once

// DirectionWidget: Filament filagui ImGuiExtensions (Apache-2.0), adapted for MiniEngine math types.
// See ImGuiDirectionWidget.cpp for license / attribution.

namespace viewer_imgui
{
	/** DragFloat3 + 2D orientation widget (arrow on quaternion trackball). v is a unit-ish direction; normalized on edit. */
	bool DirectionWidget(const char* label, float v[3]);
}
