#pragma once
#include "Scene/Component.h"
#include "math/matrix4x4.h"
#include "math/frustum.h"

namespace Engine
{
	struct CameraComponentPrivate;

	class CameraComponent : public Component
	{
	public:
		DECLARE_COMPONENT_CLASS_NAME(CameraComponent)
		CameraComponent(std::weak_ptr<Actor> Owner);
		virtual ~CameraComponent();

		virtual void InitResource() override;
		virtual void Tick(float DeltaTime) override;

		void SetViewMatrix(const math::Matrix4x4& view);
		void SetCameraPos(const math::Vector3& Pos);
		/** Perspective clip planes (LH); applied on next projection build (Tick / SetViewMatrix). */
		void SetClipDistancePlanes(float NearZ, float FarZ);

		/** World-space point the camera looks at (glTF viewer–style framing). Disable to restore legacy Target = +Z. */
		void SetExplicitLookAtWorldTarget(const math::Vector3& worldLookAt, bool bEnable);
		float GetFovVerticalRadians() const;

		math::Matrix4x4 GetViewMatrix()const;
		math::Matrix4x4 GetPrevViewMatrix()const;
		math::Vector3 GetCameraPos() const;
	 
		math::Matrix4x4 GetPrevProjMatrix() const;
		math::Matrix4x4 GetProjMatrix() const;
		float GetNearPlane() const;
		float GetFarPlane() const;
		math::Vector4 GetTemporalAAJitter() const;

		void UpdateFrustum(const math::Vector3& eye, const math::Vector3& forward, const math::Vector3& up);
		const math::Frustum& GetFrustum() const;

		void SetProjectionJitter(uint32_t width, uint32_t height);

		math::Matrix4x4 HackAddTemporalAAProjectionJitter( bool PrevFrame = false);
		int32_t GetFrameIndexMod2() const;
		int32_t GetFrameIndex() const;

		/** Increments a serial consumed by TAA/SSR to discard invalid history (cuts, teleports). */
		void NotifyTemporalHistoryInvalidate();
		uint32_t GetTemporalHistoryGeneration() const;

		/**
		 * Full scene swap after LoadScene (game thread): bumps temporal generation and schedules first-frame prev-matrix sync.
		 * UE analogue: marking the primary ViewState invalid / camera-cut without snapping matrices before Tick builds View.
		 */
		void MarkTemporalHistoryStaleAfterSceneCut();

	protected:
		CameraComponentPrivate* d_ptr;

		/** Call at end of Tick after SetProjectionJitter: aligns prev view/proj/jitter for zero velocity on first frame (new camera / scene reload). */
		void EnsureTemporalPrevMatricesInitialized();

	};
	DECLARE_COMPONENT_TRAITS_CLASS_NAME(CameraComponent);
}
