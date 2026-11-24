#pragma	once

#include <chrono>

//	エンジン用の設定情報
struct engine_config_data
{
	SINGLETON(engine_config_data)

public:
	//	ウィンドウズ関係の情報
	struct windows_data
	{
		LONG window_left = 0;
		LONG window_top = 0;
		//	スクリーンサイズは決めない

		bool vertical_sync = false;	//	垂直同期

		template<class Archive>
		void serialize(Archive& archive, int version);
	} windows;

	//	アンチエイリアス関係の情報
	struct antialias_data
	{
		enum class modes
		{
			NONE,
			FXAA,
			SMAA,
		};
		modes mode = modes::SMAA;

		template<class Archive>
		void serialize(Archive& archive, int version);
	} antialias;

private:
	engine_config_data();
	~engine_config_data();
	engine_config_data(const engine_config_data&) = delete;
	engine_config_data(engine_config_data&&) = delete;
};
