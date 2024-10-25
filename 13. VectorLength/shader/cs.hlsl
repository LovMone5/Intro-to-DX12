struct Data {
    float3 v;
    float val;
    int id;
};

StructuredBuffer<Data> g_input : register(t0);
RWStructuredBuffer<Data> g_output : register(u0);

[numthreads(256, 1, 1)]
void CS(int3 id : SV_DispatchThreadID) {
    g_output[id.x].v = g_input[id.x].v;
    g_output[id.x].val = length(g_input[id.x].v);
    g_output[id.x].id = id.x;
}
