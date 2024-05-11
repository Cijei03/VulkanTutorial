#include "wavefront_loader.hpp"
#include <fstream>
#include <cassert>
#include <sstream>

#include <iostream>

static std::vector<std::string> SequenceLine(const std::string_view XStr)
{
	std::vector<std::string> SequencedLine;

	size_t LastI = 0;
	for (size_t i = 0; i < XStr.length(); i++)
	{
		if (XStr[i] == ' ' || i == XStr.length() - 1)
		{
			auto x = std::string(XStr.substr(LastI, (i + 1) - LastI));
			std::erase(x, ' ');
			SequencedLine.push_back(x);
			LastI = i;
		}
	}

	return SequencedLine;
}

static std::vector<std::string> SequenceFaceLine(const std::string_view XStr)
{
	std::vector<std::string> SequencedLine;

	size_t LastI = 0;
	for (size_t i = 0; i < XStr.length(); i++)
	{
		if (XStr[i] == '/' || i == XStr.length() - 1)
		{
			auto x = std::string(XStr.substr(LastI, (i + 1) - LastI));
			std::erase(x, '/');
			std::erase(x, ' ');
			SequencedLine.push_back(x);
			LastI = i;
		}
	}

	return SequencedLine;
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

			auto SequencedLine = SequenceLine(Cache);

			if (SequencedLine[0] == "newmtl")
			{
				if (MaterialCache.MaterialName.length() > 0)
				{
					this->Materials.push_back(MaterialCache);
				}
				MaterialCache = tnrMaterial
				{
					.MaterialName = SequencedLine[1],
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
			else if (SequencedLine[0] == "Ka")
			{
				MaterialCache.AmbientColor =
				{
					std::stof(SequencedLine[1]),
					std::stof(SequencedLine[2]),
					std::stof(SequencedLine[3])
				};
			}
			else if (SequencedLine[0] == "Kd")
			{
				MaterialCache.AlbedoColor =
				{
					std::stof(SequencedLine[1]),
					std::stof(SequencedLine[2]),
					std::stof(SequencedLine[3])
				};
			}
			else if (SequencedLine[0] == "Ks")
			{
				MaterialCache.SpecularColor =
				{
					std::stof(SequencedLine[1]),
					std::stof(SequencedLine[2]),
					std::stof(SequencedLine[3])
				};
			}
			else if (SequencedLine[0] == "Ns")
			{
				MaterialCache.SpecularFactor = std::stof(SequencedLine[1]);
			}
			else if (SequencedLine[0] == "Ni")
			{
				MaterialCache.RefractionFactor = std::stof(SequencedLine[1]);
			}
			else if (SequencedLine[0] == "map_Kd")
			{
				MaterialCache.AlbedoMap = std::optional<tnrMaterial::tnrTextureInfo>(tnrMaterial::tnrTextureInfo{ SequencedLine[1] });
			}
			else if (SequencedLine[0] == "map_Ks")
			{
				MaterialCache.SpecularMap = std::optional<tnrMaterial::tnrTextureInfo>(tnrMaterial::tnrTextureInfo{ SequencedLine[1] });
			}
			else if (SequencedLine[0] == "map_bump" || SequencedLine[0] == "bump")
			{
				MaterialCache.NormalMap = std::optional<tnrMaterial::tnrTextureInfo>(tnrMaterial::tnrTextureInfo{ SequencedLine[1] });
			}
		}

		if (MaterialCache.MaterialName.length() > 0)
		{
			this->Materials.push_back(MaterialCache);
		}
	}
	void tnrWavefrontLoader::LoadObject(const std::string& ObjFile)
	{
		const bool ShouldFlipY = this->Flags & tnrWavefrontOpenFlag::FLIP_POSITION_Y_AXIS;

		std::ifstream File(ObjFile);

		std::string Cache;
		tnrObject ObjectCache;

		std::vector<tnrObject::vec3<float>> Positions;
		std::vector<tnrObject::vec3<float>> Normals;
		std::vector<tnrObject::vec2<float>> TextureCoords;

		while (std::getline(File, Cache, '\n'))
		{
			if (Cache.length() < 1) continue;
			if (Cache[0] == '#') continue;

			auto SequencedLine = SequenceLine(Cache);

			if (SequencedLine[0] == "o")
			{
				if (ObjectCache.ObjectName.length() > 0)
				{
					this->Objects.push_back(ObjectCache);
				}

				ObjectCache = tnrObject
				{
					.ObjectName = SequencedLine[1],
					.MaterialName = "",
					.Positions = std::vector<tnrObject::vec3<float>>(),
					.Normals = std::vector<tnrObject::vec3<float>>()
				};

				//Positions.clear();
				//Normals.clear();
				//TextureCoords.clear();
			}
			else if (SequencedLine[0] == "v")
			{
				Positions.push_back
				(
					tnrObject::vec3<float>
					{
						.x = std::stof(SequencedLine[1]),
						.y = (ShouldFlipY ? std::stof(SequencedLine[2]) * -1.0f : std::stof(SequencedLine[2])),
						.z = std::stof(SequencedLine[3])
					}
				);
			}
			else if (SequencedLine[0] == "vn")
			{
				Normals.push_back
				(
					tnrObject::vec3<float>
					{
						.x = std::stof(SequencedLine[1]),
						.y = std::stof(SequencedLine[2]),
						.z = std::stof(SequencedLine[3])
					}
				);
			}
			else if (SequencedLine[0] == "vt")
			{
				TextureCoords.push_back
				(
					tnrObject::vec2<float>
					{
						.x = std::stof(SequencedLine[1]),
						.y = std::stof(SequencedLine[2])
					}
				);
			}
			else if (SequencedLine[0] == "usemtl")
			{
				ObjectCache.MaterialName = SequencedLine[1];
			}
			else if (SequencedLine[0] == "f")
			{				
				auto AddVertexData = 
				[
					&ObjectCache,
					&Positions,
					&Normals,
					&TextureCoords
				](const std::string_view Face) -> void
				{
					const auto SequencedFace = SequenceFaceLine(Face);

					size_t PositionIndex = std::stoul(SequencedFace[0]) - 1;
					size_t TextureCoordIndex = std::stoul(SequencedFace[1]) - 1;
					size_t NormalIndex = std::stoul(SequencedFace[2]) - 1;

					ObjectCache.Positions.push_back(Positions[PositionIndex]);
					ObjectCache.TextureCoords.push_back(TextureCoords[TextureCoordIndex]);
					ObjectCache.Normals.push_back(Normals[NormalIndex]);
				};

				AddVertexData(SequencedLine[1]);
				AddVertexData(SequencedLine[2]);
				AddVertexData(SequencedLine[3]);
			}
		}
		if (ObjectCache.ObjectName.length() > 0)
		{
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