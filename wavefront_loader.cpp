#include "wavefront_loader.hpp"
#include <fstream>
#include <cassert>
#include <sstream>
#include <span>

#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <atomic>
#include <memory>

#include <iostream>

static std::vector<std::string> GenerateInputBuffer(const std::string_view XStr, const char Char)
{
	std::vector<std::string> InputBuffer;

	size_t LastI = 0;
	for (size_t i = 0; i < XStr.length(); i++)
	{
		if (XStr[i] == Char || i == XStr.length() - 1)
		{
			auto x = std::string(XStr.substr(LastI, (i + 1) - LastI));
			std::erase(x, Char);
			InputBuffer.push_back(x);
			LastI = i;
		}
	}

	return InputBuffer;
}

struct TriangleIndices
{
	std::vector<uint32_t> PositionIndices = std::vector<uint32_t>(3);
	std::vector<uint32_t> NormalIndices = std::vector<uint32_t>(3);
	std::vector<uint32_t> TextureCoordIndices = std::vector<uint32_t>(3);
};

static void ProcessTrianglesIntoObject(
	tnr::m3d::wavefront::tnrObject& Object, 
	const std::span<TriangleIndices>& TrianglesOutput, 
	const std::vector<tnr::m3d::wavefront::tnrObject::vec3<float>>& PositionsSource,
	const std::vector<tnr::m3d::wavefront::tnrObject::vec3<float>>& NormalsSource,
	const std::vector<tnr::m3d::wavefront::tnrObject::vec2<float>>& TextureCoordsSource,
	const bool ShouldFlipY
)
{
	Object.Positions.reserve(TrianglesOutput.size() * 3);
	Object.Normals.reserve(TrianglesOutput.size() * 3);
	Object.TextureCoords.reserve(TrianglesOutput.size() * 3);	

	for (const auto& Triangle : TrianglesOutput)
	{
		for (const auto Index : Triangle.PositionIndices)
		{
			auto Position = PositionsSource[Index];

			if (ShouldFlipY)
			{
				Position.y *= -1.0f;
			}

			Object.Positions.push_back(Position);
		}

		for (const auto Index : Triangle.NormalIndices)
		{
			Object.Normals.push_back(NormalsSource[Index]);
		}

		for (const auto Index : Triangle.TextureCoordIndices)
		{
			Object.TextureCoords.push_back(TextureCoordsSource[Index]);
		}
	}
}


