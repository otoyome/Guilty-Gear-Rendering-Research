#include "texture.h"

#include "render_data.h"

#include <sstream>
#include <iomanip>

#include <filesystem>
#include "DirectXTex.h"

#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>


#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/types/unordered_map.hpp"

// CEREALバージョン定義
CEREAL_CLASS_VERSION(texture, 1)

namespace
{
	std::string_view extension = ".meta";

	bool generate_texture2d_core(texture_ptr& out_texture, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA* subresource_desc, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
	{
		HRESULT hr;
		//	テクスチャ
		Microsoft::WRL::ComPtr<ID3D11Texture2D> resource;
		hr = render_data::instance()->get_device()->CreateTexture2D(&desc, subresource_desc, &resource);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
		{
			return false;
		}

		//	シェーダーリソースビュー生成
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
		if (system_utility::is_bitflag(desc.BindFlags, D3D11_BIND_SHADER_RESOURCE))
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc{};
			if (srv_desc)
			{
				srvdesc = *srv_desc;
			}
			else
			{
				srvdesc.Format = desc.Format;
				srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srvdesc.Texture2D.MipLevels = 1;
			}
			hr = render_data::instance()->get_device()->CreateShaderResourceView(resource.Get(), &srvdesc, shader_resource_view.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			if (FAILED(hr))
			{
				return false;
			}
		}

		//	アンオーダードアクセスビュー生成
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view;
		if (system_utility::is_bitflag(desc.BindFlags, D3D11_BIND_UNORDERED_ACCESS))
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavdesc{};
			if (uav_desc)
			{
				uavdesc = *uav_desc;
			}
			else
			{
				uavdesc.Format = desc.Format;
				uavdesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
				uavdesc.Texture2D.MipSlice = 0;
			}
			hr = render_data::instance()->get_device()->CreateUnorderedAccessView(resource.Get(), &uavdesc, unordered_access_view.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			if (FAILED(hr))
			{
				return false;
			}
		}

		out_texture = std::make_shared<texture2d>(resource, shader_resource_view, unordered_access_view);
		return true;
	}

	bool load_image_file(const WCHAR* name, DirectX::TexMetadata& tex_meta_data, DirectX::ScratchImage& scratch_image)
	{
		HRESULT	hr;

		//	DDSファイル読み込み
		hr = DirectX::GetMetadataFromDDSFile(name, DirectX::DDS_FLAGS_NONE, tex_meta_data);
		if (SUCCEEDED(hr) &&
			SUCCEEDED(DirectX::LoadFromDDSFile(name, DirectX::DDS_FLAGS_NONE, &tex_meta_data, scratch_image)))
		{
			return true;
		}

		//	HDRファイル読み込み
		hr = DirectX::GetMetadataFromHDRFile(name, tex_meta_data);
		if (SUCCEEDED(hr) &&
			SUCCEEDED(DirectX::LoadFromHDRFile(name, &tex_meta_data, scratch_image)))
		{
			return true;
		}

		//	TGAファイル読み込み
		hr = DirectX::GetMetadataFromTGAFile(name, DirectX::TGA_FLAGS_NONE, tex_meta_data);
		if (SUCCEEDED(hr) &&
			SUCCEEDED(DirectX::LoadFromTGAFile(name, DirectX::TGA_FLAGS_NONE, &tex_meta_data, scratch_image)))
		{
			return true;
		}

		//	WICファイル読み込み
		hr = DirectX::GetMetadataFromWICFile(name, DirectX::WIC_FLAGS_NONE, tex_meta_data);
		if (SUCCEEDED(hr) &&
			SUCCEEDED(DirectX::LoadFromWICFile(name, DirectX::WIC_FLAGS_NONE, &tex_meta_data, scratch_image)))
		{
			return true;
		}

		return false;
	}

	bool load_image_memory(const std::byte* data_buffer, std::size_t data_size, DirectX::TexMetadata& tex_meta_data, DirectX::ScratchImage& scratch_image)
	{
		HRESULT	hr;

		//	DDSファイル読み込み
		hr	= DirectX::GetMetadataFromDDSMemory(data_buffer, data_size, DirectX::DDS_FLAGS_NONE, tex_meta_data);
		if( SUCCEEDED( hr ) &&
			SUCCEEDED( DirectX::LoadFromDDSMemory(data_buffer, data_size, DirectX::DDS_FLAGS_NONE, &tex_meta_data, scratch_image) ) )
		{
			return	true;
		}

		//	HDRファイル読み込み
		hr	= DirectX::GetMetadataFromHDRMemory(data_buffer, data_size, tex_meta_data);
		if( SUCCEEDED( hr ) &&
			SUCCEEDED( DirectX::LoadFromHDRMemory(data_buffer, data_size, &tex_meta_data, scratch_image) ) )
		{
			return	true;
		}

		//	TGAファイル読み込み
		hr	= DirectX::GetMetadataFromTGAMemory(data_buffer, data_size, DirectX::TGA_FLAGS_NONE, tex_meta_data);
		if( SUCCEEDED( hr ) &&
			SUCCEEDED( DirectX::LoadFromTGAMemory(data_buffer, data_size, DirectX::TGA_FLAGS_NONE, &tex_meta_data, scratch_image) ) )
		{
			return	true;
		}

		//	WICファイル読み込み
		hr	= DirectX::GetMetadataFromWICMemory(data_buffer, data_size, DirectX::WIC_FLAGS_NONE, tex_meta_data);
		if( SUCCEEDED( hr ) &&
			SUCCEEDED( DirectX::LoadFromWICMemory(data_buffer, data_size, DirectX::WIC_FLAGS_NONE, &tex_meta_data, scratch_image) ) )
		{
			return	true;
		}

		return	false;
	}

	bool save_image_file(const WCHAR* name, texture_manager::save_texture_desc::data_formats data_format, const DirectX::ScratchImage& scratch_image)
	{
		auto& meta_data = scratch_image.GetMetadata();
		auto images = scratch_image.GetImages();
		auto image_count = scratch_image.GetImageCount();

		HRESULT hr = S_OK;
		std::filesystem::path path = name;
		switch (data_format)
		{
			case texture_manager::save_texture_desc::data_formats::dds:
			{
				path = path.replace_extension(".dds");
				hr = DirectX::SaveToDDSFile(images, image_count, meta_data, DirectX::DDS_FLAGS_NONE, path.wstring().data());
				break;
			}
			case texture_manager::save_texture_desc::data_formats::jpg:
			{
				path = path.replace_extension(".jpg");
				hr = DirectX::SaveToWICFile(images, image_count, DirectX::WIC_FLAGS_NONE,
					DirectX::GetWICCodec(DirectX::WICCodecs::WIC_CODEC_JPEG), path.wstring().data());
				break;
			}
			case texture_manager::save_texture_desc::data_formats::png:
			{
				path = path.replace_extension(".png");
				hr = DirectX::SaveToWICFile(images, image_count, DirectX::WIC_FLAGS_NONE,
					DirectX::GetWICCodec(DirectX::WICCodecs::WIC_CODEC_PNG), path.wstring().data());
				break;
			}
			case texture_manager::save_texture_desc::data_formats::tga:
			{
				path = path.replace_extension(".tga");
				hr = DirectX::SaveToTGAFile(images[0], path.wstring().data());
				break;
			}
			case texture_manager::save_texture_desc::data_formats::hdr:
			{
				path = path.replace_extension(".hdr");
				hr = DirectX::SaveToHDRFile(images[0], path.wstring().data());
				break;
			}
		}
		return SUCCEEDED(hr);
	}

	bool save_image_memory(const WCHAR* name, texture_manager::save_texture_desc::data_formats data_format, const DirectX::ScratchImage& scratch_image, DirectX::Blob& out_blob)
	{
		auto& meta_data = scratch_image.GetMetadata();
		auto images = scratch_image.GetImages();
		auto image_count = scratch_image.GetImageCount();
		switch (data_format)
		{
			case texture_manager::save_texture_desc::data_formats::dds:
			{
				if (FAILED(DirectX::SaveToDDSMemory(images, image_count, meta_data, DirectX::DDS_FLAGS_NONE, out_blob)))
				{
					return false;
				}
				break;
			}
			case texture_manager::save_texture_desc::data_formats::jpg:
			{
				if (FAILED(DirectX::SaveToWICMemory(images, image_count, DirectX::WIC_FLAGS_NONE,
					DirectX::GetWICCodec(DirectX::WICCodecs::WIC_CODEC_JPEG), out_blob)))
				{
					return false;
				}
				break;
			}
			case texture_manager::save_texture_desc::data_formats::png:
			{
				if (FAILED(DirectX::SaveToWICMemory(images, image_count, DirectX::WIC_FLAGS_NONE,
					DirectX::GetWICCodec(DirectX::WICCodecs::WIC_CODEC_PNG), out_blob)))
				{
					return false;
				}
				break;
			}
			case texture_manager::save_texture_desc::data_formats::tga:
			{
				if (FAILED(DirectX::SaveToTGAMemory(images[0], out_blob)))
				{
					return false;
				}
				break;
			}
			case texture_manager::save_texture_desc::data_formats::hdr:
			{
				if (FAILED(DirectX::SaveToHDRMemory(images[0], out_blob)))
				{
					return false;
				}
				break;
			}
		}
		return true;
	}
}

bool texture_meta_data::serialize(std::string_view file_path)
{
	std::ofstream ostream(file_path.data(), std::ios::binary);
	if (ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(CEREAL_NVP(type), CEREAL_NVP(system_flags));
		}
		catch (...)
		{

			DEBUG_LOG(file_path.data());
			assert(!"texture serialize failed.\n");
			return false;
		}

		return true;
	}
	return false;
}

