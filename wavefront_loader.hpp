#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include <string>

#ifndef TUTORIAL_VK_BITMASK
	#define TUTORIAL_VK_BITMASK(XD) 1ul << XD
#endif

namespace tnr::m3d::wavefront
{
	enum tnrWavefrontOpenFlag : uint32_t
	{
		NO_FLAGS = 0,
		FLIP_POSITION_Y_AXIS = TUTORIAL_VK_BITMASK(0),		// Flips all position vectors within Y axis.
		DONT_LOAD_MATERIALS = TUTORIAL_VK_BITMASK(2),		// Load mesh without materials. If not present, material MUST be loaded before loading model file.
	};

	struct tnrMaterial
	{
		std::string MaterialName;

		// Color properties.
		struct
		{
			float x;
			float y;
			float z;
		} AmbientColor;
		struct
		{
			float x;
			float y;
			float z;
		} AlbedoColor;
		struct
		{
			float x;
			float y;
			float z;
		} SpecularColor;
		struct
		{
			float x;
			float y;
			float z;
		} EmissionColor;

		// Light properties.
		float SpecularFactor;
		float RefractionFactor;
		float AlphaFactor; // Currently this property is not readed and is omitted.

		struct tnrTextureInfo
		{
			std::string Name;
		};

		// Textures properties.
		std::optional<tnrTextureInfo> AlbedoMap;
		std::optional<tnrTextureInfo> NormalMap;
		std::optional<tnrTextureInfo> SpecularMap;
	};

	struct tnrObject
	{
		template<typename T>
		struct vec3
		{
			T x;
			T y;
			T z;
		};
		template<typename T>
		struct vec2
		{
			T x;
			T y;
		};

		std::string ObjectName;
		std::string MaterialName;

		std::vector<vec3<float>> Positions;
		std::vector<vec3<float>> Normals;
		std::vector<vec2<float>> TextureCoords;
	};

	// Vertices color unsupported. OBJ must be exported with Z as forward axis and -Y as up axis for compatibility with Vulkan.
	class tnrWavefrontLoader
	{
	private:

		struct ObjectInfo
		{
			std::optional<uint32_t> MaterialIndex;
			tnrObject Object;
		};

		std::vector<tnrObject> Objects;
		std::vector<tnrMaterial> Materials;

		tnrWavefrontOpenFlag Flags;

		void LoadMaterials(const std::string& MTLFile);
		void LoadObject(const std::string& ObjFile);
	public:
		tnrWavefrontLoader(const tnrWavefrontOpenFlag OpenFlags = tnrWavefrontOpenFlag::DONT_LOAD_MATERIALS, const std::string ModelFile = "", const std::string MaterialFile = "");
		~tnrWavefrontLoader() = default;

		const std::vector<tnrObject>& GetLoadedObjects() const;
		const std::vector<tnrMaterial>& GetLoadedMaterials() const;
	};
}