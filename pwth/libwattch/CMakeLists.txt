project(libwattch)

SET(CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG=1 -Wall")
SET(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG=1 -Wall")

INCLUDE_DIRECTORIES(${suc_SOURCE_DIR})

FILE(GLOB wattch_SOURCE *.cpp *.c)
FILE(GLOB wattch_HEADER *.h)

SOURCE_GROUP("Source Files" FILES ${wattch_SOURCE})
SOURCE_GROUP("Header Files" FILES ${wattch_HEADER})

FILE(GLOB exec_SOURCE1 wattchify.cpp)

LIST(REMOVE_ITEM wattch_SOURCE ${exec_SOURCE1})

ADD_LIBRARY(wattch ${wattch_SOURCE} ${wattch_HEADER})

##########################
# wattchify

ADD_EXECUTABLE(wattchify ${exec_SOURCE1})

TARGET_LINK_LIBRARIES("wattchify" wattch suc ${ZLIB_LIBRARIES}) 
