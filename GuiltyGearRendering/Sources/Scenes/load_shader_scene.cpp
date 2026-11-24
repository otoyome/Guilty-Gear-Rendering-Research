#include "Scenes/load_shader_scene.h"

#include "custom_pipeline_state.h"

#include "scenes/editscene/edit_model_scene.h"

load_shader_scene::load_shader_scene()
: scene_base()
{
}

load_shader_scene::~load_shader_scene()
{
}

bool load_shader_scene::initialize_core()
{
	pipeline_state_manager* instance = pipeline_state_manager::instance();

	//	@remainder	失敗時のデカールとUIパイプラインステートの実装
#if	defined(USE_IMGUI)

#pragma	region	3Dモデルエラー用デフォルトパイプラインステート
	{
		//	error_model_color_depth_write
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("3d_failed_depth", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			pso->set_flag(pipeline_state_base::flags::hidden, true);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Editor/3d_failed_depth_vs.hlsl";

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Editor/3d_failed_depth_ps.hlsl";

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	3d_failed_csm
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("3d_failed_shadowmap", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			pso->set_flag(pipeline_state_base::flags::hidden, true);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Editor/3d_failed_csm_vs.hlsl";

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Editor/3d_failed_csm_ps.hlsl";

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	3d_failed_gbuffer
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("3d_failed_gbuffer", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			pso->set_flag(pipeline_state_base::flags::hidden, true);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Editor/3d_failed_gbuffer_vs.hlsl";

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Editor/3d_failed_gbuffer_ps.hlsl";

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	3d_failed_forward
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("3d_failed_forward", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			pso->set_flag(pipeline_state_base::flags::hidden, true);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Editor/3d_failed_forward_vs.hlsl";

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Editor/3d_failed_forward_ps.hlsl";

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
	}
#pragma	endregion	//	3Dモデルエラー用デフォルトパイプラインステート

#pragma	region	ポストプロセスエラー用デフォルトパイプラインステート
	{
		std::shared_ptr<graphics_custom_pipeline_state> pso;
		instance->generate_pipeline_state<graphics_custom_pipeline_state>("postprocess_failed", &pso);
		pso->set_domain(pipeline_state_base::domains::post_process);

		//	頂点シェーダー読み込み
		{
			load_shader_file desc;
			desc.file_path = "Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl";

			std::shared_ptr<vertex_shader> shader;
			shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
			pso->set_shader(shader);
		}

		//	ピクセルシェーダー読み込み
		{
			load_shader_file desc;
			desc.file_path = "Shaders/Editor/postprocess_failed_ps.hlsl";

			std::shared_ptr<pixel_shader> shader;
			shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
			pso->set_shader(shader);
		}
	}
#pragma endregion	//	ポストプロセスエラー用デフォルトパイプラインステート

#pragma	region	頂点カラー編集用デフォルトパイプラインステート
	{
		//	edit_model_color_depth_write
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("edit_model_color_depth_write", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			pso->set_flag(pipeline_state_base::flags::hidden, true);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/EditModel/Color/edit_model_color_depth_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/EditModel/Color/edit_model_color_depth_line_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/EditModel/Color/edit_model_color_depth_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	edit_model_color_csm
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("edit_model_color_csm", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			pso->set_flag(pipeline_state_base::flags::hidden, true);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/EditModel/Color/edit_model_color_csm_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/EditModel/Color/edit_model_color_csm_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/EditModel/Color/edit_model_color_csm_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	edit_model_color_gbuffer
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("edit_model_color_gbuffer", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			pso->set_flag(pipeline_state_base::flags::hidden, true);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/EditModel/Color/edit_model_color_gbuffer_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/EditModel/Color/edit_model_color_gbuffer_line_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/EditModel/Color/edit_model_color_gbuffer_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	edit_model_color_forward
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("edit_model_color_forward", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			pso->set_flag(pipeline_state_base::flags::hidden, true);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/EditModel/Color/edit_model_color_forward_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/EditModel/Color/edit_model_color_forward_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
	}
#pragma	endregion	//	頂点カラー編集用デフォルトパイプラインステート

#endif	//	defined(USE_IMGUI)

#pragma	region	UI用デフォルトパイプラインステート
	{
		std::shared_ptr<graphics_custom_pipeline_state> pso;
		instance->generate_pipeline_state<graphics_custom_pipeline_state>("default_ui", &pso);
		pso->set_domain(pipeline_state_base::domains::post_process);

		//	頂点シェーダー読み込み
		{
			load_shader_file desc;
			desc.file_path = "Shaders/Sprite/sprite_vs.hlsl";

			std::shared_ptr<vertex_shader> shader;
			shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
			pso->set_shader(shader);
		}

		//	ピクセルシェーダー読み込み
		{
			load_shader_file desc;
			desc.file_path = "Shaders/Sprite/sprite_ps.hlsl";

			std::shared_ptr<pixel_shader> shader;
			shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
			pso->set_shader(shader);
		}
	}
#pragma endregion	//	UI用デフォルトパイプラインステート

#pragma	region	GLTF用デフォルトパイプラインステート
	{
		//	深度プリパス
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("gltf_depth", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Gltf/gltf_depth_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Gltf/gltf_depth_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	シャドウマップ生成
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("gltf_shadowmap", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Gltf/gltf_csm_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Gltf/gltf_csm_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Gltf/gltf_csm_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	G-Buffer生成
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("gltf_gbuffer", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Gltf/gltf_gbuffer_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Gltf/gltf_gbuffer_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	gltf_forward
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("gltf_forward", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Gltf/gltf_forward_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Gltf/gltf_forward_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
	}
#pragma	endregion	//	GLTF用デフォルトパイプラインステート

#pragma	region	PMX用デフォルトパイプラインステート
	{
		//	pmx_depth
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("pmx_depth", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Pmx/pmx_depth_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Pmx/pmx_depth_outline_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Pmx/pmx_depth_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	pmx_shadowmap
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("pmx_shadowmap", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Pmx/pmx_csm_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Pmx/pmx_csm_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Pmx/pmx_csm_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	pmx_gbuffer
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("pmx_gbuffer", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Pmx/pmx_gbuffer_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Pmx/pmx_gbuffer_outline_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Pmx/pmx_gbuffer_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	pmx_forward
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("pmx_forward", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Pmx/pmx_forward_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Pmx/pmx_forward_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
	}
#pragma	endregion	//	PMX用デフォルトパイプラインステート

#pragma	region	トゥーンシェーディング用デフォルトパイプラインステート
	{
		//	ggst_depth
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("ggst_depth", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Ggst/ggst_depth_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Ggst/ggst_depth_outline_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Ggst/ggst_depth_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	ggst_shadowmap
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("ggst_shadowmap", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Ggst/ggst_csm_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Ggst/ggst_csm_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Ggst/ggst_csm_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	ggst_gbuffer
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("ggst_gbuffer", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Ggst/ggst_gbuffer_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Ggst/ggst_gbuffer_outline_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Ggst/ggst_gbuffer_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	ggst_forward
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("ggst_forward", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Ggst/ggst_forward_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Ggst/ggst_forward_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
	}
#pragma	endregion	//	トゥーンシェーディング用デフォルトパイプラインステート

#pragma	region	完品トゥーンシェーディング用パイプラインステート
	{
		//	toon_depth_write
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("toon_depth", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			pso->set_flag(pipeline_state_base::flags::hidden, false);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Toon/toon_depth_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Toon/toon_depth_outline_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Toon/toon_depth_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	toon_csm_write
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("toon_shadowmap", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			pso->set_flag(pipeline_state_base::flags::hidden, false);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Toon/toon_csm_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Toon/toon_csm_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Toon/toon_csm_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	toon_gbuffer
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("toon_gbuffer", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			pso->set_flag(pipeline_state_base::flags::hidden, false);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Toon/toon_gbuffer_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Toon/toon_gbuffer_outline_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Toon/toon_gbuffer_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	toon_forward
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("toon_forward", &pso);
			pso->set_domain(pipeline_state_base::domains::surface);
			pso->set_flag(pipeline_state_base::flags::hidden, false);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Toon/toon_forward_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/Toon/toon_forward_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
	}
#pragma	endregion	//	完品トゥーンシェーディング用パイプラインステート

	return true;
}

bool load_shader_scene::uninitialize_core()
{
	return true;
}

void load_shader_scene::update_core(float elapsed_time)
{
#if defined(USE_IMGUI)
	scene_manager::instance()->change_scene<edit_model_scene>();
#endif	//	defined(USE_IMGUI)
}

#if defined(USE_IMGUI)
void load_shader_scene::imgui_render_core()
{
	if (ImGui::TreeNode("load_shader_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
