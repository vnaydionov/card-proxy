
cmake_minimum_required (VERSION 2.8)
project (card_proxy)

option (YBORM_ROOT "Path where YB.ORM is installed" /usr/local)
option (USE_VAULT_DEBUG_API "Whether to include debug methods in the build" ON)

if (CMAKE_COMPILER_IS_GNUCXX)
    SET(ENABLE_CXX11 "-std=c++11")

    EXECUTE_PROCESS(COMMAND "${CMAKE_CXX_COMPILER} -dumpversion" OUTPUT_VARIABLE GCC_VERSION)
    if (GCC_VERSION VERSION_LESS 4.7)
        SET(ENABLE_CXX11 "-std=c++0x")
    endif()

    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ENABLE_CXX11} -g -Wall -Wno-deprecated-declarations -Wno-unused-local-typedefs -Wno-format-security")
endif()

find_path (BOOST_INCLUDEDIR boost/lexical_cast.hpp /usr/include)
EXECUTE_PROCESS(COMMAND "arch | sed 's/i[456]86/i386/'" OUTPUT_VARIABLE BOOST_ARCH)
set (BOOST_LIBRARYDIR /usr/lib/${BOOST_ARCH}-linux-gnu/)
set (Boost_USE_STATIC_LIBS OFF)
set (Boost_USE_MULTITHREADED ON)
set (Boost_NO_SYSTEM_PATHS ON)

find_package (Boost 1.46.0 COMPONENTS system thread date_time chrono)

if ((NOT Boost_DATE_TIME_LIBRARY) OR (NOT Boost_THREAD_LIBRARY))
    message (FATAL_ERROR "Boost not found!!")
endif ()
add_definitions (-DYB_USE_TUPLE)

if (Boost_SYSTEM_LIBRARY)
    set (YB_BOOST_LIBS ${Boost_SYSTEM_LIBRARY})
endif ()
if (Boost_CHRONO_LIBRARY)
    set (YB_BOOST_LIBS ${YB_BOOST_LIBS} ${Boost_CHRONO_LIBRARY})
endif ()
set (YB_BOOST_LIBS ${YB_BOOST_LIBS} ${Boost_THREAD_LIBRARY}
                   ${Boost_DATE_TIME_LIBRARY})

function (check_yborm_root YBORM_ROOT)
    if (EXISTS ${YBORM_ROOT})
        find_path (YBORM_INCLUDES orm/sql_driver.h ${YBORM_ROOT}/include/yb)
        find_library (YBUTIL_LIB ybutil ${YBORM_ROOT}/lib)
        find_library (YBORM_LIB yborm ${YBORM_ROOT}/lib)
        if (YBORM_INCLUDES AND YBUTIL_LIB AND YBORM_LIB)
            message (STATUS "YB.ORM found: ${YBORM_ROOT}")
        endif ()
    endif ()
endfunction ()

if (YBORM_ROOT)
    check_yborm_root (${YBORM_ROOT})
else ()
    check_yborm_root (/usr/local)
endif ()

if ((NOT YBORM_INCLUDES) OR (NOT YBUTIL_LIB) OR (NOT YBORM_LIB))
    message(SEND_ERROR "YB.ORM not found!!")
endif ()

find_package(CURL)
IF (NOT CURL_FOUND)
    MESSAGE(FATAL_ERROR "Could not find the CURL library and development files.")
ENDIF ()

find_library(JSONC_LIB json-c)
if (NOT JSONC_LIB)
    MESSAGE(FATAL_ERROR "Could not find the JSON-C library and development files.")
ENDIF ()

add_subdirectory (xxutils)
add_subdirectory (xxcommon)
add_subdirectory (tokenizer)
add_subdirectory (keykeeper2)

install (FILES LICENSE README.md DESTINATION share/doc/card-proxy-tokenizer)

