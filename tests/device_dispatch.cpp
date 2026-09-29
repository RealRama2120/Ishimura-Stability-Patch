#include "../src/d3d_hooks.cpp"
#include <cstdio>

namespace Config {
Settings settings;
int EffectiveFpsLimit() { return 0; }
}
namespace Logger { void Write(const wchar_t*, ...) {} }
namespace GamePatches {
bool PrepareBorderlessRenderSize(unsigned int,unsigned int) { return false; }
void ObserveRenderHeight(unsigned int) {}
}

static void PrivateDispatchSentinel() {}
static void Stage(const char* text) {
    FILE* file=nullptr;
    if(fopen_s(&file,"device-dispatch-result.txt","a")==0) {
        std::fprintf(file,"%s\n",text);
        std::fclose(file);
    }
}

int main() {
    Stage("START");
    // A runtime table can contain private dispatch entries beyond the public
    // interface. Exercise the actual hook against a read-only synthetic table.
    auto table=static_cast<void**>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!table) return 1;
    for(int i=0;i<180;++i) table[i]=reinterpret_cast<void*>(&PrivateDispatchSentinel);
    void** objectTable=table;
    auto object=reinterpret_cast<IDirect3DDevice9*>(&objectTable);
    DWORD protection=0;
    if(!VirtualProtect(table,4096,PAGE_READONLY,&protection)) return 2;
    if(!D3DHooks::HookDevice(object) || objectTable!=table) return 3;
    for(int i=0;i<180;++i) {
        const bool hooked=i==16 || i==17 || i==69;
        if((table[i]!=reinterpret_cast<void*>(&PrivateDispatchSentinel))!=hooked) return 4;
    }
    reinterpret_cast<void(*)()>(objectTable[156])();
    if(!D3DHooks::HookDevice(object)) return 5;
    MEMORY_BASIC_INFORMATION memory={};
    if(!VirtualQuery(table,&memory,sizeof(memory)) || memory.Protect!=PAGE_READONLY) return 6;
    D3DHooks::g_deviceDispatch=nullptr;
    VirtualFree(table,0,MEM_RELEASE);
    Stage("synthetic private dispatch and table protections passed");

    // Exercise a real native D3D9 device so Windows' private dispatch calls,
    // frame presentation, sampler state and reset are covered without Steam.
    Config::settings.borderlessWindowed=false;
    Config::settings.anisotropicFiltering=false;
    HWND window=CreateWindowW(L"STATIC",L"Ishimura isolated D3D9 test",WS_OVERLAPPEDWINDOW,
        0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    IDirect3D9* d3d=Direct3DCreate9(D3D_SDK_VERSION);
    if(!window || !d3d) return 7;
    D3DPRESENT_PARAMETERS parameters={};
    parameters.Windowed=TRUE;
    parameters.hDeviceWindow=window;
    parameters.BackBufferWidth=320;
    parameters.BackBufferHeight=240;
    parameters.SwapEffect=D3DSWAPEFFECT_DISCARD;
    IDirect3DDevice9* device=nullptr;
    if(FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,&parameters,&device))) return 8;
    Stage("native device created");
    auto originalTable=*reinterpret_cast<void***>(device);
    if(!D3DHooks::HookDevice(device) || *reinterpret_cast<void***>(device)!=originalTable) return 9;
    if(FAILED(device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR))) return 10;
    if(FAILED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1.0f,0))) return 11;
    if(FAILED(device->Present(nullptr,nullptr,nullptr,nullptr))) return 12;
    Stage("native Present and sampler passed");
    if(FAILED(device->Reset(&parameters))) return 13;
    if(FAILED(device->Present(nullptr,nullptr,nullptr,nullptr))) return 14;
    Stage("native Reset and Present passed");
    device->Release();
    // A recreated device uses the same complete table without recursive hooks.
    device=nullptr;
    if(FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,&parameters,&device))) return 15;
    if(!D3DHooks::HookDevice(device)) { Stage("recreated device uses another dispatch table"); return 16; }
    HRESULT present=device->Present(nullptr,nullptr,nullptr,nullptr);
    if(FAILED(present)) { char text[80]; sprintf_s(text,"recreated device Present failed %08lx",present); Stage(text); return 17; }
    device->Release();
    d3d->Release();
    DestroyWindow(window);
    Stage("PASS");
    std::puts("PASS: private slot 156, unchanged unrelated slots, protection restoration, repeated hooks, native D3D9 Present/Reset/device recreation.");
    return 0;
}