bool texture_meta_data::deserialize(std::string_view file_path)
{
	std::ifstream istream(file_path.data(), std::ios::binary);
	if (istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(CEREAL_NVP(type), CEREAL_NVP(system_flags));
		}
		catch (...)
		{
			DEBUG_ERROR("texture_meta_data deserialize failed[%s]", file_path.data());
			return false;
		}

		DEBUG_LOG("texture_meta_data deserialize succeeded[%s]", file_path.data());
		return true;
	}
	DEBUG_ERROR("texture_meta_data open failed[%s]", file_path.data());
	return false;
}

//	毎回定義するのが面倒なのでマクロにしておく
#define	RETURN_SEARCH_FILE(KEY_NAME)			\
auto	key_name	= std::string(KEY_NAME);	\
auto	it	= textures.find(key_name);			\
if (it != textures.end()){						\
	auto ptr = it->second.lock();				\
	if(ptr){									\
		out_texture = ptr;						\
		return	true;							\
	}											\
	textures.erase(it);							\
}

D3D11_RESOURCE_DIMENSION texture::get_dimension() const
{
	return dimension;
}

const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& texture::get_shader_resource_view() const
{
	return shader_resource_view;
}

const Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>& texture::get_unordered_access_view() const
{
	return unordered_access_view;
}

