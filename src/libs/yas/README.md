[![Build Status](https://travis-ci.org/niXman/yas.svg?branch=master)](https://travis-ci.org/niXman/yas) [![Build status](https://ci.appveyor.com/api/projects/status/55v27uvryu0qh8mc/branch/master?svg=true)](https://ci.appveyor.com/project/niXman/yas/branch/master)

# YAS
Yet Another Serialization

-![Time](https://github.com/thekvs/cpp-serializers/raw/master/images/time.png)

* YAS is created as a replacement of [boost.serialization](https://www.boost.org/doc/libs/1_67_0/libs/serialization/doc/index.html) because of its insufficient speed of serialization ([benchmark 1](https://github.com/thekvs/cpp-serializers), [benchmark 2](https://github.com/fraillt/cpp_serializers_benchmark))
* YAS is header only library
* YAS does not depend on third-party libraries or boost
* YAS require C++11 support
* YAS binary archives is endian independent

## Supported the following types of archives:
 - binary
 - text
 - json (not fully comply)

## Supported the following compilers:
 - GCC  : 4.8.5, ... - 32/64 bit
 - MinGW: 4.8.5, ... - 32/64 bit
 - Clang: 3.5, ... - 32/64 bit
 - Intel: (untested)
 - MSVC : 2017(in c++14 mode), ... - 32/64 bit
 - Emscripten: 1.38 (clang version 6.0.1)

## Samples
The easiest way to save and load some object or vars is to use the `yas::save()` and `yas::load()` functions like this:
```cpp
#include <yas/serialize.hpp>
#include <yas/std_types.hpp>

int main() {
    int a = 3, aa{};
    short b = 4, bb{};
    float c = 3.14, cc{};
    
    constexpr std::size_t flags = 
         yas::mem // IO type
        |yas::json; // IO format
    
    auto buf = yas::save<flags>(
        YAS_OBJECT("myobject", a, b, c)
    );
    
    // buf = {"a":3,"b":4,"c":3.14}
    
    yas::load<flags>(buf,
        YAS_OBJECT_NVP("myobject"
            ,("a", aa)
            ,("b", bb)
            ,("c", cc)
        )
    );
    // a == aa && b == bb && c == cc;
}
```
The IO type can be one of `yas::mem` or `yas::file`.
The IO format can be one of `yas::binary` or `yas::text` or `yas::json`.

The `YAS_OBJECT()`/`YAS_OBJECT_NVP()`/`YAS_OBJECT_STRUCT()`/`YAS_OBJECT_STRUCT_NVP()` macro are declared [here](https://github.com/niXman/yas/blob/master/include/yas/object.hpp), example use is [here](https://github.com/niXman/yas/blob/master/tests/base/include/yas_object.hpp).

More examples you can see [here](https://github.com/niXman/yas/blob/master/tests/base/include/serialize.hpp).

## TODO:
* protobuf/messagepack support
* limits
* objects versioning

## Support the project
You can support the YAS project by donating:
* BTC: 12rjx6prAxwJ1Aep6HuM54At9wDvSCDbSJ
* ETH: 0x62719DDEc96C513699a276107622C73F6cAcec47

## Serialization for the following types is supported:
 - [std::array](https://en.cppreference.com/w/cpp/container/array)
 - [std::bitset](https://en.cppreference.com/w/cpp/utility/bitset)
 - [std::chrono::duration](https://en.cppreference.com/w/cpp/chrono/duration)
 - [std::chrono::time_point](https://en.cppreference.com/w/cpp/chrono/time_point)
 - [std::complex](https://en.cppreference.com/w/cpp/numeric/complex)
 - [std::deque](https://en.cppreference.com/w/cpp/container/deque)
 - [std::forward_list](https://en.cppreference.com/w/cpp/container/forward_list)
 - [std::list](https://en.cppreference.com/w/cpp/container/list)
 - [std::map](https://en.cppreference.com/w/cpp/container/map)
 - [std::multimap](https://en.cppreference.com/w/cpp/container/multimap)
 - [std::multiset](https://en.cppreference.com/w/cpp/container/multiset)
 - [std::optional](https://en.cppreference.com/w/cpp/utility/optional)
 - [std::pair](https://en.cppreference.com/w/cpp/utility/pair)
 - [std::set](https://en.cppreference.com/w/cpp/container/set)
 - [std::string](https://en.cppreference.com/w/cpp/string/basic_string)
 - [std::string_view](https://en.cppreference.com/w/cpp/string/basic_string_view)
 - [std::tuple](https://en.cppreference.com/w/cpp/utility/tuple)
 - [std::unordered_map](https://en.cppreference.com/w/cpp/container/unordered_map)
 - [std::unordered_multimap](https://en.cppreference.com/w/cpp/container/unordered_multimap)
 - [std::unordered_multiset](https://en.cppreference.com/w/cpp/container/unordered_multiset)
 - [std::unordered_set](https://en.cppreference.com/w/cpp/container/unordered_set)
 - [std::variant](https://en.cppreference.com/w/cpp/utility/variant)
 - [std::vector](https://en.cppreference.com/w/cpp/container/vector)
 - [std::wstring](https://en.cppreference.com/w/cpp/string/basic_string)
 - [boost::array](https://www.boost.org/doc/libs/1_64_0/doc/html/array.html)
 - [boost::chrono::duration](https://www.boost.org/doc/libs/1_64_0/doc/html/chrono/reference.html#chrono.reference.cpp0x.duration_hpp.duration)
 - [boost::chrono::time_point](https://www.boost.org/doc/libs/1_64_0/doc/html/chrono/reference.html#chrono.reference.cpp0x.time_point_hpp.time_point)
 - [boost::optional](https://www.boost.org/doc/libs/1_64_0/libs/optional/doc/html/index.html)
 - [boost::variant](https://www.boost.org/doc/libs/1_64_0/doc/html/variant.html)
 - [boost::container::deque](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/deque.html)
 - [boost::container::string](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/basic_string.html)
 - [boost::container::wstring](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/basic_string.html)
 - [boost::container::vector](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/vector.html)
 - [boost::container::static_vector](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/static_vector.html)
 - [boost::container::stable_vector](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/stable_vector.html)
 - [boost::container::list](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/list.html)
 - [boost::container::slist](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/slist.html)
 - [boost::container::map](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/map.html)
 - [boost::container::multimap](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/multimap.html)
 - [boost::container::set](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/set.html)
 - [boost::container::multiset](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/multiset.html)
 - [boost::container::flat_map](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/flat_map.html)
 - [boost::container::flat_multimap](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/flat_multimap.html)
 - [boost::container::flat_set](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/flat_set.html)
 - [boost::container::flat_multiset](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/flat_multiset.html)
 - [boost::unordered_map](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/unordered_map.html)
 - [boost::unordered_multimap](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/unordered_multimap.html)
 - [boost::unordered_set](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/unordered_set.html)
 - [boost::unordered_multiset](https://www.boost.org/doc/libs/1_64_0/doc/html/boost/unordered_multiset.html)
 - [boost::fusion::pair](https://www.boost.org/doc/libs/1_64_0/libs/fusion/doc/html/fusion/support/pair.html)
 - [boost::fusion::tuple](https://www.boost.org/doc/libs/1_64_0/libs/fusion/doc/html/fusion/container/tuple.html)
 - [boost::fusion::vector](https://www.boost.org/doc/libs/1_64_0/libs/fusion/doc/html/fusion/container/vector.html)
 - [boost::fusion::list](https://www.boost.org/doc/libs/1_64_0/libs/fusion/doc/html/fusion/container/list.html)
 - [boost::fusion::map](https://www.boost.org/doc/libs/1_64_0/libs/fusion/doc/html/fusion/container/map.html)
 - [boost::fusion::set](https://www.boost.org/doc/libs/1_64_0/libs/fusion/doc/html/fusion/container/set.html)
 - [yas::intrusive_buffer](https://github.com/niXman/yas/blob/master/include/yas/buffers.hpp#L48) (only save)
 - [yas::shared_buffer](https://github.com/niXman/yas/blob/master/include/yas/buffers.hpp#L67)

## Projects using YAS
* [Ufochain](https://github.com/ufo-project/ufochain): a mimblewimble implementation of crypto currency using X17r algorithm
* [Kvant](https://github.com/KVANTdev/KVANT): Kvant - is an original project using the consensus of MimbleWimble, due to which maximum anonymity and security were achieved
* [zkPoD-lib](https://github.com/sec-bit/zkPoD-lib): zkPoD-lib is the underlying core library for zkPoD system. It fully implements PoD (proof of delivery) protocol and also provides a CLI interface together with Golang bindings
* [Litecash](https://github.com/teamlite/litecash): Litecash is the next generation scalable, confidential cryptocurrency based on an elegant and innovative Mimblewimble protocol
* [K3](https://github.com/DaMSL/K3): K3 is a programming language for building large-scale data systems
* [vistle](https://github.com/vistle/vistle): Software Environment for High-Performance Simulation and Parallel Visualization
* [LGraph](https://github.com/masc-ucsc/lgraph): Live Graph infrastructure for Synthesis and Simulation
* [Beam](https://github.com/BeamWM/beam): BEAM is a next generation scalable, confidential cryptocurrency based on an elegant and innovative Mimblewimble protocol
* [libfon9](https://github.com/fonwin/libfon9): C++11 Cross-platform infrastructure for Order Management System
* [iris-crypt](https://github.com/aspectron/iris-crypt): Store Node.js modules encrypted in a package file
* [cppan](https://github.com/tarasko/cppan): Class members annotations for C++
* [GeekSys company](http://www.geeksysgroup.com/en/): GeekSys is using YAS to serialize features from images
