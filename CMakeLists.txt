cmake_minimum_required(VERSION 3.24)
project(NovelVideoFactory LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(NovelVideoFactory WIN32 src/main.cpp)
set_target_properties(NovelVideoFactory PROPERTIES OUTPUT_NAME "CNnovels-video changer")
target_compile_definitions(NovelVideoFactory PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_options(NovelVideoFactory PRIVATE -municode -static -static-libgcc -static-libstdc++)
target_link_libraries(NovelVideoFactory PRIVATE ole32 comctl32 comdlg32 shell32 shlwapi urlmon uuid)

add_custom_command(TARGET NovelVideoFactory POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:NovelVideoFactory>/resources"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/resources/ffmpeg.exe" "$<TARGET_FILE_DIR:NovelVideoFactory>/resources/ffmpeg.exe"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/resources/edge_tts_runner.py" "$<TARGET_FILE_DIR:NovelVideoFactory>/resources/edge_tts_runner.py"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/LICENSES" "$<TARGET_FILE_DIR:NovelVideoFactory>/LICENSES"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" "$<TARGET_FILE_DIR:NovelVideoFactory>/THIRD_PARTY_NOTICES.md"
  COMMENT "Copying bundled FFmpeg and Edge TTS launcher")
if(EXISTS "${CMAKE_SOURCE_DIR}/resources/python")
  add_custom_command(TARGET NovelVideoFactory POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/resources/python" "$<TARGET_FILE_DIR:NovelVideoFactory>/resources/python"
    COMMENT "Copying embedded Python Edge TTS runtime")
endif()

