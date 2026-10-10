cmake_minimum_required(VERSION 3.20)
project(elgato-lights-installer LANGUAGES CXX)

# -DPLUGIN_DLL=<chemin de obs-elgato-lights.dll> -DAPP_VERSION=1.0.0
if(NOT PLUGIN_DLL OR NOT EXISTS "${PLUGIN_DLL}")
  message(FATAL_ERROR "PLUGIN_DLL manquant ou introuvable : '${PLUGIN_DLL}'")
endif()
if(NOT APP_VERSION)
  set(APP_VERSION "1.0.0")
endif()

# Runtime C statique : l'installateur n'a aucune dependance (pas de VC++ Redistributable).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

file(TO_NATIVE_PATH "${PLUGIN_DLL}" PLUGIN_DLL_NATIVE)
string(REPLACE "\\" "\\\\" PLUGIN_DLL "${PLUGIN_DLL_NATIVE}")
string(REPLACE "." "," APP_VERSION_COMMA "${APP_VERSION}.0")
configure_file(installer.rc.in "${CMAKE_CURRENT_BINARY_DIR}/installer.rc" @ONLY)

add_executable(Install-Elgato-Lights WIN32 installer.cpp "${CMAKE_CURRENT_BINARY_DIR}/installer.rc")
target_compile_definitions(Install-Elgato-Lights PRIVATE UNICODE _UNICODE APP_VERSION_W=L"${APP_VERSION}")
target_link_options(Install-Elgato-Lights PRIVATE /MANIFESTUAC:level='asInvoker'\ uiAccess='false')
