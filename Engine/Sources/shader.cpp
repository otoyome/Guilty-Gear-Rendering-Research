#include "shader.h"

#include "render_data.h"

#include "dxcapi.h"
#include <d3dcompiler.h>

#include "cereal/cereal.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/chrono.hpp"

namespace
{
	constexpr std::string_view	compiled_directory = "Assets/CSO/";
	constexpr std::string_view	compiled_extension = ".cso";
	constexpr std::wstring_view	shader_types[static_cast<UINT>(shader_type::max)]	=
	{
		L"vs",	//  Vertex Shader
		L"hs",	//  Hull Shader
		L"ds",	//  Domain Shader
		L"gs",	//  Geometry Shader
		L"ps",	//  Pixel Shader
		L"cs",	//  Compute Shader
	};

	constexpr std::wstring_view	shader_models[static_cast<UINT>(shader_model::max)]	=
	{
		L"5_0",
		L"6_0",
	};

	//	シェーダーコンパイルファイル入力情報
	struct compile_input_file
	{
		UINT compile_flags;									//  コンパイル設定
		std::string_view file_path;							//  シェーダーファイルパス
		std::string_view entry_point;						//  エントリーポイント
		shader_type type;									//  シェーダーステージ
		shader_model model;									//  シェーダーモデル
		std::vector<std::string_view> include_directries;	//  インクルードディレクトリ
		std::vector<shader_define> defines;					//  マクロ情報
	};

	//	シェーダーコンパイル出力情報
	struct compile_output
	{
		std::shared_ptr<void> blob;				//	シェーダーコンパイル済み情報
		const char* shader_data = nullptr;		//	シェーダー情報
		std::size_t shader_size = 0;			//	シェーダーサイズ
		std::vector<char> shader_hash;			//	シェーダーハッシュ
		std::string error_message;				//	エラーメッセージ
		shader_compiler_type compiler_type;		//	コンパイル
		std::vector<std::string> include_files;	//	インクルードファイルパス

		bool is_valid() const
		{
			return blob.get() != nullptr;
		}
	};

	//	シェーダーインクルードハンドル(FXC)
	class fxc_include_handler : public ID3DInclude
	{
	public:
		static std::vector<std::wstring> build_directry_pass(std::wstring_view file_path)
		{
			std::vector<std::wstring> result;

			std::filesystem::path path = file_path;
			std::filesystem::path root_path = path.begin()->c_str();
			if (!std::filesystem::exists(root_path) || !std::filesystem::is_directory(root_path))
			{
				return result; // 無効なルートなら空
			}

			for (const auto& entry : std::filesystem::recursive_directory_iterator(root_path))
			{
				if (entry.is_directory())
				{
					result.push_back(entry.path().wstring());
				}
			}
			return result;
		}
		static std::vector<std::wstring> build_directry_pass(std::string_view file_path)
		{
			return build_directry_pass(system_utility::convert_shift_jis_to_utf16(file_path));
		}

		std::vector<std::wstring> include_directries;
		std::vector<std::string> include_files;

		fxc_include_handler(const std::vector<std::wstring>& directries)
		: include_directries()
		{
			for (auto& directry_pass : directries)
			{
				include_directries.push_back(directry_pass);
			}
		}

		HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override
		{
			for (auto& include_directry : include_directries)
			{
				std::wstring fullPath = include_directry + L"\\" + std::wstring(pFileName, pFileName + strlen(pFileName));
				FILE* file = nullptr;
				_wfopen_s(&file, fullPath.c_str(), L"rb");
				if (!file)
					continue;

				fseek(file, 0, SEEK_END);
				long size = ftell(file);
				rewind(file);

				char* data = new char[size];
				fread(data, 1, size, file);
				fclose(file);

				*ppData = data;
				*pBytes = size;

				//	インクルードファイル登録
				{
					bool find = false;
					std::string file_path = system_utility::convert_utf16_to_shift_jis(fullPath);
					for (auto& path : include_files)
					{
						if (file_path == path)
						{
							find = true;
							break;
						}
					}
					if (!find)
					{
						include_files.push_back(file_path);
					}
				}
				return S_OK;
			}
			return E_FAIL;
		}

		HRESULT Close(LPCVOID pData) override
		{
			delete[] reinterpret_cast<const char*>(pData);
			return S_OK;
		}
	};

	//	シェーダーインクルードハンドル(DXC)
	class dxc_include_handler : public IDxcIncludeHandler
	{
	public:
		dxc_include_handler(IDxcUtils* utils)
		: ref_counter(1)
		{
			utils->CreateDefaultIncludeHandler(fallback.GetAddressOf());
		}

		const std::vector<std::wstring>& List() const { return files; }

		ULONG STDMETHODCALLTYPE AddRef() override
		{
			return ++ref_counter;
		}
		ULONG STDMETHODCALLTYPE Release() override
		{
			ULONG c = --ref_counter;
			if (!c) delete this; return c;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
		{
			if (riid == __uuidof(IDxcIncludeHandler) || riid == __uuidof(IUnknown))
			{
				*ppv = static_cast<IDxcIncludeHandler*>(this);
				AddRef();
				return S_OK;
			}
			*ppv = nullptr; return E_NOINTERFACE;
		}

		//	IDxcIncludeHandler
		HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource) override
		{
			// 前処理で要求されたパスを記録（正規化はお好みで）
			auto key = std::wstring(pFilename);
			if (seen.insert(key).second)
				files.push_back(key);

			// 実体の読み込みは既定ハンドラへ委譲
			return fallback->LoadSource(pFilename, ppIncludeSource);
		}

	private:
		std::vector<std::wstring> files;
		std::unordered_set<std::wstring> seen;
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> fallback;
		std::atomic<ULONG> ref_counter;
	};

	//	ハッシュキー生成
	system_utility::hash_key generate_hash_key(const load_shader_cso& input_data)
	{
		return system_utility::generate_hash(input_data.file_path);
	}
	system_utility::hash_key generate_hash_key(const load_shader_file& input_data)
	{
		auto key_hash = system_utility::generate_hash(input_data.file_path);
		key_hash += system_utility::generate_hash(input_data.entry_point);
		for (auto& define : input_data.defines)
		{
			key_hash += system_utility::generate_hash(define.name);
			key_hash += system_utility::generate_hash(define.value);
		}
		for (auto& include : input_data.include_directries)
		{
			key_hash += system_utility::generate_hash(include);
		}
		return key_hash;
	}
	system_utility::hash_key generate_hash_key(const compile_input_file& input_data)
	{
		auto key_hash = system_utility::generate_hash(input_data.file_path);
		key_hash += system_utility::generate_hash(input_data.entry_point);
		for (auto& define : input_data.defines)
		{
			key_hash += system_utility::generate_hash(define.name);
			key_hash += system_utility::generate_hash(define.value);
		}
		for (auto& include : input_data.include_directries)
		{
			key_hash += system_utility::generate_hash(include);
		}
		return key_hash;
	}

	//	ファイルパス生成
	std::filesystem::path generate_file_path(const compile_input_file& input_data)
	{
		std::filesystem::path compiled_file_path = std::string(compiled_directory);
		compiled_file_path += std::string(input_data.file_path);
		compiled_file_path.replace_extension("");
		compiled_file_path += std::to_string(generate_hash_key(input_data));
		compiled_file_path += std::string(compiled_extension);
		return compiled_file_path;
	}

