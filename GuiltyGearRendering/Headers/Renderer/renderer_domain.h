#pragma once

enum class renderer_domain
{
	//	G-Buffer生成
	depth_prepass,
	g_buffer_pass,
	g_buffer_decal_pass,
	screen_space_ambient_occlusion_pass,
	deffered_lighting_pass,
	screen_space_reflection_pass,

	//	被写界深度
	before_dof_translucency_pass,
	before_dof_post_process_pass,
	depth_of_field_pass,
	after_dof_post_process_pass,
	after_dof_translucency_pass,
	after_translucency_post_process_pass,

	//	モーションブラー
	motion_blur_pass,
	after_motion_blur_forward_pass,

	//	トーンマッピング
	before_bloom_post_process_pass,
	bloom_and_tone_mapping_pass,
	after_tone_mapping_post_process_pass,

	//	最終パス
	anti_alias_post_process_pass,
	final_pass,	//	バックバッファへ描き戻し

#if defined(USE_IMGUI)
	//	デバッグ描画
	debug_draw,
#endif // defined(USE_IMGUI)

	//	UIレンダリング
	ui_rendering,

#if defined(USE_IMGUI)
	//	ImGui描画
	imgui_draw_pass,
#endif // defined(USE_IMGUI)

	//	プレゼントパス
	present_pass,
};
