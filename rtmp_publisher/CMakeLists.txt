message(STATUS "cmake RTMPPublisher.a")

aux_source_directory(. RTMPPublisherSrc)

include_directories(
        ../lib/librtmp
        ../common/util
        ../common/log
        ../lib/zlog/include
)

link_directories(
        ../lib/librtmp/lib
        ../lib/zlog
)

add_compile_options(-c -g)

add_library(RTMPPublisher STATIC ${RTMPPublisherSrc})

target_link_libraries(RTMPPublisher
        rtmp zlog
)