	//	コンパイル済みファイル読込
	bool load_compiled_file(std::string_view file_path, compile_output& output_data)
	{
		FILE* fp{ nullptr };
		fopen_s(&fp, file_path.data(), "rb");
		_ASSERT_EXPR_A(fp, "CSO File not found");
		if (!fp)
			return false;

		fseek(fp, 0, SEEK_END);
		long size{ ftell(fp) };
		fseek(fp, 0, SEEK_SET);

		std::unique_ptr<unsigned char[]> buffer{ std::make_unique<unsigned char[]>(size) };
		fread(buffer.get(), size, 1, fp);
		fclose(fp);
		//	DXBC (FXC)
		if (buffer[0] == L'D' || buffer[1] == L'X' || buffer[2] == L'B' || buffer[3] == L'C')
		{
			output_data.compiler_type = shader_compiler_type::fxc;

			//	D3DBlob生成
			Microsoft::WRL::ComPtr<ID3DBlob> blob;
			if (FAILED(D3DCreateBlob(size, blob.GetAddressOf())))
			{
				output_data.error_message	= std::string("Shader Create Blob Failed [")
											+ std::string(file_path)
											+ std::string("]\n");
				DEBUG_ERROR_CATEGORY(logger::categorys::shader, output_data.error_message.data());
				return false;
			}
			memcpy_s(blob->GetBufferPointer(), size, buffer.get(), size);

			//	結果を返す
			output_data.shader_data = static_cast<const char*>(blob->GetBufferPointer());
			output_data.shader_size = blob->GetBufferSize();
			auto internal_state = std::make_shared<Microsoft::WRL::ComPtr<ID3DBlob>>();
			*internal_state = blob;
			output_data.blob = internal_state;
			return true;
		}

		//	DXIL (DXC)
		if (buffer[0] == L'D' || buffer[1] == L'X' || buffer[2] == L'I' || buffer[3] == L'L')
		{
			output_data.compiler_type = shader_compiler_type::dxc;

			//	DXC Utils生成
			Microsoft::WRL::ComPtr<IDxcUtils> dxc_utility;
			if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxc_utility.GetAddressOf()))))
			{
				output_data.error_message	= std::string("Not Found DXC Library [")
											+ std::string(file_path)
											+ std::string("]\n");
				DEBUG_ERROR_CATEGORY(logger::categorys::shader, output_data.error_message.data());
				return false;
			}

			//	D3DBlob生成
			Microsoft::WRL::ComPtr<IDxcBlobEncoding> blob;
			if (FAILED(dxc_utility->CreateBlob(buffer.get(), size, DXC_CP_ACP, blob.GetAddressOf())))
			{
				output_data.error_message	= std::string("Shader Create Blob Failed [")
											+ std::string(file_path)
											+ std::string("]\n");
				DEBUG_ERROR_CATEGORY(logger::categorys::shader, output_data.error_message.data());
				return false;
			}

			//	結果を返す
			output_data.shader_data = static_cast<const char*>(blob->GetBufferPointer());
			output_data.shader_size = blob->GetBufferSize();
			auto internal_state = std::make_shared<Microsoft::WRL::ComPtr<IDxcBlobEncoding>>();
			*internal_state = blob;
			output_data.blob = internal_state;
			return true;
		}
		output_data.error_message	= std::string("Not Shader File [")
									+ std::string(file_path)
									+ std::string("]\n");
		DEBUG_ERROR_CATEGORY(logger::categorys::shader, output_data.error_message.data());
		return false;
	}

	//	シェーダーコンパイル
	bool compile_from_file_fxc(const compile_input_file& input_data, compile_output& output_data)
	{
		HRESULT hr;
		//	インクルードパス構築
		std::vector<std::wstring> include_paths = fxc_include_handler::build_directry_pass(input_data.file_path);
		for (auto& include_directry : input_data.include_directries)
		{
			include_paths.emplace_back(system_utility::convert_shift_jis_to_utf16(include_directry));
		}

		std::vector<std::wstring> convert_texts;
		convert_texts.reserve(input_data.defines.size() + include_paths.size() + 50);

		//	ファイルパス変換
		std::wstring file_path = system_utility::convert_shift_jis_to_utf16(input_data.file_path);

		UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS;
		//	最適化OFF
		system_utility::set_bitflag(compile_flags, D3DCOMPILE_SKIP_OPTIMIZATION,
			system_utility::is_bitflag(input_data.compile_flags, shader_compile_flags::disable_optimize));
		//	デバッグ情報を含める
		system_utility::set_bitflag(compile_flags, D3DCOMPILE_DEBUG,
			system_utility::is_bitflag(input_data.compile_flags, shader_compile_flags::debug));

		//	マクロ
		std::vector<D3D_SHADER_MACRO> defines;
		for (std::size_t index = 0; index < input_data.defines.size(); ++index)
		{
			D3D_SHADER_MACRO define;
			define.Name = input_data.defines[index].name.data();
			define.Definition = input_data.defines[index].value.data();
			defines.push_back(define);
		}
		if (!input_data.defines.empty())
		{
			//	終端指定
			defines.push_back({nullptr, nullptr});
		}

		//	ターゲット設定
		std::string target;
		{
			std::string shader_type = system_utility::convert_utf16_to_shift_jis(shader_types[static_cast<UINT>(input_data.type)]);
			std::string shader_model = system_utility::convert_utf16_to_shift_jis(shader_models[static_cast<UINT>(input_data.model)]);
			target = shader_type.data();
			target += "_";
			target += shader_model.data();
		}

		//	D3DBlob生成
		Microsoft::WRL::ComPtr<ID3DBlob> blob;
		{
			FILE* fp{ nullptr };
			fopen_s(&fp, input_data.file_path.data(), "rb");
			_ASSERT_EXPR_A(fp, "File not found");
			if (!fp)
				return false;

			fseek(fp, 0, SEEK_END);
			long size{ ftell(fp) };
			fseek(fp, 0, SEEK_SET);

			std::unique_ptr<unsigned char[]> buffer{ std::make_unique<unsigned char[]>(size) };
			fread(buffer.get(), size, 1, fp);
			fclose(fp);

			if (FAILED(D3DCreateBlob(size, blob.GetAddressOf())))
			{
				output_data.error_message = std::string("Shader Create Blob Failed [")
					+ std::string(input_data.file_path)
					+ std::string("]\n");
				DEBUG_ERROR_CATEGORY(logger::categorys::shader, output_data.error_message.data());
				return false;
			}
			memcpy_s(blob->GetBufferPointer(), size, buffer.get(), size);
		}

		//	事前にインクルードファイルを収集する
		{
			// プリプロセス（前処理）だけ実行
			Microsoft::WRL::ComPtr<ID3DBlob> preprocessed;
			Microsoft::WRL::ComPtr<ID3DBlob> errors;

			std::unique_ptr<fxc_include_handler> include_handler = std::make_unique<fxc_include_handler>(include_paths);
			hr = D3DPreprocess(blob->GetBufferPointer(),
								blob->GetBufferSize(),
								input_data.file_path.data(),
								defines.data(),
								include_handler.get(),
								preprocessed.GetAddressOf(),
								errors.GetAddressOf());
			if (SUCCEEDED(hr))
				output_data.include_files = include_handler->include_files;
		}

		//	シェーダーファイルコンパイル
		Microsoft::WRL::ComPtr<ID3DBlob> source_blob, error_blob;
		std::unique_ptr<fxc_include_handler> include_handler = std::make_unique<fxc_include_handler>(include_paths);
		hr = D3DCompile(blob->GetBufferPointer(),
						blob->GetBufferSize(),
						input_data.file_path.data(),
						defines.data(),
						include_handler.get(),
						input_data.entry_point.data(),
						target.data(),
						compile_flags,
						0,
						source_blob.GetAddressOf(),
						error_blob.GetAddressOf());
		if (error_blob)
			output_data.error_message = static_cast<char*>(error_blob->GetBufferPointer());

		//	コンパイル失敗
		if (FAILED(hr))
		{
			output_data.error_message	= std::string("Shader Compile Failed [")
										+ std::string(input_data.file_path)
										+ std::string("]\n")
										+ output_data.error_message;
			DEBUG_ERROR_CATEGORY(logger::categorys::shader, output_data.error_message.data());
			return	false;
		}

		//	コンパイル結果を返す
		output_data.shader_data = static_cast<const char*>(source_blob->GetBufferPointer());
		output_data.shader_size = source_blob->GetBufferSize();
		auto internal_state = std::make_shared<Microsoft::WRL::ComPtr<ID3DBlob>>();
		*internal_state = source_blob;
		output_data.blob = internal_state;
		output_data.include_files = include_handler->include_files;

		//	コンパイルしたファイルを保存
		{
			std::filesystem::file_time_type input_write_time = std::filesystem::last_write_time(input_data.file_path);
			std::filesystem::path compiled_file_path = generate_file_path(input_data);
			std::filesystem::path output_directry_path(compiled_file_path.parent_path());
			if (!std::filesystem::exists(output_directry_path))
			{
				std::filesystem::create_directories(output_directry_path);
			}
			//	csoファイル生成
			{
				{
					std::ofstream os(compiled_file_path.string().c_str(), std::ios::binary);
					os.write(output_data.shader_data, output_data.shader_size);
				}
				std::filesystem::last_write_time(compiled_file_path, input_write_time);	//	ファイル時間を変えておく
			}
			//	簡易メタデータファイル生成
			{
				shader_meta_data meta_data;
				for (auto& include_file : output_data.include_files)
				{
					meta_data.include_files.emplace_back();
					auto& data = meta_data.include_files.back();
					data.include_file_path = include_file;
					data.last_write_time = std::filesystem::last_write_time(data.include_file_path);
				}
				compiled_file_path.replace_extension(".meta");
				meta_data.serialize(compiled_file_path.string());
				std::filesystem::last_write_time(compiled_file_path, input_write_time);	//	ファイル時間を変えておく
			}
		}

		return	true;
	}
	bool compile_from_file_dxc(const compile_input_file& input_data, compile_output& output_data)
	{
		std::vector<std::wstring> include_paths = fxc_include_handler::build_directry_pass(input_data.file_path);

		HRESULT hr;
		std::vector<std::wstring> convert_texts;
		convert_texts.reserve(input_data.defines.size() + input_data.include_directries.size() + include_paths.size() + 50);

		//	ファイルパス変換
		std::wstring file_path = system_utility::convert_shift_jis_to_utf16(input_data.file_path);

		//	DXC API準備
		Microsoft::WRL::ComPtr<IDxcLibrary> library_handle;
		if (FAILED(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(library_handle.GetAddressOf()))))
			return false;

		//	DXC Compiler生成
		Microsoft::WRL::ComPtr<IDxcCompiler3> compiler_handle;
		if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler_handle.GetAddressOf()))))
			return false;

		//	DXC Utils生成
		Microsoft::WRL::ComPtr<IDxcUtils> dxc_utility;
		if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxc_utility.GetAddressOf()))))
			return false;

		//	インクルードハンドラーを生成
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> include_handler;
		if (FAILED(dxc_utility->CreateDefaultIncludeHandler(include_handler.GetAddressOf())))
			return false;

		//	コンパイル時引数設定
		//	https://github.com/microsoft/DirectXShaderCompiler/wiki/Using-dxc.exe-and-dxcompiler.dll#dxcompiler-dll-interface
		std::vector<LPCWSTR> arguments;
		{
			arguments.push_back(DXC_ARG_ENABLE_STRICTNESS);
			if (system_utility::is_bitflag(input_data.compile_flags, shader_compile_flags::disable_optimize))
			{
				//	最適化OFF
				arguments.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
			}

			if (system_utility::is_bitflag(input_data.compile_flags, shader_compile_flags::debug))
			{
				//	デバッグ情報を埋め込む
				arguments.push_back(DXC_ARG_DEBUG);
			}

			//	ターゲット設定
			{
				arguments.push_back(L"-T");

				std::wstring shader_type = std::wstring(shader_types[static_cast<UINT>(input_data.type)]);
				std::wstring shader_model = std::wstring(shader_models[static_cast<UINT>(input_data.model)]);
				convert_texts.push_back(shader_type + std::wstring(L"_") + shader_model);
				arguments.push_back(convert_texts.back().data());
			}

			//	マクロ
			for (std::size_t index = 0; index < input_data.defines.size(); ++index)
			{
				arguments.push_back(L"-D");
				convert_texts.emplace_back();
				convert_texts.back() = system_utility::convert_shift_jis_to_utf16(input_data.defines[index].name);
				convert_texts.back() += L"=";
				convert_texts.back() += system_utility::convert_shift_jis_to_utf16(input_data.defines[index].value);
				arguments.push_back(convert_texts.back().data());
			}

			//	追加のインクルードディレクトリ設定
			for (auto& directry_pass : include_paths)
			{
				arguments.push_back(L"-I");
				arguments.push_back(directry_pass.data());
			}
			for (std::size_t index = 0; index < input_data.include_directries.size(); ++index)
			{
				arguments.push_back(L"-I");
				convert_texts.push_back(system_utility::convert_shift_jis_to_utf16(input_data.include_directries[index].data()));
				arguments.push_back(convert_texts.back().data());
			}

			//	行列優先指定
			//	ここで定義する事で、シェーダー側で row_major の設定をする必要がなくなる。
			if (system_utility::is_bitflag(input_data.compile_flags, shader_compile_flags::pack_matrix_row_major))
			{
				//	デバッグ情報を埋め込む
				arguments.push_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR);
			}

			//	エントリーポイント
			arguments.push_back(L"-E");
			convert_texts.push_back(system_utility::convert_shift_jis_to_utf16(input_data.entry_point.data()));
			arguments.push_back(convert_texts.back().data());

			//	ファイルパス
			arguments.push_back(file_path.data());
		}

		//	ファイル読み込み
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> source;
		if (FAILED(library_handle->CreateBlobFromFile(file_path.data(), nullptr, source.GetAddressOf())))
		{
			return	false;
		}

		DxcBuffer source_buffer;
		source_buffer.Ptr		= source->GetBufferPointer();
		source_buffer.Size		= source->GetBufferSize();
		source_buffer.Encoding	= DXC_CP_UTF8;

		//	プリプロセスのみ先に実行
		{
			auto args = arguments;
			args.push_back(L"-P");

			Microsoft::WRL::ComPtr<IDxcResult>	result_handle;
			std::unique_ptr<dxc_include_handler> include_handler = std::make_unique<dxc_include_handler>(dxc_utility.Get());

			if (FAILED(compiler_handle->Compile(&source_buffer,
				args.data(), static_cast<UINT>(arguments.size()),
				include_handler.get(),
				IID_PPV_ARGS(result_handle.GetAddressOf()))))
			{
				return false;
			}
			output_data.include_files.clear();
			for (auto& w : include_handler->List())
			{
				output_data.include_files.push_back(system_utility::convert_utf16_to_shift_jis(w));
			}
		}

		//	読み込んだシェーダーをコンパイル
		Microsoft::WRL::ComPtr<IDxcResult>	result_handle;
		if (FAILED(compiler_handle->Compile(&source_buffer, arguments.data(), static_cast<UINT>(arguments.size()),
											include_handler.Get(), IID_PPV_ARGS(result_handle.GetAddressOf()))))
		{
			return	false;
		}


		//	エラー情報取得
		Microsoft::WRL::ComPtr<IDxcBlobUtf8> error_handle;
		hr	= result_handle->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(error_handle.GetAddressOf()), nullptr);
		assert(SUCCEEDED(hr));
		if(error_handle != nullptr && error_handle->GetStringLength() != 0 )
		{
			output_data.error_message = error_handle->GetStringPointer();
		}

		//	ステータスを取得して失敗しているか判定
		if (FAILED(result_handle->GetStatus(&hr)))
			return false;
		if (FAILED(hr))
			return false;

		//	コンパイル結果を取得
		Microsoft::WRL::ComPtr<IDxcBlob> shader_handle;
		if (FAILED(result_handle->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shader_handle.GetAddressOf()), nullptr)))
			return	false;

		//	コンパイル結果を返す
		output_data.shader_data = static_cast<const char*>(shader_handle->GetBufferPointer());
		output_data.shader_size = shader_handle->GetBufferSize();
		auto internal_state = std::make_shared<Microsoft::WRL::ComPtr<IDxcBlob>>();
		*internal_state = shader_handle;
		output_data.blob = internal_state;

		Microsoft::WRL::ComPtr<IDxcBlob> hash;
		hr = result_handle->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(hash.GetAddressOf()), nullptr);
		assert(SUCCEEDED(hr));
		if (hash != nullptr)
		{
			DxcShaderHash* hash_buffer = static_cast<DxcShaderHash*>(hash->GetBufferPointer() );
			for (int index = 0; index < ARRAYSIZE(hash_buffer->HashDigest); ++index)
			{
				output_data.shader_hash.push_back(hash_buffer->HashDigest[index]);
			}
		}

		return	true;
	}
	bool compile_from_file(const compile_input_file& input_data, compile_output& output_data)
	{
		//	コンパイル済みファイルがあるか調査
		{
			std::filesystem::path compiled_file_path = generate_file_path(input_data);
			if (std::filesystem::exists(compiled_file_path))
			{
				//	時間が一緒であればコンパイル済みファイルを読み込む
				std::filesystem::file_time_type input_write_time = std::filesystem::last_write_time(input_data.file_path);
				std::filesystem::file_time_type compiled_write_time = std::filesystem::last_write_time(compiled_file_path);
				if (input_write_time == compiled_write_time)
				{
					//	メタデータを読込、インクルードファイル時間をチェック
					compiled_file_path.replace_extension(".meta");
					shader_meta_data meta_data;
					meta_data.deserialize(compiled_file_path.string());

					bool is_load_compiled = true;
					for (auto& include_file : meta_data.include_files)
					{
						if (!std::filesystem::exists(include_file.include_file_path) ||
							std::filesystem::last_write_time(include_file.include_file_path) != include_file.last_write_time)
						{
							is_load_compiled = false;
							break;
						}
					}

					if (is_load_compiled)
					{
						//	インクルードファイルを保存しておく
						for (auto& include_file : meta_data.include_files)
						{
							output_data.include_files.emplace_back(include_file.include_file_path);
						}
						compiled_file_path.replace_extension(std::string(compiled_extension));
						std::string path = compiled_file_path.string();
						load_shader_cso cso_data;
						cso_data.file_path = path;
						return load_compiled_file(cso_data.file_path, output_data);
					}
				}
			}
		}
		if (input_data.model <= shader_model::_5_0)
		{
			output_data.compiler_type = shader_compiler_type::fxc;
			if (compile_from_file_fxc(input_data, output_data))
				return true;
		}
		else
		{
			output_data.compiler_type = shader_compiler_type::dxc;
			if (compile_from_file_dxc(input_data, output_data))
				return true;
		}
		return false;
	}

	//	リフレクション
	struct reflection_output
	{
		std::shared_ptr<void> internal_state;
	};

	bool generate_reflection_fxc(std::shared_ptr<void>& blob_ptr, reflection_output& output_data)
	{
		auto blob = std::static_pointer_cast<Microsoft::WRL::ComPtr<ID3DBlob>>(blob_ptr);

		Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflector;
		if (FAILED(D3DReflect((*blob)->GetBufferPointer(), (*blob)->GetBufferSize(),
			IID_ID3D11ShaderReflection, reinterpret_cast<void**>(reflector.GetAddressOf()))))
		{
			return	false;
		}

		auto internal_state = std::make_shared<Microsoft::WRL::ComPtr<ID3D11ShaderReflection>>();
		*internal_state = reflector;
		output_data.internal_state = internal_state;
		return	true;
	}
	bool generate_reflection_dxc(std::shared_ptr<void>& blob_ptr, reflection_output& output_data)
	{
		//	DXC Utils生成
		Microsoft::WRL::ComPtr<IDxcUtils>	dxc_utility;
		if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxc_utility.GetAddressOf()))))
			return	false;

		//	リフレクションコンテナ取得
		Microsoft::WRL::ComPtr<IDxcContainerReflection>	container_reflection;
		if (FAILED(DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(container_reflection.GetAddressOf()))))
			return	false;

		auto blob = std::static_pointer_cast<Microsoft::WRL::ComPtr<IDxcBlob>>(blob_ptr);

		//	DXILチャンクブロックのインデックスを取得
		UINT	shader_index = 0;
		if (FAILED(container_reflection->Load((*blob).Get())))
			return	false;

		if (FAILED(container_reflection->FindFirstPartKind(DXC_PART_DXIL, &shader_index)))
			return	false;

		//	シェーダーリフレクションインターフェース取得
		Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflector;
		if (FAILED(container_reflection->GetPartReflection(shader_index, IID_PPV_ARGS(reflector.GetAddressOf()))))
			return	false;

		auto internal_state = std::make_shared<Microsoft::WRL::ComPtr<ID3D11ShaderReflection>>();
		*internal_state = reflector;
		output_data.internal_state = internal_state;
		return	true;
	}
	bool generate_reflection(shader_compiler_type type, std::shared_ptr<void>& blob_ptr, reflection_output& output_data)
	{
		if (type == shader_compiler_type::fxc)
		{
			return generate_reflection_fxc(blob_ptr, output_data);
		}
		return generate_reflection_dxc(blob_ptr, output_data);
	}
}

