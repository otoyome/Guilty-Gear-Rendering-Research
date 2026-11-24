#include "depth_of_field.hlsli"

RWStructuredBuffer<int4> integrate_buffer : register(u0);
RWStructuredBuffer<int4> integrate_transpose_buffer : register(u1);

int4 read_from_buffer(int2 location)
{
    const uint offset = calaculate_buffer_offset(location);
    return integrate_buffer[offset];
}

uint calculate_transposed_offset(int2 location)
{
    return ((location.x * buffer_resolution.y) + location.y);
}
void write_to_buffer_transposed(int2 location, int4 color)
{
    const int offset = calculate_transposed_offset(location);
    integrate_transpose_buffer[offset] = color;
}

[numthreads(64, 1, 1)]
void main(int3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= buffer_resolution.x)
        return;
        
    int2 location = int2(dispatch_thread_id.x, 0);

    //  ‰Šú‰»
    int4 delta = read_from_buffer(location);
    int4 color = delta;

    int chunk_end = buffer_resolution.y;

    //  ‚’¼—İÏ˜a
    [loop] for (int index = 1; index < chunk_end; ++index)
    {
        location = int2(dispatch_thread_id.x, index);

        delta += read_from_buffer(location);

        color += delta;

        write_to_buffer_transposed(location, color);
    }
}
