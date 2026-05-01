#include "Procedural/ProceduralFloor.h"
#include "GltfModel/GltfMeshInfo.h"
#include "GltfModel/GltfMeshBuffer.h"
#include "GltfModel/MeshBase.h"
#include "GltfModel/DynamicBoneInfo.h"
#include "Scene/SceneModelAsset.h"
#include "Material/MaterialBase.h"
#include "Engine/Engine.h"
#include "RHI/DynamicRHI.h"
#include "RHI/RHITexture2D.h"
#include "RHI/RHIDefinitions.h"
#include "Thread/RenderThread.h"

namespace Engine
{
	namespace
	{
		struct BrushedMetalConfig
		{
			bool Enable = false;
			int32_t TextureSize = 512;
			math::Vector2 DirectionUV = math::Vector2(1.f, 0.f); // along U by default
			float StreakScale = 220.0f;
			float NormalStrength = 0.25f;
			float RoughnessBase = 0.78f;
			float RoughnessVariation = 0.18f;
		};

		struct FrostedConfig
		{
			bool Enable = false;
			int32_t TextureSize = 512;
			float NoiseScale = 320.0f;      // density of bumps
			float NormalStrength = 0.22f;   // bump amplitude
			float RoughnessBase = 0.68f;
			float RoughnessVariation = 0.22f;
		};

		static bool TryParseVec2(const nlohmann::json& JsonValue, math::Vector2& Out)
		{
			try
			{
				if (JsonValue.is_string())
				{
					std::string s = JsonValue.get<std::string>();
					return std::sscanf(s.c_str(), "%f,%f", &Out.x, &Out.y) == 2;
				}
				if (JsonValue.is_array() && JsonValue.size() >= 2)
				{
					Out.x = JsonValue[0].get<float>();
					Out.y = JsonValue[1].get<float>();
					return true;
				}
			}
			catch (...)
			{
			}
			return false;
		}

		static bool TryParseIVec2(const nlohmann::json& JsonValue, int32_t& OutX, int32_t& OutY)
		{
			try
			{
				if (JsonValue.is_string())
				{
					std::string s = JsonValue.get<std::string>();
					return std::sscanf(s.c_str(), "%d,%d", &OutX, &OutY) == 2;
				}
				if (JsonValue.is_array() && JsonValue.size() >= 2)
				{
					OutX = JsonValue[0].get<int32_t>();
					OutY = JsonValue[1].get<int32_t>();
					return true;
				}
			}
			catch (...)
			{
			}
			return false;
		}

		struct ProceduralMeshInfo final : public GltfMeshInfo
		{
			std::vector<math::Vector3> VertData;
			std::vector<math::Vector3> NormalData;
			std::vector<math::Vector2> UVData;
			std::vector<math::Vector4> TangentData;
			std::vector<uint32_t> IndexData;

			explicit ProceduralMeshInfo(uint32_t VertCount, uint32_t IndexCount)
			{
				VertData.resize(VertCount);
				NormalData.resize(VertCount);
				UVData.resize(VertCount);
				TangentData.resize(VertCount);
				IndexData.resize(IndexCount);

				nNumVertices = VertCount;
				// Engine RHIIndexBuffer takes triangle count and internally multiplies by 3.
				nNumFaces = IndexCount / 3;
				Vertices = VertData.data();
				Normals = NormalData.data();
				TextureCoords = UVData.data();
				Tangents = TangentData.data();
				FacesIndex = nullptr;
				FacesIndex32 = IndexData.data();
				type = 0;
				BoneIDs = nullptr;
				BoneWeights = nullptr;
			}
		};

