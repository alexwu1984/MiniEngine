#include "Renderer.h"
#include "Scene.h"
#include <fstream>

constexpr float kInfinity = std::numeric_limits<float>::max();

// Compute reflection direction
math::Vector3 reflect(const math::Vector3& I, const math::Vector3& N)
{
	return I - 2 * math::Vector3::Dot(I, N) * N;
}

// [comment]
// Compute refraction direction using Snell's law
//
// We need to handle with care the two possible situations:
//
//    - When the ray is inside the object
//
//    - When the ray is outside.
//
// If the ray is outside, you need to make cosi positive cosi = -N.I
//
// If the ray is inside, you need to invert the refractive indices and negate the normal N
// [/comment]
math::Vector3 refract(const math::Vector3& I, const math::Vector3& N, const float& ior)
{
	float cosi = math::Clamp(math::Vector3::Dot(I, N) ,-1.f, 1.f);
	float etai = 1, etat = ior;
	math::Vector3 n = N;
	if (cosi < 0) { cosi = -cosi; }
	else { std::swap(etai, etat); n = -N; }
	float eta = etai / etat;
	float k = 1 - eta * eta * (1 - cosi * cosi);
	return k < 0 ? 0 : eta * I + (eta * cosi - sqrtf(k)) * n;
}

// [comment]
// Compute Fresnel equation
//
// \param I is the incident view direction
//
// \param N is the normal at the intersection point
//
// \param ior is the material refractive index
// [/comment]
float fresnel(const math::Vector3& I, const math::Vector3& N, const float& ior)
{
	float cosi = math::Clamp(math::Vector3::Dot(I, N), -1.f, 1.f);
	float etai = 1, etat = ior;
	if (cosi > 0) { std::swap(etai, etat); }
	// Compute sini using Snell's law
	float sint = etai / etat * math::Sqrt(std::max(0.f, 1 - cosi * cosi));
	// Total internal reflection
	if (sint >= 1) {
		return 1;
	}
	else {
		float cost = math::Sqrt(std::max(0.f, 1 - sint * sint));
		cosi = fabsf(cosi);
		float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
		float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
		return (Rs * Rs + Rp * Rp) / 2;
	}
	// As a consequence of the conservation of energy, transmittance is given by:
	// kt = 1 - kr;
}

// [comment]
// Returns true if the ray intersects an object, false otherwise.
//
// \param orig is the ray origin
// \param dir is the ray direction
// \param objects is the list of objects the scene contains
// \param[out] tNear contains the distance to the cloesest intersected object.
// \param[out] index stores the index of the intersect triangle if the interesected object is a mesh.
// \param[out] uv stores the u and v barycentric coordinates of the intersected point
// \param[out] *hitObject stores the pointer to the intersected object (used to retrieve material information, etc.)
// \param isShadowRay is it a shadow ray. We can return from the function sooner as soon as we have found a hit.
// [/comment]
std::optional<hit_payload> trace(
	const math::Vector3& orig, const math::Vector3& dir,
	const std::vector<std::unique_ptr<Object> >& objects)
{
	float tNear = kInfinity;
	std::optional<hit_payload> payload;
	for (const auto& object : objects)
	{
		float tNearK = kInfinity;
		uint32_t indexK;
		math::Vector2 uvK;
		if (object->intersect(orig, dir, tNearK, indexK, uvK) && tNearK < tNear)
		{
			payload.emplace();
			payload->hit_obj = object.get();
			payload->tNear = tNearK;
			payload->index = indexK;
			payload->uv = uvK;
			tNear = tNearK;
		}
	}

	return payload;
}

