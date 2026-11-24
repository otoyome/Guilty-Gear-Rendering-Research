#include "screen_space_reflection.hlsli"

#include "common_gbuffer_loader.hlsli"

//  NDC空間座標＞UV空間座標に変換
float4 convert_ndcspaceposition_to_uvspaceposition(float4 ndc)
{
    float2 uv = ndc.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    return float4(uv, ndc.z, ndc.w);
}

//  UV空間座標＞NDC空間座標に変換
float4 convert_uvspaceposition_to_ndcspaceposition(float2 uv)
{
    float projection_dpeth = convert_linear_depth_to_projection_depth(get_gbuffer_data(uv).linear_depth, z_buffer_parameteres);
    return float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), projection_dpeth, 1);
}

//  UV空間座標＞視線空間座標に変換
float4 convert_uvspaceposition_to_viewspaceposition(float2 uv)
{
    float4 ndc = convert_uvspaceposition_to_ndcspaceposition(uv);
    float4 position = mul(ndc, inverse_projection_transform);
    return position / position.w;
}

float4 main(VS_OUT pin) : SV_TARGET
{
    GBufferData data = get_gbuffer_data(pin.texcoord.xy);

    if (data.shading_model == shading_model_unlit || data.metalness <= 0.01f)
    {
        return float4(0, 0, 0, 0);
    }

    //  視線空間でのレイの開始位置・終了位置を求める
    float4 viewspace_position = mul(float4(data.w_position, 1), view_transform);
    viewspace_position /= viewspace_position.w;
    float4 viewspace_normal = normalize(mul(float4(data.w_normal, 0), view_transform));
    float3 viewspace_ray = normalize(viewspace_position.xyz);
    float3 viewspace_reflect_ray = normalize(reflect(viewspace_ray, viewspace_normal.xyz));
    float4 viewspace_ray_origin = float4(viewspace_position.xyz, 1.0f);
    float ray_length = ((viewspace_ray_origin.z + viewspace_reflect_ray.z * ray_max_distance) < camera_clip_distance.x)
                        ? (camera_clip_distance.x - viewspace_ray_origin.z) / viewspace_reflect_ray.z
                        : ray_max_distance;
    
    float4 viewspace_ray_end = float4(viewspace_position.xyz + viewspace_reflect_ray * ray_length, 1.0f);

    //  厚み
    float tickness = ray_tickness.x;
    float ray_adjust_rate = 1.0f - saturate(dot(viewspace_normal.xyz, viewspace_reflect_ray.xyz));

    //  反射ベクトルが水平なほどピクセル単位での深度変化が大きくなるので角度に応じて厚みを調整する
    float pixel_ray_resolution = saturate(ray_resolution);
    if (is_flag(ray_flags_reflection_dir_adjust))
    {
        float tickness_rate = ray_adjust_rate <= 0.00001f ? 0 : pow(2, 25 * (ray_adjust_rate - 1.0f));
        tickness = lerp(ray_tickness.x, ray_tickness.y, tickness_rate);
        pixel_ray_resolution = saturate(ray_resolution * (1.0f + ray_adjust_rate));
    }
    
    //  UV空間でのレイの開始位置及び、終端位置を求める
    float4 clipspace_ray_origin = mul(viewspace_ray_origin, projection_transform);
    float4 clipspace_ray_end = mul(viewspace_ray_end, projection_transform);

    float2 uvspace_ray_origin = convert_ndcspaceposition_to_uvspaceposition(clipspace_ray_origin / clipspace_ray_origin.w).xy;
    float2 uvspace_ray_end = convert_ndcspaceposition_to_uvspaceposition(clipspace_ray_end / clipspace_ray_end.w).xy;

    //  スクリーン空間に変換
    float2 texturespace_ray_origin = uvspace_ray_origin * viewport_size.xy;
    float2 texturespace_ray_end = uvspace_ray_end * viewport_size.xy;
    float2 texturespace_ray_delta = texturespace_ray_end - texturespace_ray_origin;

    //  イテレーションする際、どちらかの軸をピクセル単位で移動させたいので基準方向を変える
    float use_axis_x = abs(texturespace_ray_delta.x) >= abs(texturespace_ray_delta.y) ? 1 : 0;
    float axis_delta = lerp(abs(texturespace_ray_delta.y), abs(texturespace_ray_delta.x), use_axis_x) * pixel_ray_resolution;
    float2 texturespace_ray_step = texturespace_ray_delta.xy / axis_delta;
    
    //  UV空間上でレイを更新させて大まかな遮蔽位置を求める
    int increment_count = 0;
    float viewspace_ray_current_distance = viewspace_ray_origin.z; //  視線空間でのレイの深度
    float viewspace_diff_depth; //  視線空間上での深度差分
    float4 viewspace_current_ray_position; //  視線空間での現在のレイの座標

    float2 texturespace_current_ray_position = texturespace_ray_origin.xy;
    float2 hit_uv = texturespace_current_ray_position * viewport_size.zw; //  遮蔽位置のUVを記録

    //  レイの深度判定の細分化
    int subdivision_step_count = 1;
    if (is_flag(ray_flags_substep_check))
    {
        //  水平に近いほど細分化
        subdivision_step_count = max(1, (int) (ray_adjust_rate * ray_adjust_rate * (float) ray_substep_iteration_max));
    }

    int hit_flag0 = 0, hit_flag1 = 0; //  遮蔽したかどうか
    float search_0 = 0, search_1 = 0; //  レイのステップ進捗状態を保存
    [loop]
    for (increment_count = 0; increment_count < int(axis_delta); ++increment_count)
    {
        //  レイを進める
        float step_rate = 1.0f;
        if (is_flag(ray_flags_step_random))
        {
            step_rate += random(texturespace_current_ray_position + time_data.xy) * 0.5f;
        }
        texturespace_current_ray_position += texturespace_ray_step * step_rate;

        //  遮蔽位置のUVを求める
        hit_uv.xy = texturespace_current_ray_position * viewport_size.zw;

        //  範囲外は除外
        if (saturate(hit_uv.x) != hit_uv.x || saturate(hit_uv.y) != hit_uv.y)
            return float4(0, 0, 0, 0);

        //  視線空間上の座標を求める
        viewspace_current_ray_position = convert_uvspaceposition_to_viewspaceposition(hit_uv.xy);

        //  レイの進捗を算出
        float2 ray_step = (texturespace_current_ray_position.xy - texturespace_ray_origin.xy) / texturespace_ray_delta.xy;
        search_1 = saturate(use_axis_x ? ray_step.x : ray_step.y);

        //  深度判定の細分化
        float viewspace_ray_previous_distance = 0;
        float search_delta = (search_1 - search_0) / (float) subdivision_step_count;
        [loop] for (int i = 0; i <= subdivision_step_count; ++i)
        {
            //  透視補正補間
		    // https://www.comp.nus.edu.sg/~lowkl/publications/lowk_persp_interp_techrep.pdf
            viewspace_ray_current_distance = (viewspace_ray_origin.z * viewspace_ray_end.z) / lerp(viewspace_ray_end.z, viewspace_ray_origin.z, search_0 + search_delta * (float) i);

            //  遮蔽判定
            viewspace_diff_depth = viewspace_ray_current_distance - viewspace_current_ray_position.z;
            if (viewspace_diff_depth > 0 && viewspace_diff_depth < tickness)  //  一定範囲内なら遮蔽と判断
            {
                hit_flag0 = 1;
                break;
            }
        }
        if (hit_flag0)
            break;
        
        //  精細に検索する際に必要なため古い進捗状況を記録
        search_0 = search_1;
            
        viewspace_ray_previous_distance = viewspace_ray_current_distance;
    }

    //  大まかに検索したので、今度は詳細に検索
    search_1 = search_0 + ((search_1 - search_0) / 2.0f); //  開始位置は中間地点から前後に検索していく
    static const int secondary_step_count = 10;
    int secondary_steps = secondary_step_count * hit_flag0;
    [loop]
    for (increment_count = 0; increment_count < secondary_steps; ++increment_count)
    {
        //  ヒット位置を算出
        texturespace_current_ray_position = lerp(texturespace_ray_origin.xy, texturespace_ray_end.xy, search_1);
        hit_uv.xy = texturespace_current_ray_position * viewport_size.zw;

        //  範囲外は除外
        if (saturate(hit_uv.x) != hit_uv.x || saturate(hit_uv.y) != hit_uv.y)
        {
            return float4(0, 0, 0, 0);
        }

        viewspace_current_ray_position = convert_uvspaceposition_to_viewspaceposition(hit_uv.xy);
        viewspace_ray_current_distance = (viewspace_ray_origin.z * viewspace_ray_end.z) / lerp(viewspace_ray_end.z, viewspace_ray_origin.z, search_1);

        //  遮蔽判定
        viewspace_diff_depth = viewspace_ray_current_distance - viewspace_current_ray_position.z;
        if (viewspace_diff_depth > 0 && viewspace_diff_depth < tickness)  //  一定範囲内なら遮蔽と判断
        {
            //  今回の位置が遮蔽されているので、遮蔽されていない方向にレイを進める
            hit_flag1 = 1;
            search_1 = search_0 + ((search_1 - search_0) / 2);
        }
        else
        {
            //  今回の位置が遮蔽されていないので、遮蔽されている方向にレイを進める
            float temp = search_1;
            search_1 = search_1 + ((search_1 - search_0) / 2);
            search_0 = temp;
        }
    }

    //  処理ピクセルにて、レイが見えているかどうかを決める
    float alpha = hit_flag1;

    //  反射方向がカメラへの方向に近づくにつれてフェード
    alpha *= (1 - max(dot(-viewspace_ray, viewspace_reflect_ray), 0));

    //  深度差分と厚みからフェード
    alpha *= (1 - saturate(viewspace_diff_depth / tickness));

    //  あまり遠い位置の反射情報は目立たないようにしておく
    float reflection_length = length(viewspace_current_ray_position - viewspace_ray_origin);
    alpha *= (1 - saturate(reflection_length / ray_max_distance));

    //  hit_uvがサンプリング最大範囲に近づくにつれて透過させていく
    static const float screen_space_reflection_border = 0.2f;
    alpha *= smoothstep(1.0f, 1.0f - screen_space_reflection_border, saturate(2.0f * abs(hit_uv.x - 0.5f)));
    alpha *= smoothstep(1.0f, 1.0f - screen_space_reflection_border, saturate(2.0f * abs(hit_uv.y - 0.5f)));

    return float4(hit_uv.xy, reflection_length, saturate(alpha));
}