bool shader_meta_data::serialize(std::string_view file_path)
{
	std::ofstream ostream(file_path.data(), std::ios::binary);
	if (ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(CEREAL_NVP(include_files));
		}
		catch (...)
		{
			DEBUG_LOG_CATEGORY(logger::categorys::shader, "shader_meta_data serialize failed[%s]", file_path.data());
			return false;
		}

		DEBUG_LOG_CATEGORY(logger::categorys::shader, "shader_meta_data serialize successed[%s]", file_path.data());
		return true;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::shader, "shader_meta_data open failed[%s]", file_path.data());
	return false;
}

bool shader_meta_data::deserialize(std::string_view file_path)
{
	std::ifstream istream(file_path.data(), std::ios::binary);
	if (istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(CEREAL_NVP(include_files));
		}
		catch (...)
		{
			DEBUG_LOG_CATEGORY(logger::categorys::shader, "shader_meta_data deserialize failed[%s]", file_path.data());
			return false;
		}

		DEBUG_LOG_CATEGORY(logger::categorys::shader, "shader_meta_data deserialize successed[%s]", file_path.data());
		return true;
	}
	DEBUG_LOG_CATEGORY(logger::categorys::shader, "shader_meta_data open failed[%s]", file_path.data());
	return false;
}

shader_base::shader_base(load_shader_cso cso_data)
: load_type(shader_load_type::cso)
, cso_data(cso_data)
{
}

