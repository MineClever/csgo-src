# CopyIfExists.cmake
# 辅助脚本：仅当源文件存在时才执行复制（用于可选的 PDB 文件）
# 调用方式：cmake -DSRC=<src> -DDST=<dst> -P CopyIfExists.cmake
if(EXISTS "${SRC}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SRC}" "${DST}")
endif()
