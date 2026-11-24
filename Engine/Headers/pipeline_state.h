#pragma once

#include "graphics_context.h"
#include "shader.h"

#include <d3d11shader.h>

class material;

//	基底パイプラインステート
//	☆ﾃｷﾄｰ('ω')ｰﾄｷﾃ☆
class pipeline_state_base
{
public:
	enum class flags
	{
		hidden,	//	リストから非表示にするフラグ
	};

	//	処理系統
	enum class domains : std::uint8_t
	{
		surface,		//	メッシュ表面描画
		deferred_decal,	//	遅延デカール
		post_process,	//	ポストプロセス
		user_interface,	//	UI

		unknown,

		system,			//	システム
	};


	enum class cbv_flags
	{
		user_packed = 1 << 0,	//	変数のメモリレイアウトを指定しているかどうか。
	};

	enum class view_flags
	{
		packing = 1 << 0,
		comparison_sampler = 1 << 1,
		texture_component_0 = 1 << 2,
		texture_component_1 = 1 << 3,
		texture_components = 1 << 4,
	};

	enum class variable_kinds : std::uint8_t
	{
		Unknown,
		Scalar,
		Vector,
		MatrixRows,
		MatrixColumns,
		Struct,
	};

	enum class variable_types : std::uint8_t
	{
		Unknown,
		Bool,
		Int,
		UInt,
		Float,

		Buffer,
		BufferEx,
		Texture,
		Texture1d,
		Texture1dArray,
		Texture2d,
		Texture2dArray,
		Texture2dMS,
		Texture2dMSArray,
		Texture3d,
		TextureCube,
		TextureCubeArray,
		StructuredBuffer,
		ByteAddressBuffer,

		RWBuffer,
		RWBufferEx,
		RWTexture1d,
		RWTexture1dArray,
		RWTexture2d,
		RWTexture2dArray,
		RWTexture3d,
		RWStructuredBuffer,
		RWByteAddressBuffer,
		AppendStructuredBuffer,
		ConsumeStructuredBuffer,

		Sampler,
	};

	static UINT get_variable_type_size(variable_kinds kind, variable_types type, UINT rows, UINT columns);

	struct constant_buffer_view_desc
	{
		struct variable_desc
		{
			std::string name;					//	名前
			UINT start_offset;					//	オフセット値
			UINT size;							//	変数サイズ
			variable_kinds kind;				//	型定義
			variable_types type;				//	型情報
			UINT rows;							//	行数
			UINT columns;						//	列数
			std::vector<variable_desc> members;	//	メンバ変数

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		std::string name;						//	名前
		view_flags flags;						//	フラグ情報
		cbv_flags cbv_flags;					//	定数バッファ用フラグ
		UINT register_index;					//	レジスター番号
		UINT total_size;						//	定数バッファサイズ
		std::vector<variable_desc> variables;	//	メンバ変数情報

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct resource_view_desc
	{
		std::string name;		//	名前
		view_flags flags;		//	フラグ情報
		UINT register_index;	//	レジスター番号
		UINT array_size;		//	配列数(通常は1)
		variable_types type;	//	型情報

		inline bool is_srvs() const
		{
			return	type == variable_types::Buffer ||
					type == variable_types::BufferEx ||
					type == variable_types::Texture1d ||
					type == variable_types::Texture1dArray ||
					type == variable_types::Texture2d ||
					type == variable_types::Texture2dArray ||
					type == variable_types::Texture2dMS ||
					type == variable_types::Texture2dMSArray ||
					type == variable_types::Texture3d ||
					type == variable_types::TextureCube ||
					type == variable_types::TextureCubeArray ||
					type == variable_types::StructuredBuffer ||
					type == variable_types::ByteAddressBuffer;
		}
		inline bool is_uavs() const
		{
			return	type == variable_types::RWBuffer ||
					type == variable_types::RWBufferEx ||
					type == variable_types::RWTexture1d ||
					type == variable_types::RWTexture1dArray ||
					type == variable_types::RWTexture2d ||
					type == variable_types::RWTexture2dArray ||
					type == variable_types::RWTexture3d ||
					type == variable_types::RWStructuredBuffer ||
					type == variable_types::RWByteAddressBuffer ||
					type == variable_types::AppendStructuredBuffer ||
					type == variable_types::ConsumeStructuredBuffer;
		}
		inline bool is_sampler() const
		{
			return	type == variable_types::Sampler;
		}
		bool is_valid() const
		{
			return is_srvs() || is_uavs() || is_sampler();
		}

		inline bool is_texture() const
		{
			return	type == variable_types::Texture1d ||
					type == variable_types::Texture1dArray ||
					type == variable_types::Texture2d ||
					type == variable_types::Texture2dArray ||
					type == variable_types::Texture2dMS ||
					type == variable_types::Texture2dMSArray ||
					type == variable_types::Texture3d ||
					type == variable_types::TextureCube ||
					type == variable_types::TextureCubeArray ||
					type == variable_types::RWTexture1d ||
					type == variable_types::RWTexture1dArray ||
					type == variable_types::RWTexture2d ||
					type == variable_types::RWTexture2dArray ||
					type == variable_types::RWTexture3d;
		}
		inline bool is_buffer() const
		{
			return	type == variable_types::Buffer ||
					type == variable_types::BufferEx ||
					type == variable_types::RWBuffer ||
					type == variable_types::RWBufferEx ||
					type == variable_types::StructuredBuffer ||
					type == variable_types::ByteAddressBuffer ||
					type == variable_types::RWStructuredBuffer ||
					type == variable_types::RWByteAddressBuffer ||
					type == variable_types::AppendStructuredBuffer ||
					type == variable_types::ConsumeStructuredBuffer;
		}

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

	struct input_resource_desc
	{
		std::vector<constant_buffer_view_desc> cbvs;
		std::vector<resource_view_desc> rvs;

		template<class Archive>
		void serialize(Archive& archive, int version);
	};

public:
	//	システムフラグ取得・設定
	bool is_flag(flags flag) const;
	void set_flag(flags flag, bool value);

	//	名前取得・設定
	void set_name(std::string_view name);
	std::string_view get_name() const;

	//	ドメイン設定
	void set_domain(domains domain);
	domains get_domain() const;

public:
	pipeline_state_base();
	virtual ~pipeline_state_base();

	virtual void generate_input_resource_descs(std::vector<input_resource_desc>& out_data) {}
	void generate_input_resource_desc(input_resource_desc& out_data, shader_base* shader);

#if defined(USE_IMGUI)
	void on_updates(const shader_base* shader_base);

	virtual void on_update_core(const shader_base* shader_base) {}

public:
	UINT register_reload_callback(std::function<void(const pipeline_state_base*)> callback);
	void unregister_reload_callback(UINT id);

private:
	struct callback_data
	{
		UINT id;
		std::function<void(const pipeline_state_base*)> callback;
		callback_data(UINT id, std::function<void(const pipeline_state_base*)> callback) : id(id), callback(callback){}
	};

	UINT callback_id_counter = 0;
	std::vector<callback_data> reload_callbacks;
#endif	//	defined(USE_IMGUI)

private:
	std::string name;
	UINT system_flags;
	domains domain;
};
ENUM_ATTR_BITFLAG(pipeline_state_base::cbv_flags);
ENUM_ATTR_BITFLAG(pipeline_state_base::view_flags);

//	描画用パイプラインステート
class graphics_pipeline_state : public pipeline_state_base
{
public:
	graphics_pipeline_state(){}
	virtual ~graphics_pipeline_state() {}

