#pragma once

enum class shader_load_type
{
	cso,
	file,

	max,
};

enum class shader_type
{
	vertex,
	hull,
	domain,
	geometry,
	pixel,
	compute,

	max,
};

enum class shader_model
{
	_5_0,
	_6_0,

	max,
};

enum class shader_compiler_type
{
	fxc,
	dxc,
};

enum class shader_compile_flags
{
	none = 0,
	disable_optimize = 1 << 00,	//	最適化OFF
	debug = 1 << 01,	//	デバッグフラグ
	pack_matrix_row_major = 1 << 02,	//	行列を行メジャーで格納します
	warning_are_error = 1 << 03,	//	警告もデバッグ扱いにする
};
ENUM_ATTR_BITFLAG(shader_compile_flags);

struct shader_define
{
	std::string_view name;
	std::string_view value;
	shader_define(){}
	shader_define(std::string_view name, std::string_view value) : name(name), value(value) {}
};

struct load_shader_cso
{
	std::string_view file_path;
	std::vector<D3D11_INPUT_ELEMENT_DESC> input_element_desc;
};

struct load_shader_file
{
	std::string_view file_path;
	std::string_view entry_point = "main";
	shader_model model = shader_model::_5_0;
	UINT compile_flags;
	std::vector<std::string_view> include_directries;	//	追加のインクルードディレクトリ
	std::vector<shader_define> defines;	//	マクロ

	std::vector<D3D11_INPUT_ELEMENT_DESC> input_element_desc;
};

struct shader_meta_data
{
	//	簡易的なシェーダーのメタデータ
	struct include_data
	{
		std::string include_file_path;
		std::filesystem::file_time_type last_write_time;
		template<class Archive>
		void serialize(Archive& archive, int version)
		{
			archive(CEREAL_NVP(include_file_path), CEREAL_NVP(last_write_time));
		}
	};
	std::vector<include_data> include_files;

	bool serialize(std::string_view file_path);
	bool deserialize(std::string_view file_path);
};

class shader_base
{
protected:
	using file_path_type = std::filesystem::path;
	using file_time_type = std::filesystem::file_time_type;

public:
	virtual shader_type get_type() const = 0;
	std::shared_ptr<void> get_reflection() const { return internal_reflection; }

protected:
	shader_base(load_shader_cso cso_data);
	shader_base(load_shader_file file_data);
	virtual ~shader_base(){}

	void set_shader_compiler_type(shader_compiler_type type);
	void set_reflection(std::shared_ptr<void> reflection);

public:
	bool load();

#if defined(USE_IMGUI)
	bool is_failed();
	bool is_reload();
	void reload();

	UINT register_reload_callback(std::function<void(const shader_base*)> callback);
	void unregister_reload_callback(UINT id);

protected:
	void claer_monitor_files();
	void register_monitor_file(std::string_view file_path);

public:
#endif	//	defined(USE_IMGUI)

protected:
	virtual bool load_from_cso(load_shader_cso cso_data) = 0;
	virtual bool load_from_file(load_shader_file file_data) = 0;

private:
	shader_load_type load_type;
	shader_compiler_type compiler_type;
	load_shader_cso cso_data;
	load_shader_file file_data;

	std::shared_ptr<void> internal_reflection;

#if defined(USE_IMGUI)
	struct monitor_file_data
	{
		file_path_type file_path;
		file_time_type last_write_time;
		monitor_file_data() {}
		monitor_file_data(file_path_type& file_path, file_time_type& last_write_time)
			: file_path(file_path)
			, last_write_time(last_write_time)
		{
		}
	};
	struct callback_data
	{
		UINT id;
		std::function<void(const shader_base*)> callback;
	};

	UINT callback_id_counter = 0;
	std::vector<monitor_file_data> monitor_files;
	std::vector<callback_data> reload_callbacks;

protected:
	bool failed_flag = true;	//	読込み成功/失敗フラグ
#endif	//	defined(USE_IMGUI)
};

class vertex_shader : public shader_base
{
public:
	static shader_type get_static_type() { return shader_type::vertex; }
	shader_type get_type() const override { return get_static_type(); }
	Microsoft::WRL::ComPtr<ID3D11VertexShader> get_shader() const { return shader; }
	const std::vector<D3D11_INPUT_ELEMENT_DESC>& get_input_element_desc() const { return input_element_desc; }
	Microsoft::WRL::ComPtr<ID3D11InputLayout> get_input_layout() const { return input_layout; }

public:
	vertex_shader(load_shader_cso cso_data);
	vertex_shader(load_shader_file file_data);
	~vertex_shader() override;

private:
	bool load_from_cso(load_shader_cso cso_data) override;
	bool load_from_file(load_shader_file file_data) override;
	bool generate_input_layout(std::shared_ptr<void> shader_reflection);

private:
	bool is_set_input_element_desc = false;
	std::vector<D3D11_INPUT_ELEMENT_DESC> input_element_desc;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
};

class hull_shader : public shader_base
{
public:
	static shader_type get_static_type() { return shader_type::hull; }
	shader_type get_type() const override { return get_static_type(); }
	Microsoft::WRL::ComPtr<ID3D11HullShader> get_shader() const { return shader; }

public:
	hull_shader(load_shader_cso cso_data);
	hull_shader(load_shader_file file_data);
	~hull_shader() override;

private:
	bool load_from_cso(load_shader_cso cso_data) override;
	bool load_from_file(load_shader_file file_data) override;

private:
	Microsoft::WRL::ComPtr<ID3D11HullShader> shader;
};

class domain_shader : public shader_base
{
public:
	static shader_type get_static_type() { return shader_type::domain; }
	shader_type get_type() const override { return get_static_type(); }
	Microsoft::WRL::ComPtr<ID3D11DomainShader> get_shader() const { return shader; }

public:
	domain_shader(load_shader_cso cso_data);
	domain_shader(load_shader_file file_data);
	~domain_shader() override;

private:
	bool load_from_cso(load_shader_cso cso_data) override;
	bool load_from_file(load_shader_file file_data) override;

private:
	Microsoft::WRL::ComPtr<ID3D11DomainShader> shader;
};

class geometry_shader : public shader_base
{
public:
	static shader_type get_static_type() { return shader_type::geometry; }
	shader_type get_type() const override { return get_static_type(); }
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> get_shader() const { return shader; }

public:
	geometry_shader(load_shader_cso cso_data);
	geometry_shader(load_shader_file file_data);
	~geometry_shader() override;

private:
	bool load_from_cso(load_shader_cso cso_data) override;
	bool load_from_file(load_shader_file file_data) override;

private:
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> shader;
};

class pixel_shader : public shader_base
{
public:
	static shader_type get_static_type() { return shader_type::pixel; }
	shader_type get_type() const override { return get_static_type(); }
	Microsoft::WRL::ComPtr<ID3D11PixelShader> get_shader() const { return shader; }

public:
	pixel_shader(load_shader_cso cso_data);
	pixel_shader(load_shader_file file_data);
	~pixel_shader() override;

private:
	bool load_from_cso(load_shader_cso cso_data) override;
	bool load_from_file(load_shader_file file_data) override;

private:
	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
};

class compute_shader : public shader_base
{
public:
	static shader_type get_static_type() { return shader_type::compute; }
	shader_type get_type() const override { return get_static_type(); }
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> get_shader() const { return shader; }

public:
	compute_shader(load_shader_cso cso_data);
	compute_shader(load_shader_file file_data);
	~compute_shader() override;

private:
	bool load_from_cso(load_shader_cso cso_data) override;
	bool load_from_file(load_shader_file file_data) override;

private:
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader;
};

using shader_ptr = std::shared_ptr<shader_base>;

class shader_manager
{
	using shader_map = std::unordered_map<system_utility::hash_key, std::weak_ptr<shader_base>>;
	SINGLETON(shader_manager)

private:
	shader_manager(){}
	~shader_manager(){}

public:
	template<class type>
	bool load_shader(std::shared_ptr<type>& out_shader, load_shader_cso data)
	{
		shader_ptr shader;
		if (load_shader(type::get_static_type(), shader, data))
		{
			out_shader	= std::static_pointer_cast<type>(shader);
			return	true;
		}
		return	false;
	}
	template<class type>
	bool load_shader(std::shared_ptr<type>& out_shader, load_shader_file data)
	{
		shader_ptr shader;
		if (load_shader(type::get_static_type(), shader, data))
		{
			out_shader = std::static_pointer_cast<type>(shader);
			return	true;
		}
		return	false;
	}

	void check_shader_update_and_recompile();

private:
	bool	load_shader(shader_type type, shader_ptr& out_shader, load_shader_cso data);
	bool	load_shader(shader_type type, shader_ptr& out_shader, load_shader_file data);

private:
	shader_map	shaderes;
	UINT check_counter = 0;
};