// [comment]
// Implementation of the Whitted-style light transport algorithm (E [S*] (D|G) L)
//
// This function is the function that compute the color at the intersection point
// of a ray defined by a position and a direction. Note that thus function is recursive (it calls itself).
//
// If the material of the intersected object is either reflective or reflective and refractive,
// then we compute the reflection/refraction direction and cast two new rays into the scene
// by calling the castRay() function recursively. When the surface is transparent, we mix
// the reflection and refraction color using the result of the fresnel equations (it computes
// the amount of reflection and refraction depending on the surface normal, incident view direction
// and surface refractive index).
//
// If the surface is diffuse/glossy we use the Phong illumation model to compute the color
// at the intersection point.
// [/comment]
math::Vector3 castRay(
	const math::Vector3& orig, const math::Vector3& dir, const Scene& scene,
	int depth)
{
	if (depth > scene.maxDepth) {
		return math::Vector3(0.0, 0.0, 0.0);
	}

	math::Vector3 hitColor = scene.backgroundColor;
	if (auto payload = trace(orig, dir, scene.get_objects()); payload)
	{
		math::Vector3 hitPoint = orig + dir * payload->tNear;
		math::Vector3 N; // normal
		math::Vector2 st; // st coordinates
		payload->hit_obj->getSurfaceProperties(hitPoint, dir, payload->index, payload->uv, N, st);
		switch (payload->hit_obj->materialType) {
		case REFLECTION_AND_REFRACTION:
		{
			math::Vector3 reflectionDirection = reflect(dir, N).Normalize();
			math::Vector3 refractionDirection = refract(dir, N, payload->hit_obj->ior).Normalize();
			math::Vector3 reflectionRayOrig = (math::Vector3::Dot(reflectionDirection, N) < 0) ?
				hitPoint - N * scene.epsilon :
				hitPoint + N * scene.epsilon;
			math::Vector3 refractionRayOrig = (math::Vector3::Dot(refractionDirection, N) < 0) ?
				hitPoint - N * scene.epsilon :
				hitPoint + N * scene.epsilon;
			math::Vector3 reflectionColor = castRay(reflectionRayOrig, reflectionDirection, scene, depth + 1);
			math::Vector3 refractionColor = castRay(refractionRayOrig, refractionDirection, scene, depth + 1);
			float kr = fresnel(dir, N, payload->hit_obj->ior);
			hitColor = reflectionColor * kr + refractionColor * (1 - kr);
			break;
		}
		case REFLECTION:
		{
			float kr = fresnel(dir, N, payload->hit_obj->ior);
			math::Vector3 reflectionDirection = reflect(dir, N);
			math::Vector3 reflectionRayOrig = (math::Vector3::Dot(reflectionDirection, N) < 0) ?
				hitPoint + N * scene.epsilon :
				hitPoint - N * scene.epsilon;
			hitColor = castRay(reflectionRayOrig, reflectionDirection, scene, depth + 1) * kr;
			break;
		}
		default:
		{
			// [comment]
			// We use the Phong illumation model int the default case. The phong model
			// is composed of a diffuse and a specular reflection component.
			// [/comment]
			math::Vector3 lightAmt = 0, specularColor = 0;
			math::Vector3 shadowPointOrig = (math::Vector3::Dot(dir, N) < 0) ?
				hitPoint + N * scene.epsilon :
				hitPoint - N * scene.epsilon;
			// [comment]
			// Loop over all lights in the scene and sum their contribution up
			// We also apply the lambert cosine law
			// [/comment]
			for (auto& light : scene.get_lights()) {
				math::Vector3 lightDir = light->position - hitPoint;
				// square of the distance between hitPoint and the light
				float lightDistance2 = math::Vector3::Dot(lightDir, lightDir);
				// is the point in shadow, and is the nearest occluding object closer to the object than the light itself?
				auto shadow_res = trace(shadowPointOrig, lightDir, scene.get_objects());
				bool inShadow = shadow_res && (shadow_res->tNear * shadow_res->tNear < lightDistance2);

				lightAmt += light->illuminate(lightDir, N, inShadow);

				math::Vector3 reflectionDirection = reflect(-lightDir.Normalize(), N);

				specularColor += powf(std::max(0.f, -math::Vector3::Dot(reflectionDirection, dir)),
					payload->hit_obj->specularExponent) * light->intensity;
			}

			hitColor = lightAmt * payload->hit_obj->evalDiffuseColor(st) * payload->hit_obj->Kd + specularColor * payload->hit_obj->Ks;
			break;
		}
		}
	}

	return hitColor;
}

// [comment]
// The main render function. This where we iterate over all pixels in the image, generate
// primary rays and cast these rays into the scene. The content of the framebuffer is
// saved to a file.
// [/comment]
void Renderer::Render(const Scene& scene)
{
	m_frameBuffer.resize(scene.width * scene.height);

	float scale = std::tan(math::Radians(scene.fov * 0.5f));
	float imageAspectRatio = scene.width / (float)scene.height;

	// Use this variable as the eye position to start your rays.
	math::Vector3 eye_pos(0);
	int m = 0;
	for (int j = 0; j < scene.height; ++j)
	{
		for (int i = 0; i < scene.width; ++i)
		{
			// generate primary ray direction
			float x;
			float y;

			//https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-generating-camera-rays/generating-camera-rays
			float ndcX = (float(i) + 0.5) / float(scene.width);
			float ndcY = (float(j) + 0.5) / float(scene.height);
			x = (2 * ndcX - 1) * imageAspectRatio * scale;
			y = (1 - 2 * ndcY) * scale;

			// TODO: Find the x and y positions of the current pixel to get the direction
			// vector that passes through it.
			// Also, don't forget to multiply both of them with the variable *scale*, and
			// x (horizontal) variable with the *imageAspectRatio*            

			math::Vector3 dir = math::Vector3(x, y, -1); // Don't forget to normalize this direction!
			dir = dir.Normalize();
			math::Vector3 color = castRay(eye_pos, dir, scene, 0);
			m_frameBuffer[m++] = core::FColor(255 * math::Clamp(color.x, 0.f, 1.f), 255 * math::Clamp(color.y, 0.f, 1.f), 255 * math::Clamp(color.z, 0.f, 1.f));
		}
	}
}

uint8_t* Renderer::GetBuffer()
{
	return &(m_frameBuffer[0].B);
}