shader_base::shader_base(load_shader_file file_data)
: load_type(shader_load_type::file)
, file_data(file_data)
{
}

void shader_base::set_shader_compiler_type(shader_compiler_type type)
{
	compiler_type = type;
}

void shader_base::set_reflection(std::shared_ptr<void> reflection)
{
	internal_reflection = reflection;
}

bool shader_base::load()
{
	bool flag = false;
	switch (load_type)
	{
		case shader_load_type::cso:
		{
			flag = load_from_cso(cso_data);
			break;
		}
		case shader_load_type::file:
		{
			flag = load_from_file(file_data);
			break;
		}
	}

#if defined(USE_IMGUI)
	failed_flag = !flag;
#endif // defined(USE_IMGUI)

	return flag;
}

#if defined(USE_IMGUI)
bool shader_base::is_failed()
{
	return failed_flag;
}

bool shader_base::is_reload()
{
	bool flag = false;

	for (auto& file : monitor_files)
	{
		if (!std::filesystem::exists(file.file_path))
		{
			flag = true;
			break;
		}

		//	最終更新日時と違った場合は再コンパイルが必要か判定
		file_time_type write_time = std::filesystem::last_write_time(file.file_path);
		if (file.last_write_time != write_time)
		{
			flag = true;
			break;
		}
	}

	return flag;
}

void shader_base::reload()
{
	if (load())
	{
		//	登録コールバック実行
		for (auto& data : reload_callbacks)
		{
			data.callback(this);
		}
	}
	else
	{
		//	エラーで何度もコンパイルされるのはまずいので、
		//	モニターファイルの日時を更新しておく
		for (auto& file : monitor_files)
		{
			if (!std::filesystem::exists(file.file_path))
				continue;

			file.last_write_time = std::filesystem::last_write_time(file.file_path);
		}
	}
}

UINT shader_base::register_reload_callback(std::function<void(const shader_base*)> callback)
{
	reload_callbacks.push_back({ callback_id_counter++, callback });
	return reload_callbacks.back().id;
}

void shader_base::unregister_reload_callback(UINT id)
{
	auto it = std::find_if(
		reload_callbacks.begin(),
		reload_callbacks.end(),
		[id](const callback_data& data) { return data.id == id; }
	);
	if (it == reload_callbacks.end())
		return;
	reload_callbacks.erase(it);
}

