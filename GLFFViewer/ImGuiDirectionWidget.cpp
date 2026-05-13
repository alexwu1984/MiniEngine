/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * -----------------------------------------------------------------------------
 * Adapted from Filament libs/filagui/src/ImGuiExtensions.cpp (DirectionWidget /
 * ArrowWidget) for MiniEngine: math::Vector3 / math::Quaternion, local ImGui path.
 */

#include "ImGuiDirectionWidget.h"

#include "Imgui/imgui.h"
#include "Imgui/imgui_internal.h"

#include "math/quaternion.h"
#include "math/vector3.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace viewer_imgui
{
	namespace
	{
		static constexpr float kPi = 3.14159265358979323846f;

		static math::Quaternion QuatHamilton(const math::Quaternion& q, const math::Quaternion& p)
		{
			const math::Vector3 qv(q.x, q.y, q.z);
			const math::Vector3 pv(p.x, p.y, p.z);
			const math::Vector3 v = math::Vector3::Cross(qv, pv) + pv * q.w + qv * p.w;
			const float s = q.w * p.w - qv.Dot(pv);
			math::Quaternion r(v.x, v.y, v.z, s);
			r.Normalize();
			return r;
		}

		static math::Vector3 RotateVectorByUnitQuat(const math::Quaternion& q, const math::Vector3& v)
		{
			const math::Vector3 u(q.x, q.y, q.z);
			const math::Vector3 t = math::Vector3::Cross(u, v) * 2.0f;
			return v + t * q.w + math::Vector3::Cross(u, t);
		}

		static float ClampF(float x, float lo, float hi)
		{
			return (std::max)(lo, (std::min)(hi, x));
		}

		class ArrowWidget
		{
		public:
			explicit ArrowWidget(const math::Vector3& direction);
			bool draw();
			math::Vector3 getDirection() const;

		private:
			math::Quaternion mDirectionQuat{};
			enum EArrowParts
			{
				ARROW_CONE,
				ARROW_CONE_CAP,
				ARROW_CYL,
				ARROW_CYL_CAP
			};
			static void createArrow();
			void drawTriangles(ImDrawList* draw_list, const ImVec2& offset, const ImVector<ImVec2>& triProj, const ImVector<ImU32>& colLight,
							   int numVertices) const;
			static float quatD(float w, float h) { return std::min(std::abs(w), std::abs(h)) - 4.0f; }
			static float quatPX(float x, float w, float h) { return (x * 0.5f * quatD(w, h) + w * 0.5f + 0.5f); }
			static float quatPY(float y, float w, float h) { return (-y * 0.5f * quatD(w, h) + h * 0.5f - 0.5f); }
			static float quatIX(int x, float w, float h) { return (2.0f * x - w - 1.0f) / quatD(w, h); }
			static float quatIY(int y, float w, float h) { return (-2.0f * y + h - 1.0f) / quatD(w, h); }
			static void quatFromDirection(math::Quaternion& quat, const math::Vector3& dir);
			static ImU32 blendColor(ImU32 c1, ImU32 c2, float t);
			static const ImU32 DirColor = 0xffff0000;
			static const int WidgetSize = 100;
		};

		static ImVector<math::Vector3> s_ArrowTri[4];
		static ImVector<ImVec2> s_ArrowTriProj[4];
		static ImVector<math::Vector3> s_ArrowNorm[4];
		static ImVector<ImU32> s_ArrowColLight[4];

		inline float ImVec2Cross(const ImVec2& left, const ImVec2& right)
		{
			return (left.x * right.y) - (left.y * right.x);
		}

		inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
		{
			return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
		}

		inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs)
		{
			return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
		}

		ArrowWidget::ArrowWidget(const math::Vector3& direction)
		{
			quatFromDirection(mDirectionQuat, direction);
		}

		math::Vector3 ArrowWidget::getDirection() const
		{
			math::Vector3 d = RotateVectorByUnitQuat(mDirectionQuat, math::Vector3(1.f, 0.f, 0.f));
			return d.Normalize();
		}

		bool ArrowWidget::draw()
		{
			ImGuiStyle& style = ImGui::GetStyle();
			ImDrawList* draw_list = ImGui::GetWindowDrawList();

			if (s_ArrowTri[0].empty())
				ArrowWidget::createArrow();

			bool value_changed = false;

			const ImVec2 orient_pos = ImGui::GetCursorScreenPos();

			const float sv_orient_size = std::min(ImGui::CalcItemWidth(), float(WidgetSize));
			const float w = sv_orient_size;
			const float h = sv_orient_size;

			static math::Quaternion origQuat;
			static math::Vector3 coordOld;
			bool highlighted = false;
			ImGui::InvisibleButton("widget", ImVec2(sv_orient_size, sv_orient_size));
			if (ImGui::IsItemActive())
			{
				highlighted = true;
				const ImVec2 mouse = ImGui::GetMousePos() - orient_pos;
				if (ImGui::IsMouseClicked(0))
				{
					origQuat = mDirectionQuat;
					coordOld = math::Vector3(quatIX((int)mouse.x, w, h), quatIY((int)mouse.y, w, h), 1.0f);
				}
				else if (ImGui::IsMouseDragging(0))
				{
					const math::Vector3 coord(quatIX((int)mouse.x, w, h), quatIY((int)mouse.y, w, h), 1.0f);
					math::Vector3 pVec = coord;
					math::Vector3 oVec = coordOld;
					pVec.z = 0.0f;
					const float n0 = oVec.GetLength();
					const float n1 = pVec.GetLength();
					if (n0 > FLT_EPSILON && n1 > FLT_EPSILON)
					{
						math::Vector3 v0 = oVec * (1.0f / n0);
						math::Vector3 v1 = pVec * (1.0f / n1);
						math::Vector3 axis = math::Vector3::Cross(v0, v1);
						const float sa = axis.GetLength();
						const float ca = v0.Dot(v1);
						float angle = std::atan2(sa, ca);
						if (coord.x * coord.x + coord.y * coord.y > 1.0f)
							angle *= 1.0f + 1.5f * (coord.GetLength() - 1.0f);
						axis = axis.Normalize();
						const math::Quaternion qrot(axis, angle);
						const float nqorig = origQuat.Length();
						if (std::abs(nqorig) > FLT_EPSILON * FLT_EPSILON)
						{
							const math::Quaternion qorig = math::Quaternion(origQuat.x / nqorig, origQuat.y / nqorig, origQuat.z / nqorig, origQuat.w / nqorig);
							mDirectionQuat = QuatHamilton(qrot, qorig);
						}
						else
							mDirectionQuat = qrot;
						value_changed = true;
					}
				}
				draw_list->AddRectFilled(orient_pos, orient_pos + ImVec2(sv_orient_size, sv_orient_size),
										 ImColor(style.Colors[ImGuiCol_FrameBgActive]), style.FrameRounding);
			}
			else
			{
				const ImColor color(ImGui::IsItemHovered() ? style.Colors[ImGuiCol_FrameBgHovered] : style.Colors[ImGuiCol_FrameBg]);
				draw_list->AddRectFilled(orient_pos, orient_pos + ImVec2(sv_orient_size, sv_orient_size), color, style.FrameRounding);
			}

			math::Quaternion quat = math::Quaternion::Normalize(mDirectionQuat);
			const ImColor alpha(1.0f, 1.0f, 1.0f, highlighted ? 1.0f : 0.75f);
			const math::Vector3 arrowDir = RotateVectorByUnitQuat(quat, math::Vector3(1.f, 0.f, 0.f));

			for (int k = 0; k < 4; ++k)
			{
				const int j = (arrowDir.z > 0) ? 3 - k : k;
				const size_t ntri = (size_t)s_ArrowTri[j].size();
				for (size_t i = 0; i < ntri; ++i)
				{
					math::Vector3 coord = s_ArrowTri[j][(int)i];
					math::Vector3 norm = s_ArrowNorm[j][(int)i];
					if (coord.x > 0)
						coord.x = 2.5f * coord.x - 2.0f;
					else
						coord.x += 0.2f;
					coord.y *= 1.5f;
					coord.z *= 1.5f;
					coord = RotateVectorByUnitQuat(quat, coord);
					norm = RotateVectorByUnitQuat(quat, norm);
					s_ArrowTriProj[j][(int)i] = ImVec2(quatPX(coord.x, w, h), quatPY(coord.y, w, h));
					const ImU32 col = (DirColor | 0xff000000) & alpha;
					s_ArrowColLight[j][(int)i] = blendColor(0xff000000, col, std::abs(ClampF(norm.z, -1.0f, 1.0f)));
				}
				drawTriangles(draw_list, orient_pos, s_ArrowTriProj[j], s_ArrowColLight[j], (int)ntri);
			}

			return value_changed;
		}

		void ArrowWidget::drawTriangles(ImDrawList* draw_list, const ImVec2& offset, const ImVector<ImVec2>& triProj, const ImVector<ImU32>& colLight,
										int numVertices) const
		{
			const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
			IM_ASSERT(numVertices % 3 == 0);
			draw_list->PrimReserve(numVertices, numVertices);
			for (int ii = 0; ii < numVertices / 3; ii++)
			{
				ImVec2 v1 = offset + triProj[ii * 3];
				ImVec2 v2 = offset + triProj[ii * 3 + 1];
				ImVec2 v3 = offset + triProj[ii * 3 + 2];

				const ImVec2 d1 = v2 - v1;
				const ImVec2 d2 = v3 - v1;
				const float c = ImVec2Cross(d1, d2);
				if (c > 0.0f)
				{
					v2 = v1;
					v3 = v1;
				}

				draw_list->PrimWriteIdx((ImDrawIdx)draw_list->_VtxCurrentIdx);
				draw_list->PrimWriteIdx((ImDrawIdx)(draw_list->_VtxCurrentIdx + 1));
				draw_list->PrimWriteIdx((ImDrawIdx)(draw_list->_VtxCurrentIdx + 2));
				draw_list->PrimWriteVtx(v1, uv, colLight[ii * 3]);
				draw_list->PrimWriteVtx(v2, uv, colLight[ii * 3 + 1]);
				draw_list->PrimWriteVtx(v3, uv, colLight[ii * 3 + 2]);
			}
		}

		void ArrowWidget::createArrow()
		{
			const int SUBDIV = 15;
			const float CYL_RADIUS = 0.08f;
			const float CONE_RADIUS = 0.16f;
			const float CONE_LENGTH = 0.25f;
			const float ARROW_BGN = -1.1f;
			const float ARROW_END = 1.15f;

			for (int i = 0; i < 4; ++i)
			{
				s_ArrowTri[i].clear();
				s_ArrowNorm[i].clear();
			}

			float x0, x1, y0, y1, z0, z1, a0, a1, nx, nn;
			for (int i = 0; i < SUBDIV; ++i)
			{
				a0 = 2.0f * kPi * (float(i)) / SUBDIV;
				a1 = 2.0f * kPi * (float(i + 1)) / SUBDIV;
				x0 = ARROW_BGN;
				x1 = ARROW_END - CONE_LENGTH;
				y0 = std::cos(a0);
				z0 = std::sin(a0);
				y1 = std::cos(a1);
				z1 = std::sin(a1);
				s_ArrowTri[ARROW_CYL].push_back(math::Vector3(x1, CYL_RADIUS * y0, CYL_RADIUS * z0));
				s_ArrowTri[ARROW_CYL].push_back(math::Vector3(x0, CYL_RADIUS * y0, CYL_RADIUS * z0));
				s_ArrowTri[ARROW_CYL].push_back(math::Vector3(x0, CYL_RADIUS * y1, CYL_RADIUS * z1));
				s_ArrowTri[ARROW_CYL].push_back(math::Vector3(x1, CYL_RADIUS * y0, CYL_RADIUS * z0));
				s_ArrowTri[ARROW_CYL].push_back(math::Vector3(x0, CYL_RADIUS * y1, CYL_RADIUS * z1));
				s_ArrowTri[ARROW_CYL].push_back(math::Vector3(x1, CYL_RADIUS * y1, CYL_RADIUS * z1));
				s_ArrowNorm[ARROW_CYL].push_back(math::Vector3(0, y0, z0));
				s_ArrowNorm[ARROW_CYL].push_back(math::Vector3(0, y0, z0));
				s_ArrowNorm[ARROW_CYL].push_back(math::Vector3(0, y1, z1));
				s_ArrowNorm[ARROW_CYL].push_back(math::Vector3(0, y0, z0));
				s_ArrowNorm[ARROW_CYL].push_back(math::Vector3(0, y1, z1));
				s_ArrowNorm[ARROW_CYL].push_back(math::Vector3(0, y1, z1));
				s_ArrowTri[ARROW_CYL_CAP].push_back(math::Vector3(x0, 0, 0));
				s_ArrowTri[ARROW_CYL_CAP].push_back(math::Vector3(x0, CYL_RADIUS * y1, CYL_RADIUS * z1));
				s_ArrowTri[ARROW_CYL_CAP].push_back(math::Vector3(x0, CYL_RADIUS * y0, CYL_RADIUS * z0));
				s_ArrowNorm[ARROW_CYL_CAP].push_back(math::Vector3(-1, 0, 0));
				s_ArrowNorm[ARROW_CYL_CAP].push_back(math::Vector3(-1, 0, 0));
				s_ArrowNorm[ARROW_CYL_CAP].push_back(math::Vector3(-1, 0, 0));
				x0 = ARROW_END - CONE_LENGTH;
				x1 = ARROW_END;
				nx = CONE_RADIUS / (x1 - x0);
				nn = 1.0f / std::sqrtf(nx * nx + 1);
				s_ArrowTri[ARROW_CONE].push_back(math::Vector3(x1, 0, 0));
				s_ArrowTri[ARROW_CONE].push_back(math::Vector3(x0, CONE_RADIUS * y0, CONE_RADIUS * z0));
				s_ArrowTri[ARROW_CONE].push_back(math::Vector3(x0, CONE_RADIUS * y1, CONE_RADIUS * z1));
				s_ArrowTri[ARROW_CONE].push_back(math::Vector3(x1, 0, 0));
				s_ArrowTri[ARROW_CONE].push_back(math::Vector3(x0, CONE_RADIUS * y1, CONE_RADIUS * z1));
				s_ArrowTri[ARROW_CONE].push_back(math::Vector3(x1, 0, 0));
				s_ArrowNorm[ARROW_CONE].push_back(math::Vector3(nn * nx, nn * y0, nn * z0));
				s_ArrowNorm[ARROW_CONE].push_back(math::Vector3(nn * nx, nn * y0, nn * z0));
				s_ArrowNorm[ARROW_CONE].push_back(math::Vector3(nn * nx, nn * y1, nn * z1));
				s_ArrowNorm[ARROW_CONE].push_back(math::Vector3(nn * nx, nn * y0, nn * z0));
				s_ArrowNorm[ARROW_CONE].push_back(math::Vector3(nn * nx, nn * y1, nn * z1));
				s_ArrowNorm[ARROW_CONE].push_back(math::Vector3(nn * nx, nn * y1, nn * z1));
				s_ArrowTri[ARROW_CONE_CAP].push_back(math::Vector3(x0, 0, 0));
				s_ArrowTri[ARROW_CONE_CAP].push_back(math::Vector3(x0, CONE_RADIUS * y1, CONE_RADIUS * z1));
				s_ArrowTri[ARROW_CONE_CAP].push_back(math::Vector3(x0, CONE_RADIUS * y0, CONE_RADIUS * z0));
				s_ArrowNorm[ARROW_CONE_CAP].push_back(math::Vector3(-1, 0, 0));
				s_ArrowNorm[ARROW_CONE_CAP].push_back(math::Vector3(-1, 0, 0));
				s_ArrowNorm[ARROW_CONE_CAP].push_back(math::Vector3(-1, 0, 0));
			}

			for (int i = 0; i < 4; ++i)
			{
				s_ArrowTriProj[i].clear();
				s_ArrowTriProj[i].resize(s_ArrowTri[i].size());
				s_ArrowColLight[i].clear();
				s_ArrowColLight[i].resize(s_ArrowTri[i].size());
			}
		}

		ImU32 ArrowWidget::blendColor(ImU32 c1, ImU32 c2, float t)
		{
			ImColor color1(c1);
			ImColor color2(c2);
			const float invt = 1.0f - t;
			color1 = ImColor((color1.Value.x * invt) + (color2.Value.x * t), (color1.Value.y * invt) + (color2.Value.y * t),
							 (color1.Value.z * invt) + (color2.Value.z * t), (color1.Value.w * invt) + (color2.Value.w * t));
			return color1;
		}

		void ArrowWidget::quatFromDirection(math::Quaternion& out, const math::Vector3& dir)
		{
			const float dn = dir.GetLength();
			if (dn < FLT_EPSILON * FLT_EPSILON)
			{
				out.x = out.y = out.z = 0;
				out.w = 1;
			}
			else
			{
				math::Vector3 rotAxis(0.f, -dir.z, dir.y);
				if (rotAxis.Dot(rotAxis) < FLT_EPSILON * FLT_EPSILON)
				{
					rotAxis.x = rotAxis.y = 0;
					rotAxis.z = 1;
				}
				else
					rotAxis = rotAxis.Normalize();
				const float rotAngle = std::acos(ClampF(dir.x / dn, -1.0f, 1.0f));
				out = math::Quaternion(rotAxis, rotAngle);
				out.Normalize();
			}
		}
	} // namespace

	bool DirectionWidget(const char* label, float v[3])
	{
		ImGui::PushID(label);
		ImGui::BeginGroup();
		math::Vector3 dir(v[0], v[1], v[2]);

		bool changed = ImGui::DragFloat3(label, &dir.x, 0.01f, -1.0f, 1.0f);
		if (changed)
		{
			v[0] = dir.x;
			v[1] = dir.y;
			v[2] = dir.z;
		}

		if (dir.GetSqrLength() < 1e-12f)
			dir = math::Vector3(1.f, 0.f, 0.f);
		else
			dir = dir.Normalize();

		ArrowWidget widget(dir);
		if (widget.draw() && !changed)
		{
			changed = true;
			dir = widget.getDirection();
			v[0] = dir.x;
			v[1] = dir.y;
			v[2] = dir.z;
		}
		ImGui::EndGroup();
		ImGui::PopID();
		return changed;
	}
} // namespace viewer_imgui