texture_data_types texture::get_type() const
{
	return type;
}

void texture::set_type(texture_data_types type)
{
	this->type = type;
}

bool texture::is_file_path_or_key() const
{
	return this->is_file;
}

std::string_view texture::get_file_path_or_key() const
{
	return	this->file_path_or_key;
}

void texture::set_file_path(std::string_view file_path)
{
	this->is_file = true;
	this->file_path_or_key = file_path;
}

void texture::set_data_key(std::string_view key)
{
	this->is_file = false;
	this->file_path_or_key = key;
}

texture::texture(D3D11_RESOURCE_DIMENSION dimension, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view)
: is_file(false)
, file_path_or_key()
, type(texture_data_types::color)
, system_flags(0)
, dimension(dimension)
, shader_resource_view(shader_resource_view)
, unordered_access_view(unordered_access_view)
{
	set_flag(flags::srgb, false);
}

#if defined(USE_IMGUI)
bool texture::is_reload()
{
	if (!is_file)
		return false;

	bool flag = false;

	//	ファイルが存在しない場合
	if (!std::filesystem::exists(file_path_or_key))
	{
		return false;
	}

	//	最終更新日時と違った場合は再コンパイルが必要か判定
	file_time_type write_time = std::filesystem::last_write_time(file_path_or_key);
	if (last_write_time != write_time)
	{
		return true;
	}

	return false;
}

