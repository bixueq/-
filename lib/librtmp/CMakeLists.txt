message(STATUS "cmake AudioProcess.a")
set(PrjName rtmpx)


aux_source_directory(. mainSrc)

include_directories(
        ../../common/log
        ../../lib/zlog/include
        ../../lib/libadns/include
)

link_directories(

)

add_library(${PrjName} STATIC ${mainSrc})