		class ProceduralPBRMaterial final : public MaterialBase
		{
		public:
			ProceduralPBRMaterial(const MaterialConfig& InConfig, const BrushedMetalConfig& InBrushed, const FrostedConfig& InFrosted)
				: Config(InConfig)
				, Brushed(InBrushed)
				, Frosted(InFrosted)
			{
				Config.UseConfig = true;
				auto CreateTexCommand = [this](RenderCore::DynamicRHI* DyRHI) {
					auto MakeTexBGRA = [](int32_t W, int32_t H, const uint8_t* Data) -> std::shared_ptr<RenderCore::RHITexture2D>
						{
							return GEngine->GetRHI()->RHICreateTexture2D(
								RenderCore::EPixelFormat::PF_B8G8R8A8,
								RenderCore::ETextureCreateFlags::TexCreate_ShaderResource,
								W, H, 1, (void*)Data, W * 4);
						};

					auto Make1x1BGRA = [](const core::FLinearColor& C) -> std::shared_ptr<RenderCore::RHITexture2D>
						{
							uint8_t bgra[] = {
								(uint8_t)(C.B * 255.0f),
								(uint8_t)(C.G * 255.0f),
								(uint8_t)(C.R * 255.0f),
								(uint8_t)(C.A * 255.0f)
							};
							return GEngine->GetRHI()->RHICreateTexture2D(
								RenderCore::EPixelFormat::PF_B8G8R8A8,
								RenderCore::ETextureCreateFlags::TexCreate_ShaderResource,
								1, 1, 1, bgra, 4);
						};

					BaseColorTex = Make1x1BGRA(core::FLinearColor(Config.BaseColor));
					MetalRoughTex = Make1x1BGRA(core::FLinearColor(1.f, float(Config.Roughness), float(Config.Metallic), 1.f));
					NormalTex = Make1x1BGRA(core::FLinearColor(0.5f, 0.5f, 1.f, 1.f));
					EmissiveTex = Make1x1BGRA(core::FLinearColor(0.f, 0.f, 0.f, 1.f));
					OcclusionTex = Make1x1BGRA(core::FLinearColor(1.f, 1.f, 1.f, 1.f));

					// Procedural surface overrides (prefer Frosted if both enabled)
					if (Frosted.Enable)
					{
						const int32_t S = (std::max)(16, Frosted.TextureSize);
						std::vector<uint8_t> NormalBGRA(size_t(S) * size_t(S) * 4);
						std::vector<uint8_t> MRBGRA(size_t(S) * size_t(S) * 4);

						auto frac = [](float v) { return v - std::floor(v); };
						auto smooth01 = [](float t) { return t * t * (3.f - 2.f * t); };
						auto hash2 = [&](int32_t x, int32_t y)
							{
								// cheap hash to [0,1)
								float n = std::sin(float(x) * 127.1f + float(y) * 311.7f) * 43758.5453f;
								return frac(n);
							};
						auto noise2 = [&](float x, float y)
							{
								int32_t x0 = (int32_t)std::floor(x);
								int32_t y0 = (int32_t)std::floor(y);
								float tx = x - float(x0);
								float ty = y - float(y0);
								float a = hash2(x0, y0);
								float b = hash2(x0 + 1, y0);
								float c = hash2(x0, y0 + 1);
								float d = hash2(x0 + 1, y0 + 1);
								float ux = smooth01(tx);
								float uy = smooth01(ty);
								float ab = a + (b - a) * ux;
								float cd = c + (d - c) * ux;
								return ab + (cd - ab) * uy;
							};

						for (int32_t y = 0; y < S; ++y)
						{
							for (int32_t x = 0; x < S; ++x)
							{
								float u = float(x) / float(S);
								float v = float(y) / float(S);
								float px = u * Frosted.NoiseScale;
								float py = v * Frosted.NoiseScale;

								float h = noise2(px, py);
								float hx = noise2(px + 1.0f, py) - noise2(px - 1.0f, py);
								float hy = noise2(px, py + 1.0f) - noise2(px, py - 1.0f);
								float nx = hx * 0.5f * Frosted.NormalStrength;
								float ny = hy * 0.5f * Frosted.NormalStrength;
								float l2 = nx * nx + ny * ny;
								if (l2 > 0.95f)
								{
									float invl = 1.0f / std::sqrt(l2);
									nx *= invl * 0.97f;
									ny *= invl * 0.97f;
								}

								uint8_t r = (uint8_t)(std::clamp(nx * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f);
								uint8_t g = (uint8_t)(std::clamp(ny * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f);
								uint8_t b = 128;
								uint8_t a = 255;
								size_t n = (size_t(y) * size_t(S) + size_t(x)) * 4;
								NormalBGRA[n + 0] = b;
								NormalBGRA[n + 1] = g;
								NormalBGRA[n + 2] = r;
								NormalBGRA[n + 3] = a;

								float rough = Frosted.RoughnessBase + (h - 0.5f) * 2.0f * Frosted.RoughnessVariation;
								rough = std::clamp(rough, 0.02f, 0.98f);
								float metallic = std::clamp(float(Config.Metallic), 0.0f, 1.0f);

								size_t m = n;
								MRBGRA[m + 0] = (uint8_t)(metallic * 255.0f); // B = metallic
								MRBGRA[m + 1] = (uint8_t)(rough * 255.0f);   // G = roughness
								MRBGRA[m + 2] = 255;
								MRBGRA[m + 3] = 255;
							}
						}

						NormalTex = MakeTexBGRA(S, S, NormalBGRA.data());
						MetalRoughTex = MakeTexBGRA(S, S, MRBGRA.data());
					}
					else if (Brushed.Enable)
					{
						const int32_t S = (std::max)(16, Brushed.TextureSize);
						std::vector<uint8_t> NormalBGRA(size_t(S) * size_t(S) * 4);
						std::vector<uint8_t> MRBGRA(size_t(S) * size_t(S) * 4);

						auto frac = [](float v) { return v - std::floor(v); };
						auto smooth01 = [](float t) { return t * t * (3.f - 2.f * t); };
						auto hash1 = [&](int32_t i)
							{
								float x = std::sin(float(i) * 12.9898f) * 43758.5453f;
								return frac(x);
							};
						auto noise1 = [&](float p)
							{
								int32_t i0 = (int32_t)std::floor(p);
								float t = p - float(i0);
								float a = hash1(i0);
								float b = hash1(i0 + 1);
								return a + (b - a) * smooth01(t);
							};

						math::Vector2 dir = Brushed.DirectionUV;
						float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
						if (len < 1e-6f) { dir = math::Vector2(1.f, 0.f); len = 1.f; }
						dir.x /= len; dir.y /= len;
						math::Vector2 perp(-dir.y, dir.x);

						for (int32_t y = 0; y < S; ++y)
						{
							for (int32_t x = 0; x < S; ++x)
							{
								float u = float(x) / float(S);
								float v = float(y) / float(S);
								float p = (u * perp.x + v * perp.y) * Brushed.StreakScale;

								float h0 = noise1(p);
								float hp = noise1(p + 1.0f);
								float hm = noise1(p - 1.0f);
								float dh = (hp - hm) * 0.5f;

								float nx = perp.x * dh * Brushed.NormalStrength;
								float ny = perp.y * dh * Brushed.NormalStrength;
								float l2 = nx * nx + ny * ny;
								if (l2 > 0.95f)
								{
									float invl = 1.0f / std::sqrt(l2);
									nx *= invl * 0.97f;
									ny *= invl * 0.97f;
								}

								// encode XY into NormalMap.rg (shader reconstructs z)
								uint8_t r = (uint8_t)(std::clamp(nx * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f);
								uint8_t g = (uint8_t)(std::clamp(ny * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f);
								uint8_t b = 128;
								uint8_t a = 255;
								size_t n = (size_t(y) * size_t(S) + size_t(x)) * 4;
								// BGRA layout in memory
								NormalBGRA[n + 0] = b;
								NormalBGRA[n + 1] = g;
								NormalBGRA[n + 2] = r;
								NormalBGRA[n + 3] = a;

								float rough = Brushed.RoughnessBase + (h0 - 0.5f) * 2.0f * Brushed.RoughnessVariation;
								rough = std::clamp(rough, 0.02f, 0.98f);
								float metallic = std::clamp(float(Config.Metallic), 0.0f, 1.0f);

								uint8_t mr_b = (uint8_t)(metallic * 255.0f); // B channel = metallic
								uint8_t mr_g = (uint8_t)(rough * 255.0f);   // G channel = roughness
								MRBGRA[n + 0] = mr_b;
								MRBGRA[n + 1] = mr_g;
								MRBGRA[n + 2] = 255;
								MRBGRA[n + 3] = 255;
							}
						}

						NormalTex = MakeTexBGRA(S, S, NormalBGRA.data());
						MetalRoughTex = MakeTexBGRA(S, S, MRBGRA.data());
					}
					};
				ENQUEUE_UNIQUE_RENDER_COMMAND(CreateTexCommand);
			}

			MaterialType GetMaterialType() const override { return MaterialType::PBR; }
			std::string GetMaterialName() const override { return "ProceduralPBR"; }
			bool IsTransparent() const override { return false; }
			std::shared_ptr<RenderCore::RHITexture2D> GetBaseColorTexture() const override { return BaseColorTex; }
			std::shared_ptr<RenderCore::RHITexture2D> GetMetallicRoughnessTexture() const override { return MetalRoughTex; }
			std::shared_ptr<RenderCore::RHITexture2D> GetNormalTexture() const override { return NormalTex; }
			std::shared_ptr<RenderCore::RHITexture2D> GetEmissiveTexture() const override { return EmissiveTex; }
			std::shared_ptr<RenderCore::RHITexture2D> GetOcclusionTexture() const override { return OcclusionTex; }
			const MaterialConfig& GetMaterialConfig() const override { return Config; }

		private:
			MaterialConfig Config{};
			BrushedMetalConfig Brushed{};
			FrostedConfig Frosted{};
			std::shared_ptr<RenderCore::RHITexture2D> BaseColorTex;
			std::shared_ptr<RenderCore::RHITexture2D> MetalRoughTex;
			std::shared_ptr<RenderCore::RHITexture2D> NormalTex;
			std::shared_ptr<RenderCore::RHITexture2D> EmissiveTex;
			std::shared_ptr<RenderCore::RHITexture2D> OcclusionTex;
		};

		class ProceduralMesh final : public MeshBase
		{
		public:
			ProceduralMesh(std::string InName,
				std::shared_ptr<GltfMeshBuffer> InBuffer,
				std::shared_ptr<MaterialBase> InMaterial,
				const math::AABB3& InBox)
				: Name(std::move(InName))
				, Buffer(std::move(InBuffer))
				, Material(std::move(InMaterial))
				, Box(InBox)
				, MeshMat()
			{
				MeshMat.Identity();
			}

			std::shared_ptr<GltfMeshBuffer> GetMeshBuffer() override { return Buffer; }
			std::shared_ptr<MaterialBase> GetMaterial() override { return Material; }
			const math::AABB3& GetBoundingBox() const override { return Box; }
			const math::Matrix4x4& GetMeshMat() const override { return MeshMat; }
			std::string GetMeshName() const override { return Name; }
			bool HasSkin() const override { return false; }
			int32_t GetNodeId() const override { return -1; }
			int32_t GetSkinId() const override { return -1; }
			std::vector<std::vector<BoneSkinInfo>>& GetBoneNodeArray() override { return EmptyBoneArray; }

		private:
			std::string Name;
			std::shared_ptr<GltfMeshBuffer> Buffer;
			std::shared_ptr<MaterialBase> Material;
			math::AABB3 Box;
			math::Matrix4x4 MeshMat;
			std::vector<std::vector<BoneSkinInfo>> EmptyBoneArray;
		};
	}

	bool BuildProceduralFloor(const nlohmann::json& FloorJson, ProceduralBuildResult& OutResult)
	{
		try
		{
			math::Vector2 Size(40.f, 40.f);
			if (FloorJson.find("Size") != FloorJson.end())
				TryParseVec2(FloorJson["Size"], Size);
			if (FloorJson.find("Width") != FloorJson.end())
				Size.x = FloorJson["Width"].get<float>();
			if (FloorJson.find("Depth") != FloorJson.end())
				Size.y = FloorJson["Depth"].get<float>();

			int32_t SegX = 100;
			int32_t SegY = 100;
			if (FloorJson.find("Segments") != FloorJson.end())
				TryParseIVec2(FloorJson["Segments"], SegX, SegY);
			if (FloorJson.find("SegX") != FloorJson.end())
				SegX = FloorJson["SegX"].get<int32_t>();
			if (FloorJson.find("SegY") != FloorJson.end())
				SegY = FloorJson["SegY"].get<int32_t>();
			SegX = (std::max)(1, SegX);
			SegY = (std::max)(1, SegY);

			float UVScale = 8.0f;
			if (FloorJson.find("UVScale") != FloorJson.end())
				UVScale = FloorJson["UVScale"].get<float>();

			MaterialConfig MatCfg;
			MatCfg.UseConfig = true;
			MatCfg.Metallic = 1.0f;
			MatCfg.Roughness = 0.8f;
			MatCfg.BaseColor = math::Vector4(0.65f, 0.68f, 0.72f, 1.0f);

			BrushedMetalConfig BrushedCfg;
			FrostedConfig FrostedCfg;
			// Optional finish presets: "glossy" / "matte"
			std::string Finish;
			if (FloorJson.find("Finish") != FloorJson.end())
			{
				try { Finish = FloorJson["Finish"].get<std::string>(); } catch (...) { Finish.clear(); }
			}
			if (Finish == "glossy")
			{
				BrushedCfg.Enable = true;
				BrushedCfg.TextureSize = 512;
				BrushedCfg.DirectionUV = math::Vector2(1.f, 0.f);
				BrushedCfg.StreakScale = 420.0f;
				BrushedCfg.NormalStrength = 0.18f;
				BrushedCfg.RoughnessBase = 0.35f;
				BrushedCfg.RoughnessVariation = 0.08f;
				MatCfg.Roughness = 0.35f;
			}
			else if (Finish == "matte")
			{
				BrushedCfg.Enable = true;
				BrushedCfg.TextureSize = 512;
				BrushedCfg.DirectionUV = math::Vector2(1.f, 0.f);
				BrushedCfg.StreakScale = 260.0f;
				BrushedCfg.NormalStrength = 0.28f;
				BrushedCfg.RoughnessBase = 0.82f;
				BrushedCfg.RoughnessVariation = 0.16f;
				MatCfg.Roughness = 0.82f;
			}
			else if (Finish == "frosted")
			{
				FrostedCfg.Enable = true;
				FrostedCfg.TextureSize = 512;
				FrostedCfg.NoiseScale = 320.0f;
				FrostedCfg.NormalStrength = 0.22f;
				FrostedCfg.RoughnessBase = 0.68f;
				FrostedCfg.RoughnessVariation = 0.22f;
				MatCfg.Roughness = 0.68f;
			}
			if (FloorJson.find("BrushedMetal") != FloorJson.end())
			{
				const auto& B = FloorJson["BrushedMetal"];
				if (B.count("Enable")) BrushedCfg.Enable = B["Enable"].get<bool>();
				else BrushedCfg.Enable = true;
				if (B.count("TextureSize")) BrushedCfg.TextureSize = B["TextureSize"].get<int32_t>();
				if (B.count("StreakScale")) BrushedCfg.StreakScale = B["StreakScale"].get<float>();
				if (B.count("NormalStrength")) BrushedCfg.NormalStrength = B["NormalStrength"].get<float>();
				if (B.count("RoughnessBase")) BrushedCfg.RoughnessBase = B["RoughnessBase"].get<float>();
				if (B.count("RoughnessVariation")) BrushedCfg.RoughnessVariation = B["RoughnessVariation"].get<float>();
				if (B.count("Direction"))
				{
					math::Vector2 Dir;
					if (TryParseVec2(B["Direction"], Dir))
						BrushedCfg.DirectionUV = Dir;
				}
			}
			if (FloorJson.find("Frosted") != FloorJson.end())
			{
				const auto& F = FloorJson["Frosted"];
				if (F.count("Enable")) FrostedCfg.Enable = F["Enable"].get<bool>();
				else FrostedCfg.Enable = true;
				if (F.count("TextureSize")) FrostedCfg.TextureSize = F["TextureSize"].get<int32_t>();
				if (F.count("NoiseScale")) FrostedCfg.NoiseScale = F["NoiseScale"].get<float>();
				if (F.count("NormalStrength")) FrostedCfg.NormalStrength = F["NormalStrength"].get<float>();
				if (F.count("RoughnessBase")) FrostedCfg.RoughnessBase = F["RoughnessBase"].get<float>();
				if (F.count("RoughnessVariation")) FrostedCfg.RoughnessVariation = F["RoughnessVariation"].get<float>();
			}

			if (FloorJson.find("Material") != FloorJson.end())
			{
				const auto& MaterialJson = FloorJson["Material"];
				if (MaterialJson.count("Metallic")) MatCfg.Metallic = MaterialJson["Metallic"].get<float>();
				if (MaterialJson.count("Roughness")) MatCfg.Roughness = MaterialJson["Roughness"].get<float>();
				if (MaterialJson.count("BaseColor"))
				{
					std::string BaseColor = MaterialJson["BaseColor"];
					std::sscanf(BaseColor.c_str(), "%f,%f,%f,%f", &MatCfg.BaseColor.x, &MatCfg.BaseColor.y, &MatCfg.BaseColor.z, &MatCfg.BaseColor.w);
				}
			}

			const uint32_t VertCount = uint32_t(SegX + 1) * uint32_t(SegY + 1);
			const uint32_t IndexCount = uint32_t(SegX) * uint32_t(SegY) * 6u;
			auto MeshInfo = std::make_shared<ProceduralMeshInfo>(VertCount, IndexCount);

			const float HalfW = Size.x * 0.5f;
			const float HalfD = Size.y * 0.5f;
			const math::Vector3 N(0.f, 1.f, 0.f);
			const math::Vector4 T(1.f, 0.f, 0.f, 1.f);

			for (int32_t y = 0; y <= SegY; ++y)
			{
				const float v = float(y) / float(SegY);
				const float z = -HalfD + v * Size.y;
				for (int32_t x = 0; x <= SegX; ++x)
				{
					const float u = float(x) / float(SegX);
					const float px = -HalfW + u * Size.x;
					const uint32_t i = uint32_t(y) * uint32_t(SegX + 1) + uint32_t(x);
					MeshInfo->VertData[i] = math::Vector3(px, 0.f, z);
					MeshInfo->NormalData[i] = N;
					MeshInfo->TangentData[i] = T;
					MeshInfo->UVData[i] = math::Vector2(u * UVScale, v * UVScale);
				}
			}

			uint32_t idx = 0;
			for (int32_t y = 0; y < SegY; ++y)
			{
				for (int32_t x = 0; x < SegX; ++x)
				{
					const uint32_t i0 = uint32_t(y) * uint32_t(SegX + 1) + uint32_t(x);
					const uint32_t i1 = i0 + 1;
					const uint32_t i2 = i0 + uint32_t(SegX + 1);
					const uint32_t i3 = i2 + 1;

					MeshInfo->IndexData[idx++] = i0;
					MeshInfo->IndexData[idx++] = i2;
					MeshInfo->IndexData[idx++] = i1;
					MeshInfo->IndexData[idx++] = i1;
					MeshInfo->IndexData[idx++] = i2;
					MeshInfo->IndexData[idx++] = i3;
				}
			}

			auto Buffer = std::make_shared<GltfMeshBuffer>();
			Buffer->InitMesh(MeshInfo);
			auto Material = std::make_shared<ProceduralPBRMaterial>(MatCfg, BrushedCfg, FrostedCfg);

			math::AABB3 Box;
			Box.Set(math::Vector3(HalfW, 0.f, HalfD), math::Vector3(-HalfW, 0.f, -HalfD));

			OutResult.Meshes.clear();
			OutResult.Meshes.emplace_back(std::make_shared<ProceduralMesh>("ProceduralFloor", Buffer, Material, Box));
			OutResult.Box = Box;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}
}