void texture::reload()
{
	texture_ptr dummy_texture;
	if (texture_manager::instance()->load_texture_from_file(dummy_texture, file_path_or_key, true))
	{
		//	シェーダーリソースビューを入れ替え
		this->shader_resource_view = dummy_texture->shader_resource_view;
		on_reload();
		this->last_write_time = std::filesystem::last_write_time(file_path_or_key);
	}
}
#endif	//	defined(USE_IMGUI)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
texture_manager::texture_manager()
{
	//	ダミーテクスチャ生成
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = 16;
		desc.Height = 16;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		generate_texture2d(white_texture2d, white_texture2d_name, desc, 0xFFFFFFFF);
		generate_texture2d(black_texture2d, black_texture2d_name, desc, 0xFF000000);
	}
}

texture_manager::~texture_manager()
{
}

void texture_manager::check_update_and_reload()
{
#if defined(USE_IMGUI)
	//	テキトーに待機
	if (++check_counter < 60)
		return;
	check_counter = 0;

	//	ファイル日時更新チェック
	auto it = textures.begin();
	while (it != textures.end())
	{
		auto ptr = it->second.lock();
		if (!ptr)
		{
			it = textures.erase(it);
			continue;
		}

		if (ptr->is_reload())
		{
			ptr->reload();
		}
		++it;
	}
#endif	//	defined(USE_IMGUI)
}

