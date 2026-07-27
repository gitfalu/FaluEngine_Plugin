#include "TextureLoader.h"
#include "core/Logger.h"

#include <DirectXTex.h>
#include <filesystem>
namespace FaluEngine {

std::shared_ptr<TextureAsset> loadTexture(const std::string& path, ID3D11Device* device,
    ID3D11DeviceContext* context) {
    if (!std::filesystem::exists(path)) {
        LOG_ERROR("TextureLoader: file not found '{}'", path);
        return nullptr;
    }

    std::wstring wpath(path.begin(), path.end());
    DirectX::ScratchImage image;
    HRESULT hr;

    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".dds") {
        hr = DirectX::LoadFromDDSFile(wpath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
        if (FAILED(hr))
        {
            LOG_ERROR("TextureLoader: Load DDS File Failed '{}'", path);
        }
    }
    else if (ext == ".hdr") {
        hr = DirectX::LoadFromHDRFile(wpath.c_str(), nullptr, image);
        if (FAILED(hr))
        {
            LOG_ERROR("TextureLoader: Load HDR File Failed '{}'", path);
        }
    }
    else {
        hr = DirectX::LoadFromWICFile(wpath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
        if (FAILED(hr))
        {
            LOG_ERROR("TextureLoader: Load WIC File Failed '{}'", path);
        }
    }

    if (ext != ".dds")
    {
        DirectX::ScratchImage mipChain;
        DirectX::GenerateMipMaps(*image.GetImage(0, 0, 0),
            DirectX::TEX_FILTER_DEFAULT,0,mipChain);
        image = std::move(mipChain);
    }

    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

    hr = DirectX::CreateTextureEx(
        device, image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        DirectX::CREATETEX_DEFAULT, &resource
    );

    if (FAILED(hr)) {
        LOG_ERROR("TextureLoader: CreateTexture failed '{}'", path);
        return nullptr;
    }

    hr = device->CreateShaderResourceView(resource.Get(), nullptr, &srv);
    if (FAILED(hr)) {
        LOG_ERROR("TextureLoader: CreateSRV failed '{}'", path);
        return nullptr;
    }

    auto asset = std::make_shared<TextureAsset>();
    asset->width = static_cast<uint32_t>(image.GetMetadata().width);
    asset->height = static_cast<uint32_t>(image.GetMetadata().height);
    asset->mipLevels = static_cast<uint32_t>(image.GetMetadata().mipLevels);
    asset->srv = srv;

    LOG_INFO("TextureLoader: loaded '{}' ({}*{}, {} mips)",
        path, asset->width, asset->height, asset->mipLevels);

    return asset;
}

void registerTextureLoader(ID3D11Device* device, ID3D11DeviceContext* context)
{
    AssetManager::get().registerLoader<TextureAsset>(
        [device, context](const std::string& path) {
            return loadTexture(path,device, context);
        }
    );
}

} // namespace FaluEngine
