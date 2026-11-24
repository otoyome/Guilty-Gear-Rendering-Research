#include "graphics/gpu_timer.h"

gpu_timer::gpu_timer(Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context)
: device_context(device_context)
{
    D3D11_QUERY_DESC query_desc;
    query_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    query_desc.MiscFlags = 0;

    if (FAILED(render_data::instance()->get_device()->CreateQuery(&query_desc, disjoint_query.GetAddressOf())))
    {
        DEBUG_ERROR("Failed to create a timer query.");
    }

    query_desc.Query = D3D11_QUERY_TIMESTAMP;

    if (FAILED(render_data::instance()->get_device()->CreateQuery(&query_desc, begin_query.GetAddressOf())))
    {
        DEBUG_ERROR("Failed to create a begin timer query.");
    }

    if (FAILED(render_data::instance()->get_device()->CreateQuery(&query_desc, end_query.GetAddressOf())))
    {
        DEBUG_ERROR("Failed to create a end timer query.");
    }
}

gpu_timer::~gpu_timer()
{
}

void gpu_timer::begin()
{
    if (!finalized)
        return;

    device_context->Begin(disjoint_query.Get());
    device_context->End(begin_query.Get());

    end_called = false;
}

void gpu_timer::end()
{
    if (end_called)
        return;

    device_context->End(end_query.Get());
    device_context->End(disjoint_query.Get());

    end_called = true;
    finalized = false;
}

float gpu_timer::get_result(bool flush)
{
    if (!finalized)
    {
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint_data;
        UINT64 time_start, time_end;
        if (flush)
        {
            if (device_context->GetData(disjoint_query.Get(), &disjoint_data, sizeof(disjoint_data), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
                device_context->GetData(begin_query.Get(), &time_start, sizeof(time_start), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK &&
                device_context->GetData(end_query.Get(), &time_end, sizeof(time_end), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK)
            {

                if (disjoint_data.Disjoint == FALSE)
                {
                    const UINT64 delta = time_end - time_start;
                    time_delta = (float)(((double)delta / (double)disjoint_data.Frequency) * 1000.0);
                }

                finalized = true;
            }
        }
        else
        {
            int wait_counter = 0;
            while (device_context->GetData(disjoint_query.Get(), &disjoint_data, sizeof(disjoint_data), 0) != S_OK)
            {
                if (wait_counter++ >= 10000)
                    return 0.0f;
                Sleep(0);
            }

            wait_counter = 0;
            while (device_context->GetData(begin_query.Get(), &time_start, sizeof(time_start), 0) != S_OK)
            {
                if (wait_counter++ >= 10000)
                    return 0.0f;
                Sleep(0);
            }
            wait_counter = 0;
            while (device_context->GetData(end_query.Get(), &time_end, sizeof(time_end), 0) != S_OK)
            {
                if (wait_counter++ >= 10000)
                    return 0.0f;
                Sleep(0);
            }

            if (disjoint_data.Disjoint == FALSE)
            {
                const UINT64 delta = time_end - time_start;
                time_delta = (float)(((double)delta / (double)disjoint_data.Frequency) * 1000.0);
            }
            else
            {
                time_delta = 0.0f;
            }

            finalized = true;
        }
    }
    return time_delta;
}