namespace tnr::m3d::wavefront
{
	void tnrWavefrontLoader::LoadMaterials(const std::string& MTLFile)
	{
		std::ifstream File(MTLFile);

		std::string Cache;
		tnrMaterial MaterialCache;
		while (std::getline(File, Cache, '\n'))
		{
			if (Cache.length() < 1) continue;
			if (Cache[0] == '#') continue;

			auto InputBuffer = GenerateInputBuffer(Cache, ' ');

			if (InputBuffer[0] == "newmtl")
			{
				if (MaterialCache.MaterialName.length() > 0)
				{
					this->Materials.push_back(MaterialCache);
				}
				MaterialCache = tnrMaterial
				{
					.MaterialName = InputBuffer[1],
					.AmbientColor = {},
					.AlbedoColor = {},
					.SpecularColor = {},
					.EmissionColor = {},
					.SpecularFactor = 0,
					.RefractionFactor = 0,
					.AlphaFactor = 0,
					.AlbedoMap = std::nullopt,
					.NormalMap = std::nullopt
				};
			}
			else if (InputBuffer[0] == "Ka")
			{
				MaterialCache.AmbientColor =
				{
					std::stof(InputBuffer[1]),
					std::stof(InputBuffer[2]),
					std::stof(InputBuffer[3])
				};
			}
			else if (InputBuffer[0] == "Kd")
			{
				MaterialCache.AlbedoColor =
				{
					std::stof(InputBuffer[1]),
					std::stof(InputBuffer[2]),
					std::stof(InputBuffer[3])
				};
			}
			else if (InputBuffer[0] == "Ks")
			{
				MaterialCache.SpecularColor =
				{
					std::stof(InputBuffer[1]),
					std::stof(InputBuffer[2]),
					std::stof(InputBuffer[3])
				};
			}
			else if (InputBuffer[0] == "Ns")
			{
				MaterialCache.SpecularFactor = std::stof(InputBuffer[1]);
			}
			else if (InputBuffer[0] == "Ni")
			{
				MaterialCache.RefractionFactor = std::stof(InputBuffer[1]);
			}
			else if (InputBuffer[0] == "map_Kd")
			{
				MaterialCache.AlbedoMap = std::optional<tnrMaterial::tnrTextureInfo>(tnrMaterial::tnrTextureInfo{ InputBuffer[1] });
			}
			else if (InputBuffer[0] == "map_Ks")
			{
				MaterialCache.SpecularMap = std::optional<tnrMaterial::tnrTextureInfo>(tnrMaterial::tnrTextureInfo{ InputBuffer[1] });
			}
			else if (InputBuffer[0] == "map_bump" || InputBuffer[0] == "bump")
			{
				MaterialCache.NormalMap = std::optional<tnrMaterial::tnrTextureInfo>(tnrMaterial::tnrTextureInfo{ InputBuffer[1] });
			}
		}

		if (MaterialCache.MaterialName.length() > 0)
		{
			this->Materials.push_back(MaterialCache);
		}
	}
	void tnrWavefrontLoader::LoadObject(const std::string& ObjFile)
	{
		auto WholeStartTime = std::chrono::system_clock::now();

		const bool ShouldFlipY = this->Flags & tnrWavefrontOpenFlag::FLIP_POSITION_Y_AXIS;

		std::ifstream File(ObjFile);

		std::string LineCache;
		tnrObject ObjectCache;

		std::vector<tnrObject::vec3<float>> Positions;
		std::vector<tnrObject::vec3<float>> Normals;
		std::vector<tnrObject::vec2<float>> TextureCoords;
		std::vector<TriangleIndices> TrianglesOutput;
				
		while (std::getline(File, LineCache, '\n'))
		{
			if (LineCache.length() < 1) continue;
			if (LineCache[0] == '#') continue;

			//auto InputBuffer = SequenceLine(LineCache);
			auto InputBuffer = GenerateInputBuffer(LineCache, ' ');

			if (InputBuffer[0] == "o")
			{
				if (ObjectCache.ObjectName.length() > 0)
				{
					ProcessTrianglesIntoObject(ObjectCache, TrianglesOutput, Positions, Normals, TextureCoords, ShouldFlipY);
					this->Objects.push_back(ObjectCache);
				}

				TrianglesOutput.clear();

				ObjectCache = tnrObject
				{
					.ObjectName = InputBuffer[1],
					.MaterialName = "",
					.Positions = std::vector<tnrObject::vec3<float>>(),
					.Normals = std::vector<tnrObject::vec3<float>>()
				};
			}
			else if (InputBuffer[0] == "v")
			{
				tnr::m3d::wavefront::tnrObject::vec3<float> Cache
				{
					.x = std::stof(InputBuffer[1]),
					.y = std::stof(InputBuffer[2]),
					.z = std::stof(InputBuffer[3])
				};

				Positions.push_back(Cache);
			}
			else if (InputBuffer[0] == "vn")
			{
				tnr::m3d::wavefront::tnrObject::vec3<float> Cache
				{
					.x = std::stof(InputBuffer[1]),
					.y = std::stof(InputBuffer[2]),
					.z = std::stof(InputBuffer[3])
				};

				Normals.push_back(Cache);
			}
			else if (InputBuffer[0] == "vt")
			{
				tnr::m3d::wavefront::tnrObject::vec2<float> Cache
				{
					.x = std::stof(InputBuffer[1]),
					.y = std::stof(InputBuffer[2])
				};

				TextureCoords.push_back(Cache);
			}
			else if (InputBuffer[0] == "usemtl")
			{
				ObjectCache.MaterialName = InputBuffer[1];
			}
			else if (InputBuffer[0] == "f")
			{
				const auto ProcessedInputVertex0 = GenerateInputBuffer(InputBuffer[1], '/');
				const auto ProcessedInputVertex1 = GenerateInputBuffer(InputBuffer[2], '/');
				const auto ProcessedInputVertex2 = GenerateInputBuffer(InputBuffer[3], '/');

				const TriangleIndices TriangleCache
				{
					.PositionIndices
					{
						std::stoul(ProcessedInputVertex0[0]) - 1,
						std::stoul(ProcessedInputVertex1[0]) - 1,
						std::stoul(ProcessedInputVertex2[0]) - 1
					},
					.NormalIndices
					{
						std::stoul(ProcessedInputVertex0[2]) - 1,
						std::stoul(ProcessedInputVertex1[2]) - 1,
						std::stoul(ProcessedInputVertex2[2]) - 1
					},
					.TextureCoordIndices
					{
						std::stoul(ProcessedInputVertex0[1]) - 1,
						std::stoul(ProcessedInputVertex1[1]) - 1,
						std::stoul(ProcessedInputVertex2[1]) - 1
					}
				};

				 TrianglesOutput.push_back(TriangleCache);
			}
		}
		if (ObjectCache.ObjectName.length() > 0)
		{
			ProcessTrianglesIntoObject(ObjectCache, TrianglesOutput, Positions, Normals, TextureCoords, ShouldFlipY);
			this->Objects.push_back(ObjectCache);
		}
	}

	tnrWavefrontLoader::tnrWavefrontLoader(const tnrWavefrontOpenFlag OpenFlags, const std::string ModelFile, const std::string MaterialFile)
	{
		this->Flags = OpenFlags;

		if (!(this->Flags & tnrWavefrontOpenFlag::DONT_LOAD_MATERIALS))
		{
			this->LoadMaterials(MaterialFile);
		}

		this->LoadObject(ModelFile);
	}

	const std::vector<tnrObject>& tnrWavefrontLoader::GetLoadedObjects() const
	{
		return this->Objects;
	}
	const std::vector<tnrMaterial>& tnrWavefrontLoader::GetLoadedMaterials() const
	{
		return this->Materials;
	}
}