void shader_base::claer_monitor_files()
{
	monitor_files.clear();
}

void shader_base::register_monitor_file(std::string_view file_path)
{
	auto path = std::filesystem::path(file_path);
	if (!std::filesystem::exists(path))
		return;

	monitor_files.emplace_back();
	monitor_files.back().file_path = path;
	monitor_files.back().last_write_time = std::filesystem::last_write_time(path);
}
#endif	//	defined(USE_IMGUI)

////////////////////////////////////////////////////////////////////////////////////////////////////////
vertex_shader::vertex_shader(load_shader_cso cso_data)
: shader_base(cso_data)
, is_set_input_element_desc(cso_data.input_element_desc.empty() ? false : true)
, input_element_desc(cso_data.input_element_desc)
{
}

vertex_shader::vertex_shader(load_shader_file file_data)
: shader_base(file_data)
, is_set_input_element_desc(file_data.input_element_desc.empty() ? false : true)
, input_element_desc(file_data.input_element_desc)
{
}

vertex_shader::~vertex_shader()
{
}

bool vertex_shader::load_from_cso(load_shader_cso cso_data)
{
	compile_output compile_data;
	if (!load_compiled_file(cso_data.file_path, compile_data))
		return false;

	if (!compile_data.is_valid())
		return false;

	set_shader_compiler_type(compile_data.compiler_type);

	HRESULT hr{ S_OK };
	Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
	hr = render_data::instance()->get_device()->CreateVertexShader(compile_data.shader_data, compile_data.shader_size, nullptr, shader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if(FAILED(hr))
		return false;

	//	リフレクション生成
	reflection_output reflection_data;
	generate_reflection(compile_data.compiler_type, compile_data.blob, reflection_data);

	//	入力レイアウト生成
	if (!is_set_input_element_desc)
	{
		input_element_desc.clear();
		generate_input_layout(reflection_data.internal_state);
	}

	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	if (!input_element_desc.empty())
	{
		hr = render_data::instance()->get_device()->CreateInputLayout(input_element_desc.data(), static_cast<UINT>(input_element_desc.size()),
			compile_data.shader_data, compile_data.shader_size, input_layout.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
		{
			return false;
		}
	}

	this->shader = shader;
	this->input_layout = input_layout;
	set_reflection(reflection_data.internal_state);

#if defined(USE_IMGUI)
	claer_monitor_files();
	register_monitor_file(cso_data.file_path);
	for (auto& path : compile_data.include_files)
	{
		register_monitor_file(path);
	}

	std::string text = std::string("Shader Load Succeeded. [")
								+ std::string(cso_data.file_path)
								+ std::string("]\n")
								+ compile_data.error_message;
	DEBUG_LOG_CATEGORY(logger::categorys::shader, text.data());
#endif	//	defined(USE_IMGUI)

	return true;
}

bool vertex_shader::load_from_file(load_shader_file file_data)
{
	HRESULT hr = S_OK;

	compile_input_file input_data;
	{
		input_data.file_path = file_data.file_path;
		input_data.entry_point = file_data.entry_point;
		input_data.model = file_data.model;
		input_data.defines = file_data.defines;
		input_data.include_directries = file_data.include_directries;
		input_data.compile_flags = file_data.compile_flags;
		input_data.type = shader_type::vertex;
	}

	compile_output compile_data;
	bool compile_succeeded = compile_from_file(input_data, compile_data);

#if defined(USE_IMGUI)
	if (std::filesystem::exists(file_data.file_path))
	{
		//	ファイル監視したいのでここで
		claer_monitor_files();
		register_monitor_file(file_data.file_path);
		for (auto& path : compile_data.include_files)
		{
			register_monitor_file(path);
		}
	}
#endif	//	defined(USE_IMGUI)

	if (!compile_succeeded)
		return false;

	if (!compile_data.is_valid())
		return false;

	set_shader_compiler_type(compile_data.compiler_type);

	Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
	hr = render_data::instance()->get_device()->CreateVertexShader(compile_data.shader_data, compile_data.shader_size, nullptr, shader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return false;

	//	リフレクション生成
	reflection_output reflection_data;
	generate_reflection(compile_data.compiler_type, compile_data.blob, reflection_data);

	//	入力レイアウト生成
	if (!is_set_input_element_desc)
	{
		input_element_desc.clear();
		generate_input_layout(reflection_data.internal_state);
	}

	Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout;
	if (!input_element_desc.empty())
	{
		hr = render_data::instance()->get_device()->CreateInputLayout(input_element_desc.data(), static_cast<UINT>(input_element_desc.size()),
																		compile_data.shader_data, compile_data.shader_size, input_layout.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
		{
			//set_blob(reserve_blob);
			return false;
		}
	}

	this->shader = shader;
	this->input_layout = input_layout;
	set_reflection(reflection_data.internal_state);

#if defined(USE_IMGUI)
	std::string text = std::string("Shader Load Succeeded. [")
								+ std::string(input_data.file_path)
								+ std::string("]\n")
								+ compile_data.error_message;
	DEBUG_LOG_CATEGORY(logger::categorys::shader, text.data());
#endif	//	defined(USE_IMGUI)

	return true;
}

bool vertex_shader::generate_input_layout(std::shared_ptr<void> shader_reflection)
{
	auto reflection_ptr = std::static_pointer_cast<Microsoft::WRL::ComPtr<ID3D11ShaderReflection>>(shader_reflection);
	auto reflection = (*reflection_ptr);

	D3D11_SHADER_DESC shader_desc;
	reflection->GetDesc(&shader_desc);

	//	入力スロット
	std::vector<UINT> input_slots_align_byte_offset(1);
	input_slots_align_byte_offset[0] = 0;
	{
		UINT size = 0;
		for (UINT index = 0; index < shader_desc.InputParameters; index++)
		{
			D3D11_SIGNATURE_PARAMETER_DESC desc;
			reflection->GetInputParameterDesc(index, &desc);

			//	システムセマンティクスは無視する
			if (desc.SystemValueType != D3D_NAME_UNDEFINED)
				continue;
			++size;
		}
		input_element_desc.reserve(size);
	}

	//	設定処理
	UINT input_slot_index = 0;
	UINT previous_semantic_index = 0;
	const char* previous_semantic_name = nullptr;
	for (UINT index = 0; index < shader_desc.InputParameters; index++)
	{
		D3D11_SIGNATURE_PARAMETER_DESC desc;
		reflection->GetInputParameterDesc(index, &desc);

		//	システムセマンティクスは無視する
		if (desc.SystemValueType != D3D_NAME_UNDEFINED)
			continue;

		//	何らかのルールを決めて、インスタンシングで利用するエレメントか判断してこの辺りを変えると良い
		D3D11_INPUT_CLASSIFICATION input_slot_class = D3D11_INPUT_PER_VERTEX_DATA;
		UINT instance_data_step_rate = 0;

		//	入力レイアウト要素設定
		input_element_desc.emplace_back();
		auto& element_desc = input_element_desc.back();
		element_desc.SemanticName			= desc.SemanticName;
		element_desc.SemanticIndex			= desc.SemanticIndex;
		element_desc.InputSlot				= input_slot_index;
		element_desc.AlignedByteOffset		= input_slots_align_byte_offset[input_slot_index];
		element_desc.InputSlotClass			= input_slot_class;
		element_desc.InstanceDataStepRate	= instance_data_step_rate;

		BYTE element_count	= desc.Mask;
		element_count = ( ( element_count & 0xA ) >> 1 ) + ( ( element_count & 0x5 ) );
		element_count = ( ( element_count & 0xC ) >> 2 ) + ( ( element_count & 0x3 ) );
		switch (desc.MinPrecision)
		{
			default:
			case	D3D_MIN_PRECISION_DEFAULT:
			{
				constexpr DXGI_FORMAT FormatList[4][3]	=
				{
					{ DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_SINT, DXGI_FORMAT_R32_FLOAT },
					{ DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32_SINT, DXGI_FORMAT_R32G32_FLOAT },
					{ DXGI_FORMAT_R32G32B32_UINT, DXGI_FORMAT_R32G32B32_SINT, DXGI_FORMAT_R32G32B32_FLOAT },
					{ DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_SINT, DXGI_FORMAT_R32G32B32A32_FLOAT },
				};
				element_desc.Format = FormatList[element_count - 1][desc.ComponentType - 1];
				break;
			}
			case	D3D_MIN_PRECISION_FLOAT_16:
			{
				constexpr	DXGI_FORMAT	FormatList[4]	=
				{
					DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16G16_FLOAT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R16G16B16A16_FLOAT,
				};
				element_desc.Format = FormatList[element_count - 1];
				break;
			}
			case	D3D_MIN_PRECISION_SINT_16:
			{
				constexpr	DXGI_FORMAT	FormatList[4]	=
				{
					DXGI_FORMAT_R16_SINT,
					DXGI_FORMAT_R16G16_SINT,
					DXGI_FORMAT_UNKNOWN,
					DXGI_FORMAT_R16G16B16A16_SINT,
				};
				assert(FormatList[element_count - 1] != DXGI_FORMAT_UNKNOWN);
				element_desc.Format = FormatList[element_count - 1];
				break;
			}
			case	D3D_MIN_PRECISION_UINT_16:
			{
				constexpr	DXGI_FORMAT	FormatList[4]	=
				{
					DXGI_FORMAT_R16_UINT,
					DXGI_FORMAT_R16G16_UINT,
					DXGI_FORMAT_UNKNOWN,
					DXGI_FORMAT_R16G16B16A16_UINT,
				};
				assert(FormatList[element_count - 1] != DXGI_FORMAT_UNKNOWN);
				element_desc.Format = FormatList[element_count - 1];
				break;
			}
			case	D3D_MIN_PRECISION_ANY_16:
			{
				/*	一応これで変換できるとは思うが、そもそも設定できないはず。
				constexpr	DXGI_FORMAT	FormatList[4]	=
				{
					DXGI_FORMAT_R16_TYPELESS,
					DXGI_FORMAT_R16G16_TYPELESS,
					DXGI_FORMAT_UNKNOWN,
					DXGI_FORMAT_R16G16B16A16_TYPELESS,
				};
				assert(FormatList[element_count - 1] != DXGI_FORMAT_UNKNOWN);
				element_desc.Format = FormatList[element_count - 1];
				break;
				*/
			}
			case	D3D_MIN_PRECISION_ANY_10:
			case	D3D_MIN_PRECISION_FLOAT_2_8:
			{
				assert(!L"DXGI_FORMATとしては存在しない" );
				break;
			}
		}

		//	フォーマット毎のオフセット値を設定
		input_slots_align_byte_offset[input_slot_index] += direct_x_helper::get_format_stride(element_desc.Format);

		//	入力スロットの変更
		if (previous_semantic_name == nullptr ||
			strcmp(previous_semantic_name, desc.SemanticName) != 0)
		{
			previous_semantic_name = desc.SemanticName;
			previous_semantic_index = -1;
		}
		if (previous_semantic_index != desc.SemanticIndex)
		{
			previous_semantic_index = desc.SemanticIndex;
			++input_slot_index;
		}

		//	新しいスロット用のバッファを追加
		if( input_slot_index >= input_slots_align_byte_offset.size() )
		{
			input_slots_align_byte_offset.resize(input_slot_index + 1);
			input_slots_align_byte_offset[input_slot_index]	= 0;
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
hull_shader::hull_shader(load_shader_cso cso_data)
: shader_base(cso_data)
{
}

hull_shader::hull_shader(load_shader_file file_data)
: shader_base(file_data)
{
}

hull_shader::~hull_shader()
{
}

bool hull_shader::load_from_cso(load_shader_cso cso_data)
{
	compile_output compile_data;
	if (!load_compiled_file(cso_data.file_path, compile_data))
		return false;

	if (!compile_data.is_valid())
		return false;

	set_shader_compiler_type(compile_data.compiler_type);

	HRESULT hr{ S_OK };
	Microsoft::WRL::ComPtr<ID3D11HullShader> shader;
	hr = render_data::instance()->get_device()->CreateHullShader(compile_data.shader_data, compile_data.shader_size, nullptr, shader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return false;

	//	リフレクション生成
	reflection_output reflection_data;
	generate_reflection(compile_data.compiler_type, compile_data.blob, reflection_data);
	set_reflection(reflection_data.internal_state);

	this->shader = shader;

#if defined(USE_IMGUI)
	claer_monitor_files();
	register_monitor_file(cso_data.file_path);
	for (auto& path : compile_data.include_files)
	{
		register_monitor_file(path);
	}

	std::string text = std::string("Shader Load Succeeded. [")
								+ std::string(cso_data.file_path)
								+ std::string("]\n")
								+ compile_data.error_message;
	DEBUG_LOG_CATEGORY(logger::categorys::shader, text.data());
#endif	//	defined(USE_IMGUI)

	return true;
}

bool hull_shader::load_from_file(load_shader_file file_data)
{
	HRESULT hr = S_OK;

	compile_input_file input_data;
	{
		input_data.file_path = file_data.file_path;
		input_data.entry_point = file_data.entry_point;
		input_data.model = file_data.model;
		input_data.defines = file_data.defines;
		input_data.include_directries = file_data.include_directries;
		input_data.compile_flags = file_data.compile_flags;
		input_data.type = shader_type::hull;
	}

	compile_output compile_data;
	bool compile_succeeded = compile_from_file(input_data, compile_data);

#if defined(USE_IMGUI)
	if (std::filesystem::exists(file_data.file_path))
	{
		//	ファイル監視したいのでここで
		claer_monitor_files();
		register_monitor_file(file_data.file_path);
		for (auto& path : compile_data.include_files)
		{
			register_monitor_file(path);
		}
	}
#endif	//	defined(USE_IMGUI)

	if (!compile_succeeded)
		return false;

	if (!compile_data.is_valid())
		return false;

	set_shader_compiler_type(compile_data.compiler_type);

	Microsoft::WRL::ComPtr<ID3D11HullShader> shader;
	hr = render_data::instance()->get_device()->CreateHullShader(compile_data.shader_data, compile_data.shader_size, nullptr, shader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return false;

	//	リフレクション生成
	reflection_output reflection_data;
	generate_reflection(compile_data.compiler_type, compile_data.blob, reflection_data);
	set_reflection(reflection_data.internal_state);
	this->shader = shader;

#if defined(USE_IMGUI)
	std::string text = std::string("Shader Load Succeeded. [")
								+ std::string(input_data.file_path)
								+ std::string("]\n")
								+ compile_data.error_message;
	DEBUG_LOG_CATEGORY(logger::categorys::shader, text.data());
#endif	//	defined(USE_IMGUI)

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
domain_shader::domain_shader(load_shader_cso cso_data)
: shader_base(cso_data)
{
}

domain_shader::domain_shader(load_shader_file file_data)
: shader_base(file_data)
{
}

domain_shader::~domain_shader()
{
}

bool domain_shader::load_from_cso(load_shader_cso cso_data)
{
	compile_output compile_data;
	if (!load_compiled_file(cso_data.file_path, compile_data))
		return false;

	if (!compile_data.is_valid())
		return false;

	set_shader_compiler_type(compile_data.compiler_type);

	HRESULT hr{ S_OK };
	Microsoft::WRL::ComPtr<ID3D11DomainShader> shader;
	hr = render_data::instance()->get_device()->CreateDomainShader(compile_data.shader_data, compile_data.shader_size, nullptr, shader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return false;

	//	リフレクション生成
	reflection_output reflection_data;
	generate_reflection(compile_data.compiler_type, compile_data.blob, reflection_data);
	set_reflection(reflection_data.internal_state);
	this->shader = shader;

#if defined(USE_IMGUI)
	claer_monitor_files();
	register_monitor_file(cso_data.file_path);
	for (auto& path : compile_data.include_files)
	{
		register_monitor_file(path);
	}

	std::string text = std::string("Shader Load Succeeded. [")
								+ std::string(cso_data.file_path)
								+ std::string("]\n")
								+ compile_data.error_message;
	DEBUG_LOG_CATEGORY(logger::categorys::shader, text.data());
#endif	//	defined(USE_IMGUI)

	return true;
}

bool domain_shader::load_from_file(load_shader_file file_data)
{
	HRESULT hr = S_OK;

	compile_input_file input_data;
	{
		input_data.file_path = file_data.file_path;
		input_data.entry_point = file_data.entry_point;
		input_data.model = file_data.model;
		input_data.defines = file_data.defines;
		input_data.include_directries = file_data.include_directries;
		input_data.compile_flags = file_data.compile_flags;
		input_data.type = shader_type::domain;
	}

	compile_output compile_data;
	bool compile_succeeded = compile_from_file(input_data, compile_data);

#if defined(USE_IMGUI)
	if (std::filesystem::exists(file_data.file_path))
	{
		//	ファイル監視したいのでここで
		claer_monitor_files();
		register_monitor_file(file_data.file_path);
		for (auto& path : compile_data.include_files)
		{
			register_monitor_file(path);
		}
	}
#endif	//	defined(USE_IMGUI)

	if (!compile_succeeded)
		return false;

	if (!compile_data.is_valid())
		return false;

	set_shader_compiler_type(compile_data.compiler_type);

	Microsoft::WRL::ComPtr<ID3D11DomainShader> shader;
	hr = render_data::instance()->get_device()->CreateDomainShader(compile_data.shader_data, compile_data.shader_size, nullptr, shader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return false;

	//	リフレクション生成
	reflection_output reflection_data;
	generate_reflection(compile_data.compiler_type, compile_data.blob, reflection_data);
	set_reflection(reflection_data.internal_state);
	this->shader = shader;

#if defined(USE_IMGUI)
	std::string text = std::string("Shader Load Succeeded. [")
								+ std::string(input_data.file_path)
								+ std::string("]\n")
								+ compile_data.error_message;
	DEBUG_LOG_CATEGORY(logger::categorys::shader, text.data());
#endif	//	defined(USE_IMGUI)

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
geometry_shader::geometry_shader(load_shader_cso cso_data)
: shader_base(cso_data)
{
}

geometry_shader::geometry_shader(load_shader_file file_data)
: shader_base(file_data)
{
}

geometry_shader::~geometry_shader()
{
}

bool geometry_shader::load_from_cso(load_shader_cso cso_data)
{
	compile_output compile_data;
	if (!load_compiled_file(cso_data.file_path, compile_data))
		return false;

	if (!compile_data.is_valid())
		return false;

	set_shader_compiler_type(compile_data.compiler_type);

	HRESULT hr{ S_OK };
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> shader;
	hr = render_data::instance()->get_device()->CreateGeometryShader(compile_data.shader_data, compile_data.shader_size, nullptr, shader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return false;

	//	リフレクション生成
	reflection_output reflection_data;
	generate_reflection(compile_data.compiler_type, compile_data.blob, reflection_data);
	set_reflection(reflection_data.internal_state);
	this->shader = shader;

#if defined(USE_IMGUI)
	claer_monitor_files();
	register_monitor_file(cso_data.file_path);
	for (auto& path : compile_data.include_files)
	{
		register_monitor_file(path);
	}

	std::string text = std::string("Shader Load Succeeded. [")
								+ std::string(cso_data.file_path)
								+ std::string("]\n")
								+ compile_data.error_message;
	DEBUG_LOG_CATEGORY(logger::categorys::shader, text.data());
#endif	//	defined(USE_IMGUI)

	return true;
}

bool geometry_shader::load_from_file(load_shader_file file_data)
{
	HRESULT hr = S_OK;

	compile_input_file input_data;
	{
		input_data.file_path = file_data.file_path;
		input_data.entry_point = file_data.entry_point;
		input_data.model = file_data.model;
		input_data.defines = file_data.defines;
		input_data.include_directries = file_data.include_directries;
		input_data.compile_flags = file_data.compile_flags;
		input_data.type = shader_type::geometry;
	}

	compile_output compile_data;
	bool compile_succeeded = compile_from_file(input_data, compile_data);

#if defined(USE_IMGUI)
	if (std::filesystem::exists(file_data.file_path))
	{
		//	ファイル監視したいのでここで
		claer_monitor_files();
		register_monitor_file(file_data.file_path);
		for (auto& path : compile_data.include_files)
		{
			register_monitor_file(path);
		}
	}
#endif	//	defined(USE_IMGUI)

	if (!compile_succeeded)
		return false;

	if (!compile_data.is_valid())
		return false;

	set_shader_compiler_type(compile_data.compiler_type);

	Microsoft::WRL::ComPtr<ID3D11GeometryShader> shader;
	hr = render_data::instance()->get_device()->CreateGeometryShader(compile_data.shader_data, compile_data.shader_size, nullptr, shader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return false;

	//	リフレクション生成
	reflection_output reflection_data;
	generate_reflection(compile_data.compiler_type, compile_data.blob, reflection_data);
	set_reflection(reflection_data.internal_state);
	this->shader = shader;

#if defined(USE_IMGUI)
	std::string text = std::string("Shader Load Succeeded. [")
								+ std::string(input_data.file_path)
								+ std::string("]\n")
								+ compile_data.error_message;
	DEBUG_LOG_CATEGORY(logger::categorys::shader, text.data());
#endif	//	defined(USE_IMGUI)

	return true;
}

#if 0
HRESULT create_gs_with_streamout_from_cso(ID3D11Device* device, const char* cso_name, ID3D11GeometryShader** geometry_shader, D3D11_SO_DECLARATION_ENTRY* streamout_declaration_desc, UINT num_entry, UINT* buffer_strides, UINT num_strides, UINT rasterized_stream)
{
	FILE* fp{ nullptr };
	fopen_s(&fp, cso_name, "rb");
	_ASSERT_EXPR_A(fp, "CSO File not found");

	fseek(fp, 0, SEEK_END);
	long cso_sz{ ftell(fp) };
	fseek(fp, 0, SEEK_SET);

	unique_ptr<unsigned char[]> buffer{ make_unique<unsigned char[]>(cso_sz) };
	fread(buffer.get(), cso_sz, 1, fp);
	fclose(fp);

	HRESULT hr{ S_OK };
	hr = device->CreateGeometryShaderWithStreamOutput(buffer.get(), cso_sz, streamout_declaration_desc, num_entry, buffer_strides, num_strides, rasterized_stream, nullptr, geometry_shader);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	return hr;
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////
pixel_shader::pixel_shader(load_shader_cso cso_data)
: shader_base(cso_data)
{
}

pixel_shader::pixel_shader(load_shader_file file_data)
: shader_base(file_data)
{
}

pixel_shader::~pixel_shader()
{
}

bool pixel_shader::load_from_cso(load_shader_cso cso_data)
{
	compile_output compile_data;
	if (!load_compiled_file(cso_data.file_path, compile_data))
		return false;

	if (!compile_data.is_valid())
		return false;

	set_shader_compiler_type(compile_data.compiler_type);

	HRESULT hr{ S_OK };
	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
	hr = render_data::instance()->get_device()->CreatePixelShader(compile_data.shader_data, compile_data.shader_size, nullptr, shader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return false;

	//	リフレクション生成
	reflection_output reflection_data;
	generate_reflection(compile_data.compiler_type, compile_data.blob, reflection_data);
	set_reflection(reflection_data.internal_state);
	this->shader = shader;

#if defined(USE_IMGUI)
	claer_monitor_files();
	register_monitor_file(cso_data.file_path);
	for (auto& path : compile_data.include_files)
	{
		register_monitor_file(path);
	}

	std::string text = std::string("Shader Load Succeeded. [")
								+ std::string(cso_data.file_path)
								+ std::string("]\n")
								+ compile_data.error_message;
	DEBUG_LOG_CATEGORY(logger::categorys::shader, text.data());
#endif	//	defined(USE_IMGUI)

	return true;
}

bool pixel_shader::load_from_file(load_shader_file file_data)
{
	HRESULT hr = S_OK;

	compile_input_file input_data;
	{
		input_data.file_path = file_data.file_path;
		input_data.entry_point = file_data.entry_point;
		input_data.model = file_data.model;
		input_data.defines = file_data.defines;
		input_data.include_directries = file_data.include_directries;
		input_data.compile_flags = file_data.compile_flags;
		input_data.type = shader_type::pixel;
	}

	compile_output compile_data;
	bool compile_succeeded = compile_from_file(input_data, compile_data);

#if defined(USE_IMGUI)
	if (std::filesystem::exists(file_data.file_path))
	{
		//	ファイル監視したいのでここで
		claer_monitor_files();
		register_monitor_file(file_data.file_path);
		for (auto& path : compile_data.include_files)
		{
			register_monitor_file(path);
		}
	}
#endif	//	defined(USE_IMGUI)

	if (!compile_succeeded)
		return false;

	if (!compile_data.is_valid())
		return false;

	set_shader_compiler_type(compile_data.compiler_type);

	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
	hr = render_data::instance()->get_device()->CreatePixelShader(compile_data.shader_data, compile_data.shader_size, nullptr, shader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return false;

	//	リフレクション生成
	reflection_output reflection_data;
	generate_reflection(compile_data.compiler_type, compile_data.blob, reflection_data);
	set_reflection(reflection_data.internal_state);
	this->shader = shader;

#if defined(USE_IMGUI)
	std::string text = std::string("Shader Load Succeeded. [")
		+ std::string(input_data.file_path)
		+ std::string("]\n")
		+ compile_data.error_message;
	DEBUG_LOG_CATEGORY(logger::categorys::shader, text.data());
#endif	//	defined(USE_IMGUI)

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
compute_shader::compute_shader(load_shader_cso cso_data)
: shader_base(cso_data)
{
}

compute_shader::compute_shader(load_shader_file file_data)
: shader_base(file_data)
{
}


compute_shader::~compute_shader()
{
}

bool compute_shader::load_from_cso(load_shader_cso cso_data)
{
	compile_output compile_data;
	if (!load_compiled_file(cso_data.file_path, compile_data))
		return false;

	if (!compile_data.is_valid())
		return false;

	set_shader_compiler_type(compile_data.compiler_type);

	HRESULT hr{ S_OK };
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader;
	hr = render_data::instance()->get_device()->CreateComputeShader(compile_data.shader_data, compile_data.shader_size, nullptr, shader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return false;

	//	リフレクション生成
	reflection_output reflection_data;
	generate_reflection(compile_data.compiler_type, compile_data.blob, reflection_data);
	set_reflection(reflection_data.internal_state);
	this->shader = shader;

#if defined(USE_IMGUI)
	claer_monitor_files();
	register_monitor_file(cso_data.file_path);
	for (auto& path : compile_data.include_files)
	{
		register_monitor_file(path);
	}

	std::string text = std::string("Shader Load Succeeded. [")
								+ std::string(cso_data.file_path)
								+ std::string("]\n")
								+ compile_data.error_message;
	DEBUG_LOG_CATEGORY(logger::categorys::shader, text.data());
#endif	//	defined(USE_IMGUI)

	return true;
}

bool compute_shader::load_from_file(load_shader_file file_data)
{
	HRESULT hr = S_OK;

	compile_input_file input_data;
	{
		input_data.file_path = file_data.file_path;
		input_data.entry_point = file_data.entry_point;
		input_data.model = file_data.model;
		input_data.defines = file_data.defines;
		input_data.include_directries = file_data.include_directries;
		input_data.compile_flags = file_data.compile_flags;
		input_data.type = shader_type::compute;
	}

	compile_output compile_data;
	bool compile_succeeded = compile_from_file(input_data, compile_data);

#if defined(USE_IMGUI)
	if (std::filesystem::exists(file_data.file_path))
	{
		//	ファイル監視したいのでここで
		claer_monitor_files();
		register_monitor_file(file_data.file_path);
		for (auto& path : compile_data.include_files)
		{
			register_monitor_file(path);
		}
	}
#endif	//	defined(USE_IMGUI)

	if (!compile_succeeded)
		return false;

	if (!compile_data.is_valid())
		return false;

	set_shader_compiler_type(compile_data.compiler_type);

	Microsoft::WRL::ComPtr<ID3D11ComputeShader> shader;
	hr = render_data::instance()->get_device()->CreateComputeShader(compile_data.shader_data, compile_data.shader_size, nullptr, shader.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	if (FAILED(hr))
		return false;

	//	リフレクション生成
	reflection_output reflection_data;
	generate_reflection(compile_data.compiler_type, compile_data.blob, reflection_data);
	set_reflection(reflection_data.internal_state);
	this->shader = shader;

#if defined(USE_IMGUI)
	std::string text = std::string("Shader Load Succeeded. [")
								+ std::string(input_data.file_path)
								+ std::string("]\n")
								+ compile_data.error_message;
	DEBUG_LOG_CATEGORY(logger::categorys::shader, text.data());
#endif	//	defined(USE_IMGUI)

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
bool shader_manager::load_shader(shader_type type, shader_ptr& out_shader, load_shader_cso data)
{
	auto key_hash = generate_hash_key(data);
	auto it = shaderes.find(key_hash);
	if (it != shaderes.end())
	{
		if (auto ptr = it->second.lock())
		{
			out_shader = ptr;
			return	true;
		}
		shaderes.erase(it);
	}

	switch(type)
	{
		case shader_type::vertex:
		{
			out_shader	= std::make_shared<vertex_shader>(data);
			break;
		}
		case shader_type::hull:
		{
			out_shader	= std::make_shared<hull_shader>(data);
			break;
		}
		case shader_type::domain:
		{
			out_shader	= std::make_shared<domain_shader>(data);
			break;
		}
		case shader_type::geometry:
		{
			out_shader	= std::make_shared<geometry_shader>(data);
			break;
		}
		case shader_type::pixel:
		{
			out_shader	= std::make_shared<pixel_shader>(data);
			break;
		}
		case shader_type::compute:
		{
			out_shader	= std::make_shared<compute_shader>(data);
			break;
		}
	}
	//	シェーダー読み込み処理
	if (out_shader && out_shader->load())
	{
		shaderes.insert(std::make_pair(key_hash, std::weak_ptr<shader_base>(out_shader)));
		return true;
	}

	out_shader	= nullptr;
	return false;
}

bool shader_manager::load_shader(shader_type type, shader_ptr& out_shader, load_shader_file data)
{
	//	キー生成
	auto key_hash = generate_hash_key(data);

	auto it = shaderes.find(key_hash);
	if (it != shaderes.end())
	{
		if (auto ptr = it->second.lock())
		{
			out_shader = ptr;
			return	true;
		}
		else
		{
			shaderes.erase(it);
		}
	}

	switch (type)
	{
		case shader_type::vertex:
		{
			out_shader = std::make_shared<vertex_shader>(data);
			break;
		}
		case shader_type::hull:
		{
			out_shader = std::make_shared<hull_shader>(data);
			break;
		}
		case shader_type::domain:
		{
			out_shader = std::make_shared<domain_shader>(data);
			break;
		}
		case shader_type::geometry:
		{
			out_shader = std::make_shared<geometry_shader>(data);
			break;
		}
		case shader_type::pixel:
		{
			out_shader = std::make_shared<pixel_shader>(data);
			break;
		}
		case shader_type::compute:
		{
			out_shader = std::make_shared<compute_shader>(data);
			break;
		}
	}

	//	シェーダー読み込み処理
	bool flag = out_shader && out_shader->load();

#if defined(USE_IMGUI)
	if (!flag)
	{
		//	一旦成功扱いで
		flag = true;
	}
#endif	//	defined(USE_IMGUI)

	if (flag)
	{
		shaderes.insert(std::make_pair(key_hash, std::weak_ptr<shader_base>(out_shader)));
		return true;
	}

	out_shader = nullptr;
	return false;
}

void shader_manager::check_shader_update_and_recompile()
{
#if defined(USE_IMGUI)
	//	テキトーに待機
	if (++check_counter < 60)
		return;
	check_counter = 0;

	//	ファイル日時更新チェック
	auto it = shaderes.begin();
	while (it != shaderes.end())
	{
		auto ptr = it->second.lock();
		if (!ptr)
		{
			it = shaderes.erase(it);
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
