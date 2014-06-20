project(qemuint)
INCLUDE("${esesc_SOURCE_DIR}/CMake.common")

INCLUDE_DIRECTORIES(${suc_SOURCE_DIR})
INCLUDE_DIRECTORIES(${crack_SOURCE_DIR})
INCLUDE_DIRECTORIES(${emulint_SOURCE_DIR})
INCLUDE_DIRECTORIES(${core_SOURCE_DIR})

FILE(GLOB qemuint_SOURCE *.cpp)
FILE(GLOB qemuint_HEADER *.h)

SOURCE_GROUP("Source Files" FILES ${qemuint_SOURCE})
SOURCE_GROUP("Header Files" FILES ${qemuint_HEADER})

FILE(GLOB exec_SOURCE1 qemumain.cpp)
FILE(GLOB exec_SOURCE2 esescbinfmt.cpp)

ADD_LIBRARY(qemuint ${qemuint_SOURCE} ${qemuint_HEADER})

##########################
# qemumain


#ADD_EXECUTABLE(qemumain ${exec_SOURCE1})

#TARGET_LINK_LIBRARIES("qemumain" qemuint emulint core crack suc)
#TARGET_LINK_LIBRARIES("qemumain" ${CMAKE_QEMU_LIBS})

#ADD_EXECUTABLE(esescbinfmt ${exec_SOURCE2})
#TARGET_LINK_LIBRARIES("esescbinfmt" suc -static)

