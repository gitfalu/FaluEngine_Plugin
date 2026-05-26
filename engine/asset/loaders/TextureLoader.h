#pragma once
#include "asset/AssetManager.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

namespace FaluEngine {

struct TextureAsset : Asset {
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 1;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    // ID3D11ShaderResourceView* srv = nullptr; // DirectX 統合後に追加
};

std::shared_ptr<TextureAsset> loadTexture(const std::string& path,ID3D11Device* device,
    ID3D11DeviceContext* context);

void registerTextureLoader(ID3D11Device* device, ID3D11DeviceContext* context);

} // namespace FaluEngine