	virtual	const std::vector<std::pair<semantic, UINT>>& get_semantic_list() const
	{
		static const std::vector<std::pair<semantic, UINT>> dummy;
		return dummy;
	}

	virtual	void begin(const graphics_context& context) = 0;
	virtual	void update_parameters(const graphics_context& context, const material& material, const buffer_datas* buffer_data)
	{
		assert(!"not compatible set_parameters");
	}
	virtual	void end(const graphics_context& context) = 0;

#if defined(USE_IMGUI)
	virtual	bool is_failed() {return false;}
#endif	//	defined(USE_IMGUI)
};

using graphics_pipeline_state_ptr = std::shared_ptr<graphics_pipeline_state>;

#if 0
//	コンピュート用パイプラインステート
class compute_pipeline_state : public pipeline_state_base
{
public:
	compute_pipeline_state(){}
	virtual ~compute_pipeline_state() {}

	virtual	void begin(const graphics_context& context) = 0;
	virtual	void dispatch(UINT index) = 0;
	virtual	void end(const graphics_context& context) = 0;
};

using compute_pipeline_state_ptr = std::shared_ptr<compute_pipeline_state>;
#endif

//	パイプラインステート管理クラス
class pipeline_state_manager
{
	SINGLETON(pipeline_state_manager)

	using graphics_pipeline_state_ptr_map = std::unordered_map<std::string, graphics_pipeline_state_ptr>;

private:
	pipeline_state_manager(){}
	~pipeline_state_manager(){}

public:
	template<class type>
	bool	generate_pipeline_state(std::string_view key, graphics_pipeline_state_ptr* pipeline_state_ptr = nullptr)
	{
		if (auto ps = pipeline_state_manager::search(key))
		{
			if (pipeline_state_ptr)
				*pipeline_state_ptr = std::static_pointer_cast<type>(ps);
			return true;
		}

		auto obj = std::make_shared<type>();
		obj->set_name(key);
		if (register_pipeline_state(std::static_pointer_cast<graphics_pipeline_state>(obj)))
		{
			if (pipeline_state_ptr)
				*pipeline_state_ptr = obj;
			return true;
		}
		return false;
	}
	template<class type>
	bool	generate_pipeline_state(std::string_view key, std::shared_ptr<type>* pipeline_state_ptr)
	{
		if (auto ps = pipeline_state_manager::search(key))
		{
			if (pipeline_state_ptr)
				*pipeline_state_ptr = std::static_pointer_cast<type>(ps);
			return true;
		}

		auto obj = std::make_shared<type>();
		obj->set_name(key);
		if (register_pipeline_state(std::static_pointer_cast<graphics_pipeline_state>(obj)))
		{
			if (pipeline_state_ptr)
				*pipeline_state_ptr = obj;
			return true;
		}
		return false;
	}
	graphics_pipeline_state_ptr	search(std::string_view key);
	std::vector<graphics_pipeline_state_ptr> get_domain_lists(pipeline_state_base::domains domain);

private:
	bool	register_pipeline_state(graphics_pipeline_state_ptr& pipeline_state);

private:
	graphics_pipeline_state_ptr_map	graphics_pipeline_state_map;
};
