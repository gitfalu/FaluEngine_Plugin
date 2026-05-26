# cmake/FindDirectX.cmake

if(NOT WIN32)
    set(DirectX_FOUND FALSE)
    return()
endif()

# Windows SDK のバージョンを取得
if(NOT CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    # レジストリから最新の Windows SDK バージョンを探す
    file(GLOB SDK_VERSIONS
        "C:/Program Files (x86)/Windows Kits/10/Include/*/um/d3d11.h"
        "C:/Program Files/Windows Kits/10/Include/*/um/d3d11.h"
    )
    if(SDK_VERSIONS)
        list(GET SDK_VERSIONS -1 _LATEST)
        string(REGEX MATCH "Include/([0-9.]+)/um" _ "${_LATEST}")
        set(_SDK_VER "${CMAKE_MATCH_1}")
    endif()
else()
    set(_SDK_VER "${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
endif()

find_path(DirectX_INCLUDE_DIR
    NAMES d3d11.h
    PATHS
        "C:/Program Files (x86)/Windows Kits/10/Include/${_SDK_VER}/um"
        "C:/Program Files/Windows Kits/10/Include/${_SDK_VER}/um"
        "$ENV{DXSDK_DIR}/Include"
    DOC "DirectX 11 include directory"
)

# x64 ライブラリを検索
find_library(DirectX_D3D11_LIBRARY
    NAMES d3d11
    PATHS
        "C:/Program Files (x86)/Windows Kits/10/Lib/${_SDK_VER}/um/x64"
        "C:/Program Files/Windows Kits/10/Lib/${_SDK_VER}/um/x64"
        "$ENV{DXSDK_DIR}/Lib/x64"
)

find_library(DirectX_DXGI_LIBRARY
    NAMES dxgi
    PATHS
        "C:/Program Files (x86)/Windows Kits/10/Lib/${_SDK_VER}/um/x64"
        "C:/Program Files/Windows Kits/10/Lib/${_SDK_VER}/um/x64"
        "$ENV{DXSDK_DIR}/Lib/x64"
)

find_library(DirectX_D3DCOMPILER_LIBRARY
    NAMES d3dcompiler
    PATHS
        "C:/Program Files (x86)/Windows Kits/10/Lib/${_SDK_VER}/um/x64"
        "C:/Program Files/Windows Kits/10/Lib/${_SDK_VER}/um/x64"
        "$ENV{DXSDK_DIR}/Lib/x64"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DirectX
    REQUIRED_VARS DirectX_INCLUDE_DIR DirectX_D3D11_LIBRARY DirectX_DXGI_LIBRARY
)

if(DirectX_FOUND AND NOT TARGET DirectX::D3D11)
    add_library(DirectX::D3D11 INTERFACE IMPORTED)
    target_include_directories(DirectX::D3D11 INTERFACE "${DirectX_INCLUDE_DIR}")

    # d3d11 と dxgi は必須、d3dcompiler は見つかった場合のみ追加
    set(_DX_LIBS ${DirectX_D3D11_LIBRARY} ${DirectX_DXGI_LIBRARY})
    if(DirectX_D3DCOMPILER_LIBRARY)
        list(APPEND _DX_LIBS ${DirectX_D3DCOMPILER_LIBRARY})
    else()
        message(WARNING "d3dcompiler not found — shader runtime compilation は無効になります")
    endif()

    target_link_libraries(DirectX::D3D11 INTERFACE ${_DX_LIBS})
endif()

mark_as_advanced(DirectX_INCLUDE_DIR DirectX_D3D11_LIBRARY
                 DirectX_DXGI_LIBRARY DirectX_D3DCOMPILER_LIBRARY)
