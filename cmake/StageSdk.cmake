# エディタ配布用SDKを作成
function(falu_stage_headers src dst)
	file(GLOB_RECURSE _hdrs RELATIVE "${src}" "${src}/*.h" "${src}/*.hpp" "${src}/*.inl")
	foreach(_f ${_hdrs})
		configure_file("${src}/${_f}" "${dst}/${_f}" COPYONLY)
	endforeach()
endfunction()

# エンジン自身のヘッダ
falu_stage_headers("${ENGINE_DIR}/include" "${SDK_DIR}/include")
falu_stage_headers("${ENGINE_DIR}" "${SDK_DIR}/engine")

# 外部ライブラリのヘッダ
if(VCPKG_DIR AND EXISTS "${VCPKG_DIR}/include")
	foreach(_d glm entt spdlog fmt sol)
		if(EXISTS "${VCPKG_DIR}/include/${_d}")
			file(COPY "${VCPKG_DIR}/include/${_d}" DESTINATION "${SDK_DIR}/include")
		endif()
	endforeach()
	file(GLOB _top
		"${VCPKG_DIR}/include/imgui*.h" "${VCPKG_DIR}/include/imconfig.h"
		"${VCPKG_DIR}/include/lua.h" "${VCPKG_DIR}/include/lua.hpp"
		"${VCPKG_DIR}/include/luaconf.h" "${VCPKG_DIR}/include/lualib.h"
		"${VCPKG_DIR}/include/lauxlib.h" )
	if(_top)
		file(COPY ${_top} DESTINATION "${SDK_DIR}/include")
	endif()
else()
	message(WARNING "StageSdk: VCPKG_DIR not found - third-party headers/libs are not staged")
endif()

# ライブラリ
set(_lib_out "${SDK_DIR}/lib/${CONFIG}")
file(MAKE_DIRECTORY "${_lib_out}")
file(GLOB _engine_libs "${ENGINE_LIB_DIR}/Falu*.lib")
if(_engine_libs)
	file(COPY ${_engine_libs} DESTINATION "${_lib_out}")
endif()

if(VCPKG_DIR)
	if(CONFIG STREQUAL "Debug")
		set(_vcpkg_lib "${VCPKG_DIR}/debug/lib")
	else()
		set(_vcpkg_lib "${VCPKG_DIR}/lib")
	endif()

	file(GLOB _third_libs
		"${_vcpkg_lib}/imgui*.lib" "${_vcpkg_lib}/spdlog*.lib"
		"${_vcpkg_lib}/fmt*.lib" "${_vcpkg_lib}/lua*.lib")
	if(_third_libs)
		file(COPY ${_third_libs} DESTINATION "${_lib_out}")
	endif()
endif()