bool texture_manager::generate_texture2d(texture_ptr& out_texture, std::string_view key, D3D11_TEXTURE2D_DESC desc, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
{
	//	存在すればそちらを渡す
	RETURN_SEARCH_FILE(key)

	if(generate_texture2d_core(out_texture, desc, nullptr, srv_desc, uav_desc))
	{
		out_texture->set_data_key(key);
		textures.insert(make_pair(key_name, out_texture));
		return true;
	}
	out_texture	= nullptr;
	return false;
}

bool texture_manager::generate_texture2d(texture_ptr& out_texture, std::string_view key, D3D11_TEXTURE2D_DESC desc, DWORD init_value/*0xAABBGGRR*/, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
{
	//	存在すればそちらを渡す
	RETURN_SEARCH_FILE(key)

	std::unique_ptr<DWORD[]> sysmem;
	size_t texels = desc.Width * desc.Height;
	sysmem = std::make_unique<DWORD[]>(texels);
	for (size_t i = 0; i < texels; ++i)
	{
		sysmem[i] = init_value;
	}

	D3D11_SUBRESOURCE_DATA subresource_data{};
	subresource_data.pSysMem = sysmem.get();
	subresource_data.SysMemPitch = sizeof(DWORD) * desc.Width;
	subresource_data.SysMemSlicePitch = 0;

	if(generate_texture2d_core(out_texture, desc, &subresource_data, srv_desc, uav_desc))
	{
		out_texture->set_data_key(key);
		textures.insert(make_pair(key_name, out_texture));
		return true;
	}
	out_texture	= nullptr;
	return false;
}

bool texture_manager::generate_texture2d(texture_ptr& out_texture, std::string_view key, D3D11_TEXTURE2D_DESC desc, D3D11_SUBRESOURCE_DATA subresource_data, D3D11_SHADER_RESOURCE_VIEW_DESC* srv_desc, D3D11_UNORDERED_ACCESS_VIEW_DESC* uav_desc)
{
	//	存在すればそちらを渡す
	RETURN_SEARCH_FILE(key)

	if (generate_texture2d_core(out_texture, desc, &subresource_data, srv_desc, uav_desc))
	{
		out_texture->set_data_key(key);
		textures.insert(make_pair(key_name, out_texture));
		return true;
	}
	out_texture = nullptr;
	return false;
}

bool texture_manager::load_texture_from_file(texture_ptr& out_texture, std::string_view file_path, bool force_load)
{
	//	存在すればそちらを渡す
	if (!force_load)
	{
		RETURN_SEARCH_FILE(file_path)
	}
	auto key_name = std::string(file_path);

	HRESULT hr;
	std::filesystem::path path(file_path);
	DirectX::TexMetadata tex_meta_data;
	DirectX::ScratchImage scratch_image;
	if (!load_image_file(path.wstring().data(), tex_meta_data, scratch_image))
	{
		return false;
	}

	//	ミップマップ生成
//	if (generate_mipmap)
	if (false)
	{
		DirectX::ScratchImage	mip_chain;
		hr = DirectX::GenerateMipMaps(scratch_image.GetImages(),
									  scratch_image.GetImageCount(),
									  scratch_image.GetMetadata(),
									  DirectX::TEX_FILTER_DEFAULT,
									  0,	//	ミップマップをすべて作る
									  mip_chain);
		if (SUCCEEDED(hr))
		{
			scratch_image = std::move(mip_chain);
		}
	}

	//	シェーダーリソースビューの生成
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	hr	= DirectX::CreateShaderResourceView(render_data::instance()->get_device().Get(),
											scratch_image.GetImages(),
											scratch_image.GetImageCount(),
											scratch_image.GetMetadata(),
											shader_resource_view.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if(SUCCEEDED(hr))
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		shader_resource_view->GetResource(resource.GetAddressOf());

		D3D11_RESOURCE_DIMENSION dimension;
		resource->GetType(&dimension);
		switch(dimension)
		{
			case	D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			{
				Microsoft::WRL::ComPtr<ID3D11Texture2D> resource_data;
				hr	= resource.As<ID3D11Texture2D>(&resource_data);
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
				if(SUCCEEDED(hr))
				{
					out_texture	= std::make_shared<texture2d>(resource_data, shader_resource_view);
					out_texture->set_file_path(file_path);
				}
				break;
			}
			//	とりあえず未対応
			case	D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			case	D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			default:
			{
				hr = E_FAIL;
				assert(!"[texture]unknown type.");
			}
		}
	}

	if (FAILED(hr))
	{
		out_texture = nullptr;
		return false;
	}

	//	メタデータがあれば読み込み
	{
		path.replace_extension(extension);
		if (std::filesystem::exists(path))
		{
			texture_meta_data meta_data;
			if (meta_data.deserialize(path.string().data()))
			{
				out_texture->set_type(meta_data.type);
				out_texture->set_system_flags(meta_data.system_flags);
			}
		}
	}
	//	テクスチャ保存
	textures.insert(make_pair(key_name, out_texture));
	return	true;
}

bool texture_manager::load_texture_from_memory(texture_ptr& out_texture, std::string_view key, const void* data, size_t size)
{
	//	存在すればそちらを渡す
	RETURN_SEARCH_FILE(key)

	HRESULT hr;
	DirectX::TexMetadata tex_meta_data;
	DirectX::ScratchImage scratch_image;
	if (!load_image_memory(reinterpret_cast<const std::byte*>(data), size, tex_meta_data, scratch_image))
	{
		return false;
	}

	//	ミップマップ生成
//	if (generate_mipmap)
	if (false)
	{
		DirectX::ScratchImage	mip_chain;
		hr = DirectX::GenerateMipMaps(scratch_image.GetImages(),
									  scratch_image.GetImageCount(),
									  scratch_image.GetMetadata(),
									  DirectX::TEX_FILTER_DEFAULT,
									  0,	//	ミップマップをすべて作る
									  mip_chain);
		if (SUCCEEDED(hr))
		{
			scratch_image = std::move(mip_chain);
		}
	}

	//	シェーダーリソースビューの生成
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	hr	= DirectX::CreateShaderResourceView(render_data::instance()->get_device().Get(),
											scratch_image.GetImages(),
											scratch_image.GetImageCount(),
											scratch_image.GetMetadata(),
											shader_resource_view.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if(SUCCEEDED(hr))
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		shader_resource_view->GetResource(resource.GetAddressOf());

		D3D11_RESOURCE_DIMENSION dimension;
		resource->GetType(&dimension);
		switch(dimension)
		{
			case	D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			{
				Microsoft::WRL::ComPtr<ID3D11Texture2D> resource_data;
				hr	= resource.As<ID3D11Texture2D>(&resource_data);
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
				if(SUCCEEDED(hr))
				{
					out_texture	= std::make_shared<texture2d>(resource_data, shader_resource_view);
					textures.insert(make_pair(key_name, out_texture));
					return	true;
				}
				break;
			}
			//	とりあえず未対応
			case	D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			case	D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			default:
			{
				assert(!"[texture]unknown type.");
			}
		}
	}
	out_texture	= nullptr;
	return false;
}

texture_ptr texture_manager::search(std::string_view name)
{
	auto key_name = std::string(name);
	auto it = textures.find(key_name);
	if (it != textures.end())
	{
		auto ptr = it->second.lock();
		if (!ptr)
		{
			textures.erase(it);
			return	nullptr;
		}
		return	ptr;
	}

	return	nullptr;
}

texture_manager::texture_map texture_manager::get_textures() const
{
	return textures;
}

bool texture_manager::save_texture(const save_texture_desc& desc)
{
	std::filesystem::path texture_filepath(desc.file_path);

	std::vector<DirectX::Image> images;
	for (auto& desc_image : desc.images)
	{
		auto& image = images.emplace_back();
		image.width = desc_image.width;
		image.height = desc_image.height;
		image.pixels = const_cast<uint8_t*>(desc_image.pixels);
		image.format = desc.format;
		image.rowPitch = graphics_utility::get_byte_offset_from_format(image.format) * image.width;
		image.slicePitch = image.rowPitch * image.height;
	}

	DirectX::ScratchImage scratch_image;
	if (desc.misc == save_texture_desc::miscs::_cube)
	{
		scratch_image.InitializeCubeFromImages(images.data(), images.size());
	}
	else
	{
		switch (desc.dimension)
		{
			case save_texture_desc::dimensions::_1d:
			{
				scratch_image.InitializeArrayFromImages(images.data(), images.size(), true);
				break;
			}
			case save_texture_desc::dimensions::_2d:
			{
				scratch_image.InitializeArrayFromImages(images.data(), images.size());
				break;
			}
			case save_texture_desc::dimensions::_3d:
			{
				scratch_image.Initialize3DFromImages(images.data(), images.size());
				break;
			}
		}
	}
	return save_image_file(texture_filepath.wstring().data(), desc.data_format, scratch_image);
}

bool texture_manager::save_texture(const texture_ptr& texture, bool save_row_texture)
{
	std::filesystem::path file_path = texture->get_file_path_or_key();

	bool save_succeeded = true;
	if (save_row_texture)
	{
		auto device = render_data::instance()->get_device();
		auto immediate_context = render_data::instance()->get_immediate_context();

		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		texture->get_shader_resource_view()->GetResource(resource.GetAddressOf());

		DirectX::ScratchImage image;
		if (FAILED(DirectX::CaptureTexture(device.Get(), immediate_context.Get(), resource.Get(), image)))
			return false;

		//	テクスチャ保存
		save_succeeded = save_image_file(file_path.wstring().data(), texture_manager::save_texture_desc::data_formats::dds, image);
	}

	if (save_succeeded)
	{
		texture_meta_data meta_data;
		meta_data.type = texture->get_type();
		meta_data.system_flags = texture->get_system_flags();

		file_path.replace_extension(extension);
		return meta_data.serialize(file_path.string());
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
texture2d::texture2d(Microsoft::WRL::ComPtr<ID3D11Texture2D> texture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view)
: texture(D3D11_RESOURCE_DIMENSION_TEXTURE2D, shader_resource_view, unordered_access_view)
{
	texture->GetDesc(&desc);

	//	初期設定
	set_flag(flags::cube_map, desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE);
}

texture2d::~texture2d()
{
}

#if defined(USE_IMGUI)
void texture2d::on_reload()
{
	//	Desc更新
	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	shader_resource_view->GetResource(resource.GetAddressOf());
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	resource.As(&texture);

	texture->GetDesc(&desc);
}
#endif	//	defined(USE_IMGUI)

#undef	RETURN_SEARCH_FILE

