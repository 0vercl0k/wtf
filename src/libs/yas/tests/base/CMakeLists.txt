cmake_minimum_required(VERSION 3.2)
project(yas-base-test)

set (CMAKE_CXX_STANDARD 11)

add_definitions(-UNDEBUG)

if (YAS_SERIALIZE_ABSL_TYPES)
    message("Abseil support is enabled")
else()
    message("Abseil support is disabled")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    add_definitions(
        -DBOOST_ERROR_CODE_HEADER_ONLY
        -DBOOST_CHRONO_HEADER_ONLY
        -DYAS_SERIALIZE_BOOST_TYPES
        -DYAS_OBJECT_MAX_MEMBERS=50
    )

    if (YAS_SERIALIZE_ABSL_TYPES)
        add_definitions(-DYAS_SERIALIZE_ABSL_TYPES)

        # Process Abseil's CMake build system
        add_subdirectory("$ENV{ABSEIL_HOME}" "build/abseil")
    endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    add_definitions(
        -DBOOST_ERROR_CODE_HEADER_ONLY
        -DBOOST_CHRONO_HEADER_ONLY
        -DYAS_SERIALIZE_BOOST_TYPES
        -DYAS_OBJECT_MAX_MEMBERS=50
    )

    if (YAS_SERIALIZE_ABSL_TYPES)
        add_definitions(-DYAS_SERIALIZE_ABSL_TYPES)
        
        # Process Abseil's CMake build system
        add_subdirectory("$ENV{ABSEIL_HOME}" "build/abseil")
    endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(COMPILER_BITS 64)
    else()
        set(COMPILER_BITS 32)
    endif()

    message("TOOLSET=" ${MSVC_TOOLSET_VERSION})

    include_directories("C:/local/boost_1_66_0")
    link_directories("C:/local/boost_1_66_0/lib${COMPILER_BITS}-msvc-14.1")

    if (${MSVC_TOOLSET_VERSION} EQUAL 142)
        message("MSVC-2019")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14 /bigobj")
    else()
        message("MSVC-2017")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14 /bigobj /d1parsePackExpressions-")
    endif()
    
    add_definitions(
        -DBOOST_ERROR_CODE_HEADER_ONLY
        -DBOOST_CHRONO_HEADER_ONLY
        -DYAS_SERIALIZE_BOOST_TYPES
        -DYAS_OBJECT_MAX_MEMBERS=50
        -D_CRT_SECURE_NO_WARNINGS
    )

    if (YAS_SERIALIZE_ABSL_TYPES)
        add_subdirectory("C:/abseil-cpp" "build/abseil")
    
        # Process Abseil's CMake build system
        add_definitions(-DYAS_SERIALIZE_ABSL_TYPES)
    endif()
endif()

include_directories(
    ../../include
)

set(SOURCE_FILES
    include/array.hpp
    include/auto_array.hpp
    include/base64.hpp
    include/base_object.hpp
    include/bitset.hpp
    include/boost_cont_deque.hpp
    include/boost_cont_flat_map.hpp
    include/boost_cont_flat_multimap.hpp
    include/boost_cont_flat_multiset.hpp
    include/boost_cont_flat_set.hpp
    include/boost_cont_list.hpp
    include/boost_cont_map.hpp
    include/boost_cont_multimap.hpp
    include/boost_cont_multiset.hpp
    include/boost_cont_set.hpp
    include/boost_cont_slist.hpp
    include/boost_cont_stable_vector.hpp
    include/boost_cont_static_vector.hpp
    include/boost_cont_string.hpp
    include/boost_cont_vector.hpp
    include/boost_cont_wstring.hpp
    include/boost_fusion_list.hpp
    include/boost_fusion_map.hpp
    include/boost_fusion_pair.hpp
    include/boost_fusion_set.hpp
    include/boost_fusion_tuple.hpp
    include/boost_fusion_vector.hpp
    include/boost_string_view.hpp
    include/boost_tuple.hpp
    include/boost_variant.hpp
    include/buffer.hpp
    include/chrono.hpp
    include/compacted_storage_size.hpp
    include/complex.hpp
    include/deque.hpp
    include/endian.hpp
    include/enum.hpp
    include/forward_list.hpp
    include/fundamental.hpp
    include/header.hpp
    include/json_conformance.hpp
    include/list.hpp
    include/map.hpp
    include/multimap.hpp
    include/multiset.hpp
    include/one_func.hpp
    include/one_memfn.hpp
    include/optional.hpp
    include/pair.hpp
    include/serialization.hpp
    include/serialize.hpp
    include/set.hpp
    include/split_func.hpp
    include/split_memfn.hpp
    include/std_streams.hpp
    include/string.hpp
    include/string_view.hpp
    include/tuple.hpp
    include/u16string.hpp
    include/unordered_map.hpp
    include/unordered_multimap.hpp
    include/unordered_multiset.hpp
    include/unordered_set.hpp
    include/variant.hpp
    include/vector.hpp
    include/version.hpp
    include/wstring.hpp
    include/yas_object.hpp
    include/absl_cont_btree_map.hpp
    include/absl_cont_flat_hash_map.hpp
    main.cpp
)

add_executable(yas-base-test ${SOURCE_FILES})

if (YAS_SERIALIZE_ABSL_TYPES)
    target_link_libraries(yas-base-test 
        absl::btree
        absl::flat_hash_map
    )
endif()
