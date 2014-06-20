project(libcacti)

SET(CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG=1 -Wall")
SET(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG=1 -Wall")

INCLUDE_DIRECTORIES(${suc_SOURCE_DIR})

FILE(GLOB cacti_SOURCE *.cpp *.c)
FILE(GLOB cacti_HEADER *.h)

SOURCE_GROUP("Source Files" FILES ${cacti_SOURCE})
SOURCE_GROUP("Header Files" FILES ${cacti_HEADER})

FILE(GLOB exec_SOURCE1 cactify.cpp)

LIST(REMOVE_ITEM cacti_SOURCE ${exec_SOURCE1})

ADD_LIBRARY(cacti ${cacti_SOURCE} ${cacti_HEADER})

##########################
# cactify

ADD_EXECUTABLE(cactify ${exec_SOURCE1})

TARGET_LINK_LIBRARIES("cactify" cacti suc ${ZLIB_LIBRARIES}) 
