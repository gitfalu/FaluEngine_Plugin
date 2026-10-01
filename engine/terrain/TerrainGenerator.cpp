#include "TerrainGenerator.h"
#include "core/Logger.h"
#include <cmath>
#include <random>

namespace FaluEngine
{
	namespace 
	{
		struct ValueNoise2D
		{
			std::vector<float> table;
			static constexpr int kSize = 256;
			static constexpr int kMask = kSize - 1;

			explicit ValueNoise2D(uint32_t seed)
			{
				std::mt19937 rng(seed);
				std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
				table.resize(static_cast<size_t>(kSize) * kSize);
				for (auto& v : table) v = dist(rng);
			}

			[[nodiscard]] float sampleGrid(int x, int z) const
			{
				x &= kMask;
				z &= kMask;
				return table[static_cast<size_t>(z) * kSize + x];
			}

			static float smoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

			[[nodiscard]] float sample(float x, float z) const
			{
				int x0 = static_cast<int>(std::floor(x));
				int z0 = static_cast<int>(std::floor(z));
				int x1 = x0 + 1;
				int z1 = z0 + 1;

				float tx = smoothStep(x - static_cast<float>(x0));
				float tz = smoothStep(z - static_cast<float>(z0));

				float v00 = sampleGrid(x0, z0);
				float v10 = sampleGrid(x1, z0);
				float v01 = sampleGrid(x0, z1);
				float v11 = sampleGrid(x1, z1);

				float a = v00 + (v10 - v00) * tx;
				float b = v01 + (v11 - v01) * tx;
				return a + (b - a) * tz;
			}

			[[nodiscard]] float fbm(float x, float z, int octaves, float lacunarity, float persistence) const
			{
				float amplitude = 1.0f;
				float frequency = 1.0f;
				float sum = 0.0f;
				float maxAmp = 0.0f;
				for (int i = 0; i < octaves; ++i)
				{
					sum += sample(x * frequency, z * frequency) * amplitude;
					maxAmp += amplitude;
					amplitude *= persistence;
					frequency *= lacunarity;
				}
				return maxAmp > 0.0f ? sum / maxAmp : 0.0f;
			}

		};
	} // namespace
	void generateTerrainHeights(TerrainComponent& terrain)
	{
		const uint32_t vx = terrain.vertexCountX();
		const uint32_t vz = terrain.vertexCountZ();

		terrain.heights.assign(static_cast<size_t>(vx) * vz, 0.0f);

		ValueNoise2D noise(terrain.seed);

		for (uint32_t z = 0; z < vz; ++z)
		{
			for (uint32_t x = 0; x < vx; ++x)
			{
				float nx = static_cast<float>(x) * terrain.noiseFrequency;
				float nz = static_cast<float>(z) * terrain.noiseFrequency;

				float h = noise.fbm(nx, nz, terrain.octaves, terrain.lacunarity, terrain.persistence);
				terrain.heights[static_cast<size_t>(z) * vx + x] = h * terrain.heightScale;
			}
		}
		terrain.dirty = false;
		LOG_INFO("TerrainGenerator: generated heights ({}x{}, seed = {})", vx, vz, terrain.seed);
	}
	std::shared_ptr<MeshAsset> buildTerrainMesh(TerrainComponent& terrain, ID3D11Device* device)
	{
		if (terrain.dirty || terrain.heights.empty())
		{
			generateTerrainHeights(terrain);
		}

		const uint32_t vx = terrain.vertexCountX();
		const uint32_t vz = terrain.vertexCountZ();

		const float stepX = terrain.sizeX / static_cast<float>(terrain.resolutionX);
		const float stepZ = terrain.sizeZ / static_cast<float>(terrain.resolutionZ);

		auto asset = std::make_shared<MeshAsset>();
		asset->vertices.resize(static_cast<size_t>(vx) * vz);

		auto heightAt = [&](int x, int z) ->float {
			x = (std::min)((std::max)(x, 0), static_cast<int>(vx) - 1);
			z = (std::min)((std::max)(z, 0), static_cast<int>(vz) - 1);
			return terrain.heights[static_cast<size_t>(z) * vx + x];
			};

		for (uint32_t z = 0; z < vz; ++z)
		{
			for (uint32_t x = 0; x < vx; ++x)
			{
				Vertex v{};
				float worldX = static_cast<float>(x) * stepX;
				float worldZ = static_cast<float>(z) * stepZ;
				float h = heightAt(static_cast<int>(x), static_cast<int>(z));

				v.position = { worldX,h,worldZ };
				v.color = { 1.0f,1.0f,1.0f,1.0f };
				v.uv = {
					static_cast<float>(x) / static_cast<float>(terrain.resolutionX),
					static_cast<float>(z) / static_cast<float>(terrain.resolutionZ)
				};

				// 中心差分で法線を計算
				float hL = heightAt(static_cast<int>(x) - 1, static_cast<int>(z));
				float hR = heightAt(static_cast<int>(x) + 1, static_cast<int>(z));
				float hD = heightAt(static_cast<int>(x), static_cast<int>(z) - 1);
				float hU = heightAt(static_cast<int>(x), static_cast<int>(z) + 1);

				glm::vec3 normal = glm::normalize(glm::vec3(
					(hL - hR) / (2.0f * stepX),
					1.0f,
					(hD - hU) / (2.0f * stepZ)
				));
				v.normal = normal;
				v.tangent = glm::normalize(glm::vec3(1.0f, (hR - hL) / (2.0f * stepX), 0.0f));
				v.bitangent = glm::normalize(glm::cross(v.normal, v.tangent));

				asset->vertices[static_cast<size_t>(z) * vx + x] = v;
			}
		}

		asset->indices.reserve(static_cast<size_t>(terrain.resolutionX) * terrain.resolutionZ * 6);
		for (uint32_t z = 0; z < terrain.resolutionZ; ++z)
		{
			for (uint32_t x = 0; x < terrain.resolutionX; ++x)
			{
				uint32_t i0 = z * vx + x;
				uint32_t i1 = z * vx + (x + 1);
				uint32_t i2 = (z + 1) * vx + x;
				uint32_t i3 = (z + 1) * vx + (x + 1);

				// 三角形の構成順序はMeshLoaderのAssimpに準拠し時計回り
				asset->indices.push_back(i0);
				asset->indices.push_back(i2);
				asset->indices.push_back(i1);

				asset->indices.push_back(i1);
				asset->indices.push_back(i2);
				asset->indices.push_back(i3);
			}
		}

		SubMesh sub{};
		sub.indexOffset = 0;
		sub.indexCount = static_cast<uint32_t>(asset->indices.size());
		asset->subMeshes.push_back(sub);

		if (device)
		{
			D3D11_BUFFER_DESC vbd = {};
			vbd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * asset->vertices.size());
			vbd.Usage = D3D11_USAGE_IMMUTABLE;
			vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

			D3D11_SUBRESOURCE_DATA vData = {};
			vData.pSysMem = asset->vertices.data();
			device->CreateBuffer(&vbd, &vData, &asset->vertexBuffer);

			D3D11_BUFFER_DESC ibd = {};
			ibd.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * asset->indices.size());
			ibd.Usage = D3D11_USAGE_IMMUTABLE;
			ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

			D3D11_SUBRESOURCE_DATA iData = {};
			iData.pSysMem = asset->indices.data();
			device->CreateBuffer(&ibd, &iData, &asset->indexBuffer);
		}

		LOG_INFO("TerrainGenerator: built mesh ({} verices, {} indices)",
			asset->vertices.size(), asset->indices.size());

		return asset;
	}
}
