#pragma once

#include "common_pipeline_states.h"
#include "material.h"

//	描画用カスタムパイプラインステート
class graphics_custom_pipeline_state : public graphics_pipeline_state
{
	template<typename Shader> const std::shared_ptr<Shader>& get_internal_shader() const {return std::shared_ptr<Shader>();}
	template<> const std::shared_ptr<vertex_shader>& get_internal_shader<vertex_shader>() const {return vs;}
	template<> const std::shared_ptr<hull_shader>& get_internal_shader<hull_shader>() const {return hs;}
	template<> const std::shared_ptr<domain_shader>& get_internal_shader<domain_shader>() const {return ds;}
	template<> const std::shared_ptr<geometry_shader>& get_internal_shader<geometry_shader>() const {return gs;}
	template<> const std::shared_ptr<pixel_shader>& get_internal_shader<pixel_shader>() const {return ps;}

	enum class depth_stencil_state
	{
		depth_test_and_write,
		depth_test_only,
		depth_write_only,
		depth_off,

		max,
	};

	enum class blend_state
	{
		opaque,			//	不透明
		translucent,	//	半透明
		addative,		//	加算
		multiply,		//	乗算

		max,
	};

	enum class raster_state
	{
		solid,
		two_sided,
		wire,
		wire_two_sided,

		max,
	};

	enum class arrangement
	{
		vertex,
		hull,
		domain,
		geometry,
		pixel,

		max,
	};

	struct index_map
	{
		struct indices
		{
			UINT ids[static_cast<int>(arrangement::max)];
			indices()
			{
				for (auto& index : ids)
				{
					index = -1;
				}
			}
		};
		std::vector<std::pair<std::string, indices>> cbv;
		std::vector<std::pair<std::string, indices>> srv;
		std::vector<std::pair<std::string, indices>> sampler;
		void clear()
		{
			cbv.clear();
			srv.clear();
		}
	};

public:
	graphics_custom_pipeline_state();
	~graphics_custom_pipeline_state() override;

	void begin(const graphics_context& context) override;
	void update_parameters(const graphics_context& context, const material& material, const buffer_datas* buffer_data) override;
	void end(const graphics_context& context) override;

	void generate_input_resource_descs(std::vector<input_resource_desc>& out_data) override;

public:
	inline bool is_dirty() const {return dirty;}

	void set_shader(const std::shared_ptr<vertex_shader>& shader);
	void set_shader(const std::shared_ptr<hull_shader>& shader);
	void set_shader(const std::shared_ptr<domain_shader>& shader);
	void set_shader(const std::shared_ptr<geometry_shader>& shader);
	void set_shader(const std::shared_ptr<pixel_shader>& shader);

	template<typename Shader>
	inline std::shared_ptr<Shader>& get_shader() const {return get_internal_shader<Shader>();}

	const std::vector<std::pair<semantic, UINT>>& get_semantic_list() const {return semantic_list;}

	void set_stencil_ref(UINT stencil_ref) {this->stencil_ref = stencil_ref;}
	UINT get_stencil_ref() const { return stencil_ref; }

private:
	void update_register_datas();

#if defined(USE_IMGUI)
	void on_update_core(const shader_base* shader_base) override;

	bool is_failed() override;
#endif	//	defined(USE_IMGUI)

private:
	bool dirty = true;
	bool dirty_input_resource_datas = true;
	UINT stencil_ref = 0;
	std::shared_ptr<vertex_shader> vs;
	std::shared_ptr<hull_shader> hs;
	std::shared_ptr<domain_shader> ds;
	std::shared_ptr<geometry_shader> gs;
	std::shared_ptr<pixel_shader> ps;

	index_map index_map;
	std::vector<input_resource_desc> input_resource_desces;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_stencil_states[static_cast<int>(depth_stencil_state::max)];
	Microsoft::WRL::ComPtr<ID3D11BlendState> blend_states[static_cast<int>(blend_state::max)];
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_states[static_cast<int>(raster_state::max)];

	struct constant_buffer_data
	{
		bool use;
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	};
	constant_buffer_data deferred_light_buffer_data;
	constant_buffer_data forward_light_buffer_data;
	constant_buffer_data shadow_map_buffer_data;
	constant_buffer_data parameter_buffer_data[static_cast<int>(arrangement::max)];

	std::vector<std::pair<semantic, UINT>> semantic_list;

	//	本来は無くて良い。良いのだが面倒なのでここでやる
	struct primitive_datas
	{
		bool use;
		UINT cbv_indices[static_cast<int>(arrangement::max)];
	};
	primitive_datas primitive_data;

#if defined(USE_IMGUI)
	UINT vs_reload_callback_id = UINT_MAX;
	UINT hs_reload_callback_id = UINT_MAX;
	UINT ds_reload_callback_id = UINT_MAX;
	UINT gs_reload_callback_id = UINT_MAX;
	UINT ps_reload_callback_id = UINT_MAX;
#endif	//	defined(USE_IMGUI)
};
