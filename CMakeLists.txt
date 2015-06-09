
cmake_minimum_required (VERSION 2.8)
project (card_proxy)

option (YBORM_ROOT "Path where YB.ORM is installed" /usr/local)
option (CPPREST_ROOT "Path where C++ REST SDK is installed" /usr/local)

if (CMAKE_COMPILER_IS_GNUCXX)
    SET(ENABLE_CXX11 "-std=c++11")

    EXECUTE_PROCESS(COMMAND "${CMAKE_CXX_COMPILER} -dumpversion" OUTPUT_VARIABLE GCC_VERSION)
    if (GCC_VERSION VERSION_LESS 4.7)
        SET(ENABLE_CXX11 "-std=c++0x")
    endif()

    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ENABLE_CXX11} -Wall -Wno-deprecated-declarations -Wno-unused-local-typedefs")
endif()

find_path (BOOST_INCLUDEDIR boost/lexical_cast.hpp /usr/include)
set (BOOST_LIBRARYDIR /usr/lib)
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

function (check_cpprest_root CPPREST_ROOT)
    if (EXISTS ${CPPREST_ROOT})
        find_path (CPPREST_INCLUDES cpprest/http_client.h ${CPPREST_ROOT}/include)
        find_library (CPPREST_LIB cpprest ${CPPREST_ROOT}/lib ${CPPREST_ROOT}/build.release/Binaries)
        if (CPPREST_INCLUDES AND CPPREST_LIB)
            message (STATUS "C++ REST SDK found: ${CPPREST_ROOT}")
        endif ()
    endif ()
endfunction ()

if (CPPREST_ROOT)
    check_cpprest_root (${CPPREST_ROOT})
else ()
    check_cpprest_root (/usr/local)
endif ()

if ((NOT CPPREST_INCLUDES) OR (NOT CPPREST_LIB))
    message(SEND_ERROR "C++ REST SDK not found!!")
endif ()

add_subdirectory (src)

install (FILES README.md DESTINATION doc)

