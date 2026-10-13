# JUCE refuses MinGW-style targets. The zig cross build (x86_64-windows-gnu, clang + libc++) works fine once
# the pointer size is detected without _MSC_VER and the MinGW guard is lifted, so patch the header in place.
set(_mc_hdr "${juce_SOURCE_DIR}/modules/juce_core/system/juce_TargetPlatform.h")
file(READ "${_mc_hdr}" _mc_src)
if(NOT _mc_src MATCHES "MELODY_CODE_MINGW_PATCH")
 string(REPLACE "  #ifdef __MINGW32__\n    #error \"MinGW is not supported. Please use an alternative compiler.\"\n  #endif"
  "  // MELODY_CODE_MINGW_PATCH\n  #if ! defined (_MSC_VER) && defined (_WIN64)\n    #define JUCE_64BIT 1\n  #endif" _mc_src "${_mc_src}")
 file(WRITE "${_mc_hdr}" "${_mc_src}")
endif()

# MinGW's __uuidof cannot see through ComSmartPtr variables; name the interfaces directly.
foreach(_mc_file juce_graphics/native/juce_DirectX_windows.cpp juce_gui_basics/native/juce_Direct2DHwndContext_windows.cpp)
 set(_mc_path "${juce_SOURCE_DIR}/modules/${_mc_file}")
 file(READ "${_mc_path}" _mc_src)
 string(REPLACE "__uuidof (device)" "__uuidof (IDXGIDevice)" _mc_new "${_mc_src}")
 string(REPLACE "__uuidof (surface)" "__uuidof (IDXGISurface)" _mc_new "${_mc_new}")
 if(NOT _mc_new STREQUAL _mc_src)
  file(WRITE "${_mc_path}" "${_mc_new}")
 endif()
endforeach()
