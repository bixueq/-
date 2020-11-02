cmake_minimum_required(VERSION 3.12)
project(QosSend)

set(CMAKE_C_COMPILER   /opt/hisi-linux/x86-arm/arm-himix100-linux/bin/arm-himix100-linux-gcc)
set(CMAKE_CXX_COMPILER /opt/hisi-linux/x86-arm/arm-himix100-linux/bin/arm-himix100-linux-g++)

#set(CMAKE_C_COMPILER   /opt/hisi-linux/x86-arm/arm-hisiv600-linux/target/bin/arm-hisiv600-linux-gcc)
#set(CMAKE_CXX_COMPILER /opt/hisi-linux/x86-arm/arm-hisiv600-linux/target/bin/arm-hisiv600-linux-g++)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_FLAGS "-DLINUX -Dlinux ")
set(CMAKE_CXX_FLAGS "-DLINUX -Dlinux -fpermissive")


aux_source_directory(./src Src)
aux_source_directory(rtmp_publisher rtmpSrc)
set(QosSendSrc
        ${Src}
        ${rtmpSrc}
        )

if(${CMAKE_C_COMPILER} MATCHES "arm-hisiv400-linux-gcc")
include_directories(
        ./src
        lib/v400/include

)
link_directories(
        lib/v400/lib
)
elseif(${CMAKE_C_COMPILER} MATCHES "arm-hisiv600-linux-gcc")
include_directories(
        ./src
        lib/v600/include
)

elseif(${CMAKE_C_COMPILER} MATCHES "arm-himix100-linux-gcc")
    include_directories(
            ./src
            lib/v400/include
           rtmp_publisher
            lib/librtmp
    )
link_directories(
        lib/librtmp/lib
        lib/v400/lib
)

endif()



add_executable(QosSend ${QosSendSrc})



target_link_libraries(QosSend
        rt
        pthread
        TerminalQos
        rtmp.a
        )