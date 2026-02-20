cmake_minimum_required(VERSION 3.10)
project(POG2WhyML)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)


set(POG2WHY_HEADERS
    TypingContext.h
    version.h
    WhyML.h
    WhyMLInternal.h
    tinyxml2.h
    )

set(POG2WHY_SRC
    TypingContext.cpp
    exceptions.cpp
    WhyML.cpp
    WhyPredicate.cpp
    WhyDefine.cpp
    WhyLocalHyp.cpp
    WhySimpleGoal.cpp
    tinyxml2.cpp
    )


include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${POFILES_SOURCE_DIR}
    ${AST_SOURCE_DIR}
    ${POG_SOURCE_DIR}
    ${tinyxml2_SOURCE_DIR}
    )

add_library(POG2WHYLIB
    ${POG2WHY_SRC}
    ${POG2WHY_HEADERS}
    )

target_link_libraries(POG2WHYLIB)

add_executable(POG2WhyML
    POG2WhyML.cpp
    )

target_link_libraries(POG2WhyML POG2WHYLIB)
