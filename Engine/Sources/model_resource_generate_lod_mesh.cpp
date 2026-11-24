#include <cmath>

#include "model_resource.h"
#include "graphics_utility.h"
#include "render_data.h"
#include "texture.h"

#include "meshoptimizer/meshoptimizer.h"

void model_resource::optimized_meshs()
{
	int mesh_index = 0;
	for (auto& mesh : meshes)
	{
		if (mesh.index_buffer_view.count() == 0)
			continue;

		//	インデックスリストを生成
		std::vector<UINT> indices;
		indices.reserve(mesh.index_buffer_view.count());
		for (UINT index = 0; index < mesh.index_buffer_view.count(); ++index)
		{
			if (mesh.index_buffer_view.stride_in_bytes == 2)
			{
				indices.push_back(reinterpret_cast<USHORT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else if (mesh.index_buffer_view.stride_in_bytes == 1)
			{
				indices.push_back(reinterpret_cast<UCHAR*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else
			{
				indices.push_back(reinterpret_cast<UINT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
		}

		{
			//	stream生成
			std::vector<meshopt_Stream> streams;
			std::size_t vertex_count = 0, position_index = 0;
			{
				bool position_find = false;
				for (auto& vbv : mesh.vertex_buffer_views)
				{
					auto& stream = streams.emplace_back();
					stream.data = vbv.second.row_data.data();
					stream.size = vbv.second.stride_in_bytes;
					stream.stride = vbv.second.stride_in_bytes;
					if (vbv.second.count() > vertex_count)
					{
						vertex_count = vbv.second.count();
					}
					if (!position_find)
					{
						if (vbv.first == semantic("POSITION", 0))
						{
							position_find = true;
						}
						else
						{
							++position_index;
						}
					}
				}
			}

			if (vertex_count == 0)
				continue;

			//	重複頂点最適化
			std::vector<UINT> remap(vertex_count);
			size_t new_vertex_count = meshopt_generateVertexRemapMulti(
				remap.data(), indices.data(), indices.size(), vertex_count,
				streams.data(), streams.size());

			//	インデックスバッファ最適化
			meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());

			//	頂点バッファ最適化
			std::vector<std::vector<UCHAR>> vertex_buffer;
			vertex_buffer.reserve(streams.size());
			for (auto& stream : streams)
			{
				auto& new_data = vertex_buffer.emplace_back(new_vertex_count * stream.stride);
				meshopt_remapVertexBuffer(new_data.data(),
						stream.data, vertex_count, stream.stride, remap.data());
			}

			{
				//	モーフとかあればここでついでに
			}

			for (UINT index = 0; index < streams.size(); ++index)
			{
				auto& stream = streams[index];
				auto& vertices = vertex_buffer[index];
				meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(),
											vertices.data(), new_vertex_count, stream.stride);
			}

			//	頂点バッファ
			meshopt_optimizeVertexFetchRemap(remap.data(), indices.data(), indices.size(), new_vertex_count);
			for (UINT index = 0; index < streams.size(); ++index)
			{
				auto& stream = streams[index];
				auto& vertices = vertex_buffer[index];
				meshopt_remapVertexBuffer(vertices.data(), vertices.data(), new_vertex_count,
										  stream.stride, remap.data());
			}

			//	頂点キャッシュ最適化
			meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), new_vertex_count);

			//	オーバードロー最適化
		//	meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(),
		//							 reinterpret_cast<float*>(vertex_buffer[position_index].data()),
		//							 vertex_count, streams[position_index].stride, 1.05f);

			//	インデックスバッファ書き戻し
			{
				mesh.index_buffer_view.row_data.resize(indices.size() * sizeof(UINT));
				memcpy_s(mesh.index_buffer_view.row_data.data(), mesh.index_buffer_view.row_data.size(),
						 indices.data(), indices.size() * sizeof(UINT));
				mesh.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
				mesh.index_buffer_view.size_in_bytes = indices.size() * sizeof(UINT);
				mesh.index_buffer_view.stride_in_bytes = sizeof(UINT);
			}

			//	頂点バッファ書き戻し
			{
				UINT index = 0;
				for (auto& vbv : mesh.vertex_buffer_views)
				{
					auto& stream = streams[index];
					auto& vertices = vertex_buffer[index];
					vbv.second.row_data.resize(vertices.size());
					memcpy_s(vbv.second.row_data.data(), vbv.second.row_data.size(),
							 vertices.data(), vertices.size());
					vbv.second.size_in_bytes = vertices.size();
					++index;
				}
			}
		}
	}
}

void model_resource::generate_shadow_meshes()
{
	for (auto& mesh : meshes)
	{
		if (mesh.index_buffer_view.count() == 0)
			continue;

		//	インデックスリストを生成
		std::vector<UINT> indices;
		indices.reserve(mesh.index_buffer_view.count());
		for (UINT index = 0; index < mesh.index_buffer_view.count(); ++index)
		{
			if (mesh.index_buffer_view.stride_in_bytes == 2)
			{
				indices.push_back(reinterpret_cast<USHORT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else if (mesh.index_buffer_view.stride_in_bytes == 1)
			{
				indices.push_back(reinterpret_cast<UCHAR*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else
			{
				indices.push_back(reinterpret_cast<UINT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
		}

		{
			//	stream生成
			std::vector<meshopt_Stream> streams;
			std::size_t vertex_count = 0, position_index = 0;
			{
				bool position_find = false;
				for (auto& vbv : mesh.vertex_buffer_views)
				{
					auto& stream = streams.emplace_back();
					stream.data = vbv.second.row_data.data();
					stream.size = vbv.second.stride_in_bytes;
					stream.stride = vbv.second.stride_in_bytes;
					if (vbv.second.count() > vertex_count)
					{
						vertex_count = vbv.second.count();
					}
					if (!position_find)
					{
						if (vbv.first == semantic("POSITION", 0))
						{
							position_find = true;
						}
						else
						{
							++position_index;
						}
					}
				}
			}

			if (vertex_count == 0)
				continue;

			//	重複頂点最適化
			meshopt_generateShadowIndexBufferMulti(indices.data(), indices.data(), indices.size(), vertex_count, streams.data(), streams.size());

			//	影のインデックスバッファ書き戻し
			{
				mesh.shadow_mesh.index_buffer_view.row_data.resize(indices.size() * sizeof(UINT));
				memcpy_s(mesh.shadow_mesh.index_buffer_view.row_data.data(),
						 mesh.shadow_mesh.index_buffer_view.row_data.size(),
						 indices.data(), indices.size() * sizeof(UINT));
				mesh.shadow_mesh.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
				mesh.shadow_mesh.index_buffer_view.size_in_bytes = indices.size() * sizeof(UINT);
				mesh.shadow_mesh.index_buffer_view.stride_in_bytes = sizeof(UINT);
			}
		}
	}
}

void model_resource::generate_lod_meshs(int max_lod_level, bool regeneratge_buffer)
{
	for (auto& mesh : meshes)
	{
		if (mesh.index_buffer_view.count() == 0)
			continue;

		//	インデックスリストを生成
		std::vector<UINT> indices;
		indices.reserve(mesh.index_buffer_view.count());
		for (UINT index = 0; index < mesh.index_buffer_view.count(); ++index)
		{
			if (mesh.index_buffer_view.stride_in_bytes == 2)
			{
				indices.push_back(reinterpret_cast<USHORT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else if (mesh.index_buffer_view.stride_in_bytes == 1)
			{
				indices.push_back(reinterpret_cast<UCHAR*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else
			{
				indices.push_back(reinterpret_cast<UINT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
		}

		{
			//	stream生成
			std::vector<meshopt_Stream> streams;
			std::size_t vertex_count = 0, position_index = 0;
			{
				bool position_find = false;
				for (auto& vbv : mesh.vertex_buffer_views)
				{
					auto& stream = streams.emplace_back();
					stream.data = vbv.second.row_data.data();
					stream.size = vbv.second.stride_in_bytes;
					stream.stride = vbv.second.stride_in_bytes;
					if (vbv.second.count() > vertex_count)
					{
						vertex_count = vbv.second.count();
					}
					if (!position_find)
					{
						if (vbv.first == semantic("POSITION", 0))
						{
							position_find = true;
						}
						else
						{
							++position_index;
						}
					}
				}
			}

			if (vertex_count == 0)
				continue;

			//	重複頂点最適化
			for (INT lod = 0; lod < max_lod_level; ++lod)
			{
				std::vector<UINT> lod_indicesLod(indices.size());
				UINT target_indices_count = static_cast<UINT>(indices.size() / pow(2, lod + 1));
				float target_error = static_cast<float>(lod) / max_lod_level;
				lod_indicesLod.resize(meshopt_simplify(lod_indicesLod.data(),
									 indices.data(),
									 indices.size(),
									 reinterpret_cast<const float*>(streams[position_index].data),
									 vertex_count,
									 streams[position_index].stride,
									 target_indices_count,
									 target_error));

				mesh::sub_mesh sub_mesh;
				sub_mesh.index_buffer_view.row_data.resize(lod_indicesLod.size() * sizeof(UINT));
				memcpy_s(sub_mesh.index_buffer_view.row_data.data(),
						 sub_mesh.index_buffer_view.row_data.size(),
						 lod_indicesLod.data(), lod_indicesLod.size() * sizeof(UINT));
				sub_mesh.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
				sub_mesh.index_buffer_view.size_in_bytes = lod_indicesLod.size() * sizeof(UINT);
				sub_mesh.index_buffer_view.stride_in_bytes = sizeof(UINT);
				mesh.lod_meshs.emplace_back(sub_mesh);
			}

			//	影のインデックスバッファ書き戻し
			{
			}
		}
	}
}