//	参考文献
//	https://software.intel.com/en-us/articles/fast-fourier-transform-for-image-processing-in-directx-11
//	https://www.isus.jp/games/fft-for-image-processing-in-directx-11/

//	TRANSFORM_INVERSE : 逆フーリエ変換かどうか
//	LENGTH : テクスチャの各方向の長さ。2の累乗で指定する事
//	ROW_PASS : X方向パス、1パス目
//	COL_PASS : Y方向パス、2パス目

#if	LENGTH <= 512
// サイズが一定以下用の高速化フラグ
#define	DOUBLE_BUFFER
#endif	//	LENGTH <= 512

Texture2D<float3> source_texture_real : register(t0);
Texture2D<float3> source_texture_imaginary : register(t1);
RWTexture2D<float3> destination_texture_real : register(u0);
RWTexture2D<float3> destination_texture_imaginary : register(u1);

// バタフライ演算用の計算バッファ
#if	defined(DOUBLE_BUFFER)
groupshared float3 ping_pong_array[2][2][LENGTH];
#define	GetPingPongBuffer(temp_index, x, index) ping_pong_array[temp_index][index][x]
#else	//	defined(DOUBLE_BUFFER)
groupshared float3 ping_pong_array[2][LENGTH];
#define	GetPingPongBuffer(temp_index, x, index) ping_pong_array[index][x]
#endif	//	defined(DOUBLE_BUFFER)

static const uint real_part = 0; // 複素数の実数部
static const uint imaginary_part = 1; // 複素数の虚数部
static const uint butterfly_count = (uint)(log(LENGTH) / log(2.0f));
static const float PI = 3.1415926f;

//////////////////////////////////////////////////////////////////////////////////////////////////////
//	バタフライ演算に必要な重みとインデックスを取得
void GetButterflyValues(uint pass_index, uint x, out uint2 out_indices, out float2 weights)
{
	int section_width = 2 << pass_index;
	int half_section_width = (int)((float)(section_width + 0.5f) * 0.5f);

	int sectionStartOffset = x & ~(section_width - 1);
	int halfSectionOffset = x & (half_section_width - 1);
	int sectionOffset = x & (section_width - 1);

	sincos(2.0f * PI * sectionOffset / (float)section_width, weights.y, weights.x);
	weights.y = -weights.y;

	out_indices.x = sectionStartOffset + halfSectionOffset;
	out_indices.y = sectionStartOffset + halfSectionOffset + half_section_width;

	if (pass_index == 0)
		out_indices	= reversebits(out_indices) >> (32 - butterfly_count) & (LENGTH - 1);
}

//	バタフライ演算
void ButterflyPass( int pass_index, uint x, uint temp_index, out float3 result_real, out float3 result_imaginary )
{
	//	重みとインデックスの算出
	uint2 indices;
	float2 weights;
	GetButterflyValues(pass_index, x, indices, weights);

	//	入力値の取得
	float3 input_real1 = GetPingPongBuffer(temp_index, indices.x, real_part);
	float3 input_imaginary1 = GetPingPongBuffer(temp_index, indices.x, imaginary_part);
	float3 input_real2 = GetPingPongBuffer(temp_index, indices.y, real_part);
	float3 input_imaginary2 = GetPingPongBuffer(temp_index, indices.y, imaginary_part);

#if	!defined(DOUBLE_BUFFER)
    //	ダブルバッファを持っていない場合は待機
	GroupMemoryBarrierWithGroupSync();
#endif	//	!defined(DOUBLE_BUFFER)

	//	加算
#if	defined(TRANSFORM_INVERSE)
	result_real = (input_real1 + weights.x * input_real2 + weights.y * input_imaginary2) * 0.5f;
	result_imaginary = (input_imaginary1 - weights.y * input_real2 + weights.x * input_imaginary2) * 0.5f;
#else	//	defined(TRANSFORM_INVERSE)
	result_real = input_real1 + weights.x * input_real2 - weights.y * input_imaginary2;
	result_imaginary = input_imaginary1 + weights.y * input_real2 + weights.x * input_imaginary2;
#endif	//	defined(TRANSFORM_INVERSE)
}

//	逆フーリエ変換出力用のバタフライ演算パス
void InverseButterflyPassRealPart(int pass_index, uint x, uint temp_index, out float3 result_real)
{
	//	重みとインデックスの算出
	uint2 indices;
	float2 weights;
	GetButterflyValues(pass_index, x, indices, weights);

	//	入力値の取得
	float3 input_real1 = GetPingPongBuffer(temp_index, indices.x, real_part);
	float3 input_real2 = GetPingPongBuffer(temp_index, indices.y, real_part);
	float3 input_imaginary2 = GetPingPongBuffer(temp_index, indices.y, imaginary_part);

	result_real	= (input_real1 + weights.x * input_real2 + weights.y * input_imaginary2) * 0.5f;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
[numthreads(LENGTH, 1, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
	const uint buffer_index = dispatch_thread_id.x;

	//	フェッチ座標
#if defined(ROW_PASS)
	uint2 texture_position = uint2(dispatch_thread_id.xy);
#else // defined(ROW_PASS)
	uint2 texture_position = uint2(dispatch_thread_id.yx);
#endif // defined(ROW_PASS)

	//	中心を周波数空間原点とする処理
	texture_position = (texture_position + LENGTH / 2) % LENGTH;

	//	入力テクスチャを共有バッファに格納
	GetPingPongBuffer(0, buffer_index, real_part) = source_texture_real[texture_position];
#if defined(ROW_PASS) && !defined(TRANSFORM_INVERSE)
	GetPingPongBuffer(0, buffer_index, imaginary_part) = 0.0f; // FFT入力は実部のみ
#else // defined(ROW_PASS) && !defined(TRANSFORM_INVERSE)
	GetPingPongBuffer(0, buffer_index, imaginary_part) = source_texture_imaginary[texture_position];
#endif	//	defined(ROW_PASS) && !defined(TRANSFORM_INVERSE)

	//	バタフライ演算
	for (uint index = 0; index < butterfly_count - 1; ++index)
	{
		GroupMemoryBarrierWithGroupSync();
		const uint source_temp_index = index % 2;
		const uint destination_temp_index = (index + 1) % 2;
		ButterflyPass(index, buffer_index, source_temp_index
					 , GetPingPongBuffer(destination_temp_index, buffer_index, real_part)
					 , GetPingPongBuffer(destination_temp_index, buffer_index, imaginary_part));
	}

	GroupMemoryBarrierWithGroupSync();

	const uint source_temp_index = (butterfly_count - 1) %  2;
#if	defined(COL_PASS) && defined(TRANSFORM_INVERSE)
	InverseButterflyPassRealPart(butterfly_count - 1, buffer_index, source_temp_index, destination_texture_real[texture_position] );
#else	//	defined(COL_PASS) && defined(TRANSFORM_INVERSE)
	ButterflyPass(butterfly_count - 1, buffer_index, source_temp_index, destination_texture_real[texture_position], destination_texture_imaginary[texture_position]);
#endif	//	defined(COL_PASS) && defined(TRANSFORM_INVERSE)
}
