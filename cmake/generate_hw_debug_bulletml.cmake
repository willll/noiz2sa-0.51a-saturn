if(NOT DEFINED HW_DEBUG_BULLETML_OUTPUT)
    message(FATAL_ERROR "HW_DEBUG_BULLETML_OUTPUT is not defined")
endif()

function(hw_debug_emit_group output_path group_name file_list_var)
    set(entries "")
    foreach(input_file IN LISTS ${file_list_var})
        get_filename_component(file_name "${input_file}" NAME)
        string(MAKE_C_IDENTIFIER "${group_name}_${file_name}" symbol_name)
        file(READ "${input_file}" hex_bytes HEX)
        string(REGEX REPLACE "(..)" "0x\\1, " byte_list "${hex_bytes}")
        file(APPEND "${output_path}" "static const uint8_t ${symbol_name}[] = { ${byte_list} };\n")
        string(APPEND entries "    {\"${file_name}\", ${symbol_name}, static_cast<uint32_t>(sizeof(${symbol_name}))},\n")
    endforeach()

    file(APPEND "${output_path}" "static const EmbeddedPattern k${group_name}Patterns[] = {\n${entries}};\n")
    file(APPEND "${output_path}" "static constexpr uint32_t k${group_name}PatternCount = static_cast<uint32_t>(sizeof(k${group_name}Patterns) / sizeof(k${group_name}Patterns[0]));\n\n")
endfunction()

file(WRITE "${HW_DEBUG_BULLETML_OUTPUT}" "#pragma once\n#include <cstdint>\n\nnamespace HWDebugBulletML {\n\nstruct EmbeddedPattern {\n    const char* name;\n    const uint8_t* data;\n    uint32_t size;\n};\n\n")

hw_debug_emit_group("${HW_DEBUG_BULLETML_OUTPUT}" "Zako" HW_DEBUG_ZAKO_BLB_FILES)
hw_debug_emit_group("${HW_DEBUG_BULLETML_OUTPUT}" "Middle" HW_DEBUG_MIDDLE_BLB_FILES)
hw_debug_emit_group("${HW_DEBUG_BULLETML_OUTPUT}" "Boss" HW_DEBUG_BOSS_BLB_FILES)

file(APPEND "${HW_DEBUG_BULLETML_OUTPUT}" "} // namespace HWDebugBulletML\n")