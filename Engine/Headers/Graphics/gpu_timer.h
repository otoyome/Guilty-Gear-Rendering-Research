#pragma once

class gpu_timer
{
public:
    gpu_timer(Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context);

    ~gpu_timer();

    void begin();
    void end();
    float get_result(bool flush = false);

    bool is_finalized() const { return finalized; }

private:

    bool finalized = true;
    bool end_called = false;
    float time_delta = 0.0f;

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
    Microsoft::WRL::ComPtr<ID3D11Query> begin_query;
    Microsoft::WRL::ComPtr<ID3D11Query> end_query;
    Microsoft::WRL::ComPtr<ID3D11Query> disjoint_query;
};

