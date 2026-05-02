#pragma once
#include "math/vector2.h"
#include "math/vector3.h"
#include "global.h"

class Object
{
public:
	Object()
		: materialType(DIFFUSE_AND_GLOSSY)
		, ior(1.3)
		, Kd(0.8)
		, Ks(0.2)
		, diffuseColor(0.2)
		, specularExponent(25)
	{}

	virtual ~Object() = default;

	virtual bool intersect(const math::Vector3&, const math::Vector3&, float&, uint32_t&, math::Vector2&) const = 0;

	virtual void getSurfaceProperties(const math::Vector3&, const math::Vector3&, const uint32_t&, const math::Vector2&, math::Vector3&,
		math::Vector2&) const = 0;

	virtual math::Vector3 evalDiffuseColor(const math::Vector2&) const
	{
		return diffuseColor;
	}

	// material properties
	MaterialType materialType;
	float ior;
	float Kd, Ks;
	math::Vector3 diffuseColor;
	float specularExponent;
};