#include "app_helper.hpp"

#include <array>
#include <climits>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <tuple>
#include <utility>

class NotStreamable {};

class Streamable {};

std::ostream &operator<<(std::ostream &out, const Streamable &) { return out << "Streamable"; }

TEST(TypeTools, Streaming) {

    EXPECT_EQ(CLI::detail::to_string(NotStreamable{}), "");

    EXPECT_EQ(CLI::detail::to_string(Streamable{}), "Streamable");

    EXPECT_EQ(CLI::detail::to_string(5), "5");

    EXPECT_EQ(CLI::detail::to_string("string"), std::string("string"));
    EXPECT_EQ(CLI::detail::to_string(std::string("string")), std::string("string"));
}

TEST(TypeTools, tuple) {
    EXPECT_FALSE(CLI::detail::is_tuple_like<int>::value);
    EXPECT_FALSE(CLI::detail::is_tuple_like<std::vector<double>>::value);
    auto v = CLI::detail::is_tuple_like<std::tuple<double, int>>::value;
    EXPECT_TRUE(v);
    v = CLI::detail::is_tuple_like<std::tuple<double, double, double>>::value;
    EXPECT_TRUE(v);
}

TEST(TypeTools, type_size) {
    auto V = CLI::detail::type_count<int>::value;
    EXPECT_EQ(V, 1);
    V = CLI::detail::type_count<void>::value;
    EXPECT_EQ(V, 0);
    V = CLI::detail::type_count<std::vector<double>>::value;
    EXPECT_EQ(V, 1);
    V = CLI::detail::type_count<std::tuple<double, int>>::value;
    EXPECT_EQ(V, 2);
    V = CLI::detail::type_count<std::tuple<std::string, double, int>>::value;
    EXPECT_EQ(V, 3);
    V = CLI::detail::type_count<std::array<std::string, 5>>::value;
    EXPECT_EQ(V, 5);
    V = CLI::detail::type_count<std::vector<std::pair<std::string, double>>>::value;
    EXPECT_EQ(V, 2);
}

TEST(TypeTools, expected_count) {
    auto V = CLI::detail::expected_count<int>::value;
    EXPECT_EQ(V, 1);
    V = CLI::detail::expected_count<void>::value;
    EXPECT_EQ(V, 0);
    V = CLI::detail::expected_count<std::vector<double>>::value;
    EXPECT_EQ(V, CLI::detail::expected_max_vector_size);
    V = CLI::detail::expected_count<std::tuple<double, int>>::value;
    EXPECT_EQ(V, 1);
    V = CLI::detail::expected_count<std::tuple<std::string, double, int>>::value;
    EXPECT_EQ(V, 1);
    V = CLI::detail::expected_count<std::array<std::string, 5>>::value;
    EXPECT_EQ(V, 1);
    V = CLI::detail::expected_count<std::vector<std::pair<std::string, double>>>::value;
    EXPECT_EQ(V, CLI::detail::expected_max_vector_size);
}

TEST(Split, SimpleByToken) {
    auto out = CLI::detail::split("one.two.three", '.');
    ASSERT_EQ(3u, out.size());
    EXPECT_EQ("one", out.at(0));
    EXPECT_EQ("two", out.at(1));
    EXPECT_EQ("three", out.at(2));
}

TEST(Split, Single) {
    auto out = CLI::detail::split("one", '.');
    ASSERT_EQ(1u, out.size());
    EXPECT_EQ("one", out.at(0));
}

TEST(Split, Empty) {
    auto out = CLI::detail::split("", '.');
    ASSERT_EQ(1u, out.size());
    EXPECT_EQ("", out.at(0));
}

TEST(String, InvalidName) {
    EXPECT_TRUE(CLI::detail::valid_name_string("valid"));
    EXPECT_FALSE(CLI::detail::valid_name_string("-invalid"));
    EXPECT_TRUE(CLI::detail::valid_name_string("va-li-d"));
    EXPECT_FALSE(CLI::detail::valid_name_string("vali&d"));
    EXPECT_TRUE(CLI::detail::valid_name_string("_valid"));
    EXPECT_FALSE(CLI::detail::valid_name_string("/valid"));
    EXPECT_TRUE(CLI::detail::valid_name_string("vali?d"));
    EXPECT_TRUE(CLI::detail::valid_name_string("@@@@"));
    EXPECT_TRUE(CLI::detail::valid_name_string("b@d2?"));
    EXPECT_TRUE(CLI::detail::valid_name_string("2vali?d"));
}

TEST(StringTools, Modify) {
    int cnt{0};
    std::string newString = CLI::detail::find_and_modify("======", "=", [&cnt](std::string &str, std::size_t index) {
        if((++cnt) % 2 == 0) {
            str[index] = ':';
        }
        return index + 1;
    });
    EXPECT_EQ(newString, "=:=:=:");
}

TEST(StringTools, Modify2) {
    std::string newString =
        CLI::detail::find_and_modify("this is a string test", "is", [](std::string &str, std::size_t index) {
            if((index > 1) && (str[index - 1] != ' ')) {
                str[index] = 'a';
                str[index + 1] = 't';
            }
            return index + 1;
        });
    EXPECT_EQ(newString, "that is a string test");
}

TEST(StringTools, Modify3) {
    // this picks up 3 sets of 3 after the 'b' then collapses the new first set
    std::string newString = CLI::detail::find_and_modify("baaaaaaaaaa", "aaa", [](std::string &str, std::size_t index) {
        str.erase(index, 3);
        str.insert(str.begin(), 'a');
        return 0u;
    });
    EXPECT_EQ(newString, "aba");
}

TEST(StringTools, flagValues) {
    EXPECT_EQ(CLI::detail::to_flag_value("0"), -1);
    EXPECT_EQ(CLI::detail::to_flag_value("t"), 1);
    EXPECT_EQ(CLI::detail::to_flag_value("1"), 1);
    EXPECT_EQ(CLI::detail::to_flag_value("6"), 6);
    EXPECT_EQ(CLI::detail::to_flag_value("-6"), -6);
    EXPECT_EQ(CLI::detail::to_flag_value("false"), -1);
    EXPECT_EQ(CLI::detail::to_flag_value("YES"), 1);
    EXPECT_THROW(CLI::detail::to_flag_value("frog"), std::invalid_argument);
    EXPECT_THROW(CLI::detail::to_flag_value("q"), std::invalid_argument);
    EXPECT_EQ(CLI::detail::to_flag_value("NO"), -1);
    EXPECT_EQ(CLI::detail::to_flag_value("475555233"), 475555233);
}

TEST(StringTools, Validation) {
    EXPECT_TRUE(CLI::detail::isalpha(""));
    EXPECT_TRUE(CLI::detail::isalpha("a"));
    EXPECT_TRUE(CLI::detail::isalpha("abcd"));
    EXPECT_FALSE(CLI::detail::isalpha("_"));
    EXPECT_FALSE(CLI::detail::isalpha("2"));
    EXPECT_FALSE(CLI::detail::isalpha("test test"));
    EXPECT_FALSE(CLI::detail::isalpha("test "));
    EXPECT_FALSE(CLI::detail::isalpha(" test"));
    EXPECT_FALSE(CLI::detail::isalpha("test2"));
}

TEST(Trim, Various) {
    std::string s1{"  sdlfkj sdflk sd s  "};
    std::string a1{"sdlfkj sdflk sd s"};
    CLI::detail::trim(s1);
    EXPECT_EQ(a1, s1);

    std::string s2{" a \t"};
    CLI::detail::trim(s2);
    EXPECT_EQ("a", s2);

    std::string s3{" a \n"};
    CLI::detail::trim(s3);
    EXPECT_EQ("a", s3);

    std::string s4{" a b "};
    EXPECT_EQ("a b", CLI::detail::trim(s4));
}

TEST(Trim, VariousFilters) {
    std::string s1{"  sdlfkj sdflk sd s  "};
    std::string a1{"sdlfkj sdflk sd s"};
    CLI::detail::trim(s1, " ");
    EXPECT_EQ(a1, s1);

    std::string s2{" a \t"};
    CLI::detail::trim(s2, " ");
    EXPECT_EQ("a \t", s2);

    std::string s3{"abdavda"};
    CLI::detail::trim(s3, "a");
    EXPECT_EQ("bdavd", s3);

    std::string s4{"abcabcabc"};
    EXPECT_EQ("cabcabc", CLI::detail::trim(s4, "ab"));
}

TEST(Trim, TrimCopy) {
    std::string orig{" cabc  "};
    std::string trimmed = CLI::detail::trim_copy(orig);
    EXPECT_EQ("cabc", trimmed);
    EXPECT_NE(orig, trimmed);
    CLI::detail::trim(orig);
    EXPECT_EQ(trimmed, orig);

    orig = "abcabcabc";
    trimmed = CLI::detail::trim_copy(orig, "ab");
    EXPECT_EQ("cabcabc", trimmed);
    EXPECT_NE(orig, trimmed);
    CLI::detail::trim(orig, "ab");
    EXPECT_EQ(trimmed, orig);
}

TEST(Validators, FileExists) {
    std::string myfile{"TestFileNotUsed.txt"};
    EXPECT_FALSE(CLI::ExistingFile(myfile).empty());
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a'));  // create file
    EXPECT_TRUE(ok);
    EXPECT_TRUE(CLI::ExistingFile(myfile).empty());

    std::remove(myfile.c_str());
    EXPECT_FALSE(CLI::ExistingFile(myfile).empty());
}

TEST(Validators, FileNotExists) {
    std::string myfile{"TestFileNotUsed.txt"};
    EXPECT_TRUE(CLI::NonexistentPath(myfile).empty());
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a'));  // create file
    EXPECT_TRUE(ok);
    EXPECT_FALSE(CLI::NonexistentPath(myfile).empty());

    std::remove(myfile.c_str());
    EXPECT_TRUE(CLI::NonexistentPath(myfile).empty());
}

TEST(Validators, FileIsDir) {
    std::string mydir{"../tests"};
    EXPECT_NE(CLI::ExistingFile(mydir), "");
}

TEST(Validators, DirectoryExists) {
    std::string mydir{"../tests"};
    EXPECT_EQ(CLI::ExistingDirectory(mydir), "");
}

TEST(Validators, DirectoryNotExists) {
    std::string mydir{"nondirectory"};
    EXPECT_NE(CLI::ExistingDirectory(mydir), "");
}

TEST(Validators, DirectoryIsFile) {
    std::string myfile{"TestFileNotUsed.txt"};
    EXPECT_TRUE(CLI::NonexistentPath(myfile).empty());
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a'));  // create file
    EXPECT_TRUE(ok);
    EXPECT_FALSE(CLI::ExistingDirectory(myfile).empty());

    std::remove(myfile.c_str());
    EXPECT_TRUE(CLI::NonexistentPath(myfile).empty());
}

TEST(Validators, PathExistsDir) {
    std::string mydir{"../tests"};
    EXPECT_EQ(CLI::ExistingPath(mydir), "");
}

TEST(Validators, PathExistsFile) {
    std::string myfile{"TestFileNotUsed.txt"};
    EXPECT_FALSE(CLI::ExistingPath(myfile).empty());
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a'));  // create file
    EXPECT_TRUE(ok);
    EXPECT_TRUE(CLI::ExistingPath(myfile).empty());

    std::remove(myfile.c_str());
    EXPECT_FALSE(CLI::ExistingPath(myfile).empty());
}

TEST(Validators, PathNotExistsDir) {
    std::string mydir{"nonpath"};
    EXPECT_NE(CLI::ExistingPath(mydir), "");
}

TEST(Validators, IPValidate1) {
    std::string ip = "1.1.1.1";
    EXPECT_TRUE(CLI::ValidIPV4(ip).empty());
    ip = "224.255.0.1";
    EXPECT_TRUE(CLI::ValidIPV4(ip).empty());
    ip = "-1.255.0.1";
    EXPECT_FALSE(CLI::ValidIPV4(ip).empty());
    ip = "1.256.0.1";
    EXPECT_FALSE(CLI::ValidIPV4(ip).empty());
    ip = "1.256.0.1";
    EXPECT_FALSE(CLI::ValidIPV4(ip).empty());
    ip = "aaa";
    EXPECT_FALSE(CLI::ValidIPV4(ip).empty());
    ip = "1.2.3.abc";
    EXPECT_FALSE(CLI::ValidIPV4(ip).empty());
    ip = "11.22";
    EXPECT_FALSE(CLI::ValidIPV4(ip).empty());
}

TEST(Validators, PositiveValidator) {
    std::string num = "1.1.1.1";
    EXPECT_FALSE(CLI::PositiveNumber(num).empty());
    num = "1";
    EXPECT_TRUE(CLI::PositiveNumber(num).empty());
    num = "10000";
    EXPECT_TRUE(CLI::PositiveNumber(num).empty());
    num = "0";
    EXPECT_FALSE(CLI::PositiveNumber(num).empty());
    num = "+0.5";
    EXPECT_TRUE(CLI::PositiveNumber(num).empty());
    num = "-1";
    EXPECT_FALSE(CLI::PositiveNumber(num).empty());
    num = "-1.5";
    EXPECT_FALSE(CLI::PositiveNumber(num).empty());
    num = "a";
    EXPECT_FALSE(CLI::PositiveNumber(num).empty());
}

TEST(Validators, NonNegativeValidator) {
    std::string num = "1.1.1.1";
    EXPECT_FALSE(CLI::NonNegativeNumber(num).empty());
    num = "1";
    EXPECT_TRUE(CLI::NonNegativeNumber(num).empty());
    num = "10000";
    EXPECT_TRUE(CLI::NonNegativeNumber(num).empty());
    num = "0";
    EXPECT_TRUE(CLI::NonNegativeNumber(num).empty());
    num = "+0.5";
    EXPECT_TRUE(CLI::NonNegativeNumber(num).empty());
    num = "-1";
    EXPECT_FALSE(CLI::NonNegativeNumber(num).empty());
    num = "-1.5";
    EXPECT_FALSE(CLI::NonNegativeNumber(num).empty());
    num = "a";
    EXPECT_FALSE(CLI::NonNegativeNumber(num).empty());
}

TEST(Validators, NumberValidator) {
    std::string num = "1.1.1.1";
    EXPECT_FALSE(CLI::Number(num).empty());
    num = "1.7";
    EXPECT_TRUE(CLI::Number(num).empty());
    num = "10000";
    EXPECT_TRUE(CLI::Number(num).empty());
    num = "-0.000";
    EXPECT_TRUE(CLI::Number(num).empty());
    num = "+1.55";
    EXPECT_TRUE(CLI::Number(num).empty());
    num = "a";
    EXPECT_FALSE(CLI::Number(num).empty());
}

TEST(Validators, CombinedAndRange) {
    auto crange = CLI::Range(0, 12) & CLI::Range(4, 16);
    EXPECT_TRUE(crange("4").empty());
    EXPECT_TRUE(crange("12").empty());
    EXPECT_TRUE(crange("7").empty());

    EXPECT_FALSE(crange("-2").empty());
    EXPECT_FALSE(crange("2").empty());
    EXPECT_FALSE(crange("15").empty());
    EXPECT_FALSE(crange("16").empty());
    EXPECT_FALSE(crange("18").empty());
}

TEST(Validators, CombinedOrRange) {
    auto crange = CLI::Range(0, 4) | CLI::Range(8, 12);

    EXPECT_FALSE(crange("-2").empty());
    EXPECT_TRUE(crange("2").empty());
    EXPECT_FALSE(crange("5").empty());
    EXPECT_TRUE(crange("8").empty());
    EXPECT_TRUE(crange("12").empty());
    EXPECT_FALSE(crange("16").empty());
}

TEST(Validators, CombinedPaths) {
    std::string myfile{"TestFileNotUsed.txt"};
    EXPECT_FALSE(CLI::ExistingFile(myfile).empty());
    bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a'));  // create file
    EXPECT_TRUE(ok);

    std::string dir{"../tests"};
    std::string notpath{"nondirectory"};

    auto path_or_dir = CLI::ExistingPath | CLI::ExistingDirectory;
    EXPECT_TRUE(path_or_dir(dir).empty());
    EXPECT_TRUE(path_or_dir(myfile).empty());
    EXPECT_FALSE(path_or_dir(notpath).empty());

    auto file_or_dir = CLI::ExistingFile | CLI::ExistingDirectory;
    EXPECT_TRUE(file_or_dir(dir).empty());
    EXPECT_TRUE(file_or_dir(myfile).empty());
    EXPECT_FALSE(file_or_dir(notpath).empty());

    auto path_and_dir = CLI::ExistingPath & CLI::ExistingDirectory;
    EXPECT_TRUE(path_and_dir(dir).empty());
    EXPECT_FALSE(path_and_dir(myfile).empty());
    EXPECT_FALSE(path_and_dir(notpath).empty());

    auto path_and_file = CLI::ExistingFile & CLI::ExistingDirectory;
    EXPECT_FALSE(path_and_file(dir).empty());
    EXPECT_FALSE(path_and_file(myfile).empty());
    EXPECT_FALSE(path_and_file(notpath).empty());

    std::remove(myfile.c_str());
    EXPECT_FALSE(CLI::ExistingFile(myfile).empty());
}

TEST(Validators, ProgramNameSplit) {
    TempFile myfile{"program_name1.exe"};
    {
        std::ofstream out{myfile};
        out << "useless string doesn't matter" << std::endl;
    }
    auto res =
        CLI::detail::split_program_name(std::string("./") + std::string(myfile) + " this is a bunch of extra stuff  ");
    EXPECT_EQ(res.first, std::string("./") + std::string(myfile));
    EXPECT_EQ(res.second, "this is a bunch of extra stuff");

    TempFile myfile2{"program name1.exe"};
    {
        std::ofstream out{myfile2};
        out << "useless string doesn't matter" << std::endl;
    }
    res = CLI::detail::split_program_name(std::string("   ") + std::string("./") + std::string(myfile2) +
                                          "      this is a bunch of extra stuff  ");
    EXPECT_EQ(res.first, std::string("./") + std::string(myfile2));
    EXPECT_EQ(res.second, "this is a bunch of extra stuff");

    res = CLI::detail::split_program_name("./program_name    this is a bunch of extra stuff  ");
    EXPECT_EQ(res.first, "./program_name");  // test sectioning of first argument even if it can't detect the file
    EXPECT_EQ(res.second, "this is a bunch of extra stuff");

    res = CLI::detail::split_program_name(std::string("  ./") + std::string(myfile) + "    ");
    EXPECT_EQ(res.first, std::string("./") + std::string(myfile));
    EXPECT_TRUE(res.second.empty());
}

TEST(CheckedMultiply, Int) {
    int a{10};
    int b{-20};
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, -200);

    a = 0;
    b = -20;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, 0);

    a = 20;
    b = 0;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, 0);

    a = std::numeric_limits<int>::max();
    b = 1;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, std::numeric_limits<int>::max());

    a = std::numeric_limits<int>::max();
    b = 2;
    ASSERT_FALSE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, std::numeric_limits<int>::max());

    a = std::numeric_limits<int>::max();
    b = -1;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, -std::numeric_limits<int>::max());

    a = std::numeric_limits<int>::max();
    b = std::numeric_limits<int>::max();
    ASSERT_FALSE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, std::numeric_limits<int>::max());

    a = std::numeric_limits<int>::min();
    b = std::numeric_limits<int>::max();
    ASSERT_FALSE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, std::numeric_limits<int>::min());

    a = std::numeric_limits<int>::min();
    b = 1;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, std::numeric_limits<int>::min());

    a = std::numeric_limits<int>::min();
    b = -1;
    ASSERT_FALSE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, std::numeric_limits<int>::min());

    b = std::numeric_limits<int>::min();
    a = -1;
    ASSERT_FALSE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, -1);

    a = std::numeric_limits<int>::min() / 100;
    b = 99;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, std::numeric_limits<int>::min() / 100 * 99);

    a = std::numeric_limits<int>::min() / 100;
    b = -101;
    ASSERT_FALSE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, std::numeric_limits<int>::min() / 100);
    a = 2;
    b = std::numeric_limits<int>::min() / 2;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    a = std::numeric_limits<int>::min() / 2;
    b = 2;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));

    a = 4;
    b = std::numeric_limits<int>::min() / 4;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));

    a = 48;
    b = std::numeric_limits<int>::min() / 48;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
}

TEST(CheckedMultiply, SizeT) {
    std::size_t a = 10;
    std::size_t b = 20;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, 200u);

    a = 0u;
    b = 20u;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, 0u);

    a = 20u;
    b = 0u;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, 0u);

    a = std::numeric_limits<std::size_t>::max();
    b = 1u;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, std::numeric_limits<std::size_t>::max());

    a = std::numeric_limits<std::size_t>::max();
    b = 2u;
    ASSERT_FALSE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, std::numeric_limits<std::size_t>::max());

    a = std::numeric_limits<std::size_t>::max();
    b = std::numeric_limits<std::size_t>::max();
    ASSERT_FALSE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, std::numeric_limits<std::size_t>::max());

    a = std::numeric_limits<std::size_t>::max() / 100;
    b = 99u;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_EQ(a, std::numeric_limits<std::size_t>::max() / 100u * 99u);
}

TEST(CheckedMultiply, Float) {
    float a{10.0F};
    float b{20.0F};
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_FLOAT_EQ(a, 200);

    a = 0.0F;
    b = 20.0F;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_FLOAT_EQ(a, 0);

    a = INFINITY;
    b = 20.0F;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_FLOAT_EQ(a, INFINITY);

    a = 2.0F;
    b = -INFINITY;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_FLOAT_EQ(a, -INFINITY);

    a = std::numeric_limits<float>::max() / 100.0F;
    b = 1.0F;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_FLOAT_EQ(a, std::numeric_limits<float>::max() / 100.0F);

    a = std::numeric_limits<float>::max() / 100.0F;
    b = 99.0F;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_FLOAT_EQ(a, std::numeric_limits<float>::max() / 100.0F * 99.0F);

    a = std::numeric_limits<float>::max() / 100.0F;
    b = 101;
    ASSERT_FALSE(CLI::detail::checked_multiply(a, b));
    ASSERT_FLOAT_EQ(a, std::numeric_limits<float>::max() / 100.0F);

    a = std::numeric_limits<float>::max() / 100.0F;
    b = -99;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_FLOAT_EQ(a, std::numeric_limits<float>::max() / 100.0F * -99.0F);

    a = std::numeric_limits<float>::max() / 100.0F;
    b = -101;
    ASSERT_FALSE(CLI::detail::checked_multiply(a, b));
    ASSERT_FLOAT_EQ(a, std::numeric_limits<float>::max() / 100.0F);
}

TEST(CheckedMultiply, Double) {
    double a{10.0F};
    double b{20.0F};
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_DOUBLE_EQ(a, 200);

    a = 0;
    b = 20;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_DOUBLE_EQ(a, 0);

    a = INFINITY;
    b = 20;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_DOUBLE_EQ(a, INFINITY);

    a = 2;
    b = -INFINITY;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_DOUBLE_EQ(a, -INFINITY);

    a = std::numeric_limits<double>::max() / 100;
    b = 1;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_DOUBLE_EQ(a, std::numeric_limits<double>::max() / 100);

    a = std::numeric_limits<double>::max() / 100;
    b = 99;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_DOUBLE_EQ(a, std::numeric_limits<double>::max() / 100 * 99);

    a = std::numeric_limits<double>::max() / 100;
    b = 101;
    ASSERT_FALSE(CLI::detail::checked_multiply(a, b));
    ASSERT_DOUBLE_EQ(a, std::numeric_limits<double>::max() / 100);

    a = std::numeric_limits<double>::max() / 100;
    b = -99;
    ASSERT_TRUE(CLI::detail::checked_multiply(a, b));
    ASSERT_DOUBLE_EQ(a, std::numeric_limits<double>::max() / 100 * -99);

    a = std::numeric_limits<double>::max() / 100;
    b = -101;
    ASSERT_FALSE(CLI::detail::checked_multiply(a, b));
    ASSERT_DOUBLE_EQ(a, std::numeric_limits<double>::max() / 100);
}

// Yes, this is testing an app_helper :)
TEST(AppHelper, TempfileCreated) {
    std::string name = "TestFileNotUsed.txt";
    {
        TempFile myfile{name};

        EXPECT_FALSE(CLI::ExistingFile(myfile).empty());

        bool ok = static_cast<bool>(std::ofstream(myfile.c_str()).put('a'));  // create file
        EXPECT_TRUE(ok);
        EXPECT_TRUE(CLI::ExistingFile(name).empty());
        EXPECT_THROW({ TempFile otherfile(name); }, std::runtime_error);
    }
    EXPECT_FALSE(CLI::ExistingFile(name).empty());
}

TEST(AppHelper, TempfileNotCreated) {
    std::string name = "TestFileNotUsed.txt";
    {
        TempFile myfile{name};

        EXPECT_FALSE(CLI::ExistingFile(myfile).empty());
    }
    EXPECT_FALSE(CLI::ExistingFile(name).empty());
}

TEST(AppHelper, Ofstream) {

    std::string name = "TestFileNotUsed.txt";
    {
        TempFile myfile(name);

        {
            std::ofstream out{myfile};
            out << "this is output" << std::endl;
        }

        EXPECT_TRUE(CLI::ExistingFile(myfile).empty());
    }
    EXPECT_FALSE(CLI::ExistingFile(name).empty());
}

TEST(Split, StringList) {

    std::vector<std::string> results{"a", "long", "--lone", "-q"};
    EXPECT_EQ(results, CLI::detail::split_names("a,long,--lone,-q"));
    EXPECT_EQ(results, CLI::detail::split_names(" a, long, --lone, -q"));
    EXPECT_EQ(results, CLI::detail::split_names(" a , long , --lone , -q "));
    EXPECT_EQ(results, CLI::detail::split_names("   a  ,  long  ,  --lone  ,    -q  "));

    EXPECT_EQ(std::vector<std::string>({"one"}), CLI::detail::split_names("one"));
}

TEST(RegEx, Shorts) {
    std::string name, value;

    EXPECT_TRUE(CLI::detail::split_short("-a", name, value));
    EXPECT_EQ("a", name);
    EXPECT_EQ("", value);

    EXPECT_TRUE(CLI::detail::split_short("-B", name, value));
    EXPECT_EQ("B", name);
    EXPECT_EQ("", value);

    EXPECT_TRUE(CLI::detail::split_short("-cc", name, value));
    EXPECT_EQ("c", name);
    EXPECT_EQ("c", value);

    EXPECT_TRUE(CLI::detail::split_short("-simple", name, value));
    EXPECT_EQ("s", name);
    EXPECT_EQ("imple", value);

    EXPECT_FALSE(CLI::detail::split_short("--a", name, value));
    EXPECT_FALSE(CLI::detail::split_short("--thing", name, value));
    EXPECT_FALSE(CLI::detail::split_short("--", name, value));
    EXPECT_FALSE(CLI::detail::split_short("something", name, value));
    EXPECT_FALSE(CLI::detail::split_short("s", name, value));
}

TEST(RegEx, Longs) {
    std::string name, value;

    EXPECT_TRUE(CLI::detail::split_long("--a", name, value));
    EXPECT_EQ("a", name);
    EXPECT_EQ("", value);

    EXPECT_TRUE(CLI::detail::split_long("--thing", name, value));
    EXPECT_EQ("thing", name);
    EXPECT_EQ("", value);

    EXPECT_TRUE(CLI::detail::split_long("--some=thing", name, value));
    EXPECT_EQ("some", name);
    EXPECT_EQ("thing", value);

    EXPECT_FALSE(CLI::detail::split_long("-a", name, value));
    EXPECT_FALSE(CLI::detail::split_long("-things", name, value));
    EXPECT_FALSE(CLI::detail::split_long("Q", name, value));
    EXPECT_FALSE(CLI::detail::split_long("--", name, value));
}

TEST(RegEx, SplittingNew) {

    std::vector<std::string> shorts;
    std::vector<std::string> longs;
    std::string pname;

    EXPECT_NO_THROW(std::tie(shorts, longs, pname) = CLI::detail::get_names({"--long", "-s", "-q", "--also-long"}));
    EXPECT_EQ(std::vector<std::string>({"long", "also-long"}), longs);
    EXPECT_EQ(std::vector<std::string>({"s", "q"}), shorts);
    EXPECT_EQ("", pname);

    EXPECT_NO_THROW(std::tie(shorts, longs, pname) =
                        CLI::detail::get_names({"--long", "", "-s", "-q", "", "--also-long"}));
    EXPECT_EQ(std::vector<std::string>({"long", "also-long"}), longs);
    EXPECT_EQ(std::vector<std::string>({"s", "q"}), shorts);

    EXPECT_THROW(std::tie(shorts, longs, pname) = CLI::detail::get_names({"-"}), CLI::BadNameString);
    EXPECT_THROW(std::tie(shorts, longs, pname) = CLI::detail::get_names({"--"}), CLI::BadNameString);
    EXPECT_THROW(std::tie(shorts, longs, pname) = CLI::detail::get_names({"-hi"}), CLI::BadNameString);
    EXPECT_THROW(std::tie(shorts, longs, pname) = CLI::detail::get_names({"---hi"}), CLI::BadNameString);
    EXPECT_THROW(std::tie(shorts, longs, pname) = CLI::detail::get_names({"one", "two"}), CLI::BadNameString);
}

TEST(String, ToLower) { EXPECT_EQ("one and two", CLI::detail::to_lower("one And TWO")); }

TEST(Join, Forward) {
    std::vector<std::string> val{{"one", "two", "three"}};
    EXPECT_EQ("one,two,three", CLI::detail::join(val));
    EXPECT_EQ("one;two;three", CLI::detail::join(val, ";"));
}

TEST(Join, Backward) {
    std::vector<std::string> val{{"three", "two", "one"}};
    EXPECT_EQ("one,two,three", CLI::detail::rjoin(val));
    EXPECT_EQ("one;two;three", CLI::detail::rjoin(val, ";"));
}

TEST(SplitUp, Simple) {
    std::vector<std::string> oput = {"one", "two three"};
    std::string orig{R"(one "two three")"};
    std::vector<std::string> result = CLI::detail::split_up(orig);
    EXPECT_EQ(oput, result);
}

TEST(SplitUp, SimpleDifferentQuotes) {
    std::vector<std::string> oput = {"one", "two three"};
    std::string orig{R"(one `two three`)"};
    std::vector<std::string> result = CLI::detail::split_up(orig);
    EXPECT_EQ(oput, result);
}

TEST(SplitUp, SimpleDifferentQuotes2) {
    std::vector<std::string> oput = {"one", "two three"};
    std::string orig{R"(one 'two three')"};
    std::vector<std::string> result = CLI::detail::split_up(orig);
    EXPECT_EQ(oput, result);
}

TEST(SplitUp, Layered) {
    std::vector<std::string> output = {R"(one 'two three')"};
    std::string orig{R"("one 'two three'")"};
    std::vector<std::string> result = CLI::detail::split_up(orig);
    EXPECT_EQ(output, result);
}

TEST(SplitUp, Spaces) {
    std::vector<std::string> oput = {"one", "  two three"};
    std::string orig{R"(  one  "  two three" )"};
    std::vector<std::string> result = CLI::detail::split_up(orig);
    EXPECT_EQ(oput, result);
}

TEST(SplitUp, BadStrings) {
    std::vector<std::string> oput = {"one", "  two three"};
    std::string orig{R"(  one  "  two three )"};
    std::vector<std::string> result = CLI::detail::split_up(orig);
    EXPECT_EQ(oput, result);

    oput = {"one", "  two three"};
    orig = R"(  one  '  two three )";
    result = CLI::detail::split_up(orig);
    EXPECT_EQ(oput, result);
}

TEST(Types, TypeName) {
    std::string int_name = CLI::detail::type_name<int>();
    EXPECT_EQ("INT", int_name);

    std::string int2_name = CLI::detail::type_name<short>();
    EXPECT_EQ("INT", int2_name);

    std::string uint_name = CLI::detail::type_name<unsigned char>();
    EXPECT_EQ("UINT", uint_name);

    std::string float_name = CLI::detail::type_name<double>();
    EXPECT_EQ("FLOAT", float_name);

    std::string vector_name = CLI::detail::type_name<std::vector<int>>();
    EXPECT_EQ("INT", vector_name);

    vector_name = CLI::detail::type_name<std::vector<double>>();
    EXPECT_EQ("FLOAT", vector_name);

    static_assert(CLI::detail::classify_object<std::pair<int, std::string>>::value ==
                      CLI::detail::object_category::tuple_value,
                  "pair<int,string> does not read like a tuple");

    std::string pair_name = CLI::detail::type_name<std::vector<std::pair<int, std::string>>>();
    EXPECT_EQ("[INT,TEXT]", pair_name);

    vector_name = CLI::detail::type_name<std::vector<std::vector<unsigned char>>>();
    EXPECT_EQ("UINT", vector_name);

    auto vclass = CLI::detail::classify_object<std::vector<std::vector<unsigned char>>>::value;
    EXPECT_EQ(vclass, CLI::detail::object_category::vector_value);

    auto tclass = CLI::detail::classify_object<std::tuple<double>>::value;
    EXPECT_EQ(tclass, CLI::detail::object_category::number_constructible);

    std::string tuple_name = CLI::detail::type_name<std::tuple<double>>();
    EXPECT_EQ("FLOAT", tuple_name);

    static_assert(CLI::detail::classify_object<std::tuple<int, std::string>>::value ==
                      CLI::detail::object_category::tuple_value,
                  "tuple<int,string> does not read like a tuple");
    tuple_name = CLI::detail::type_name<std::tuple<int, std::string>>();
    EXPECT_EQ("[INT,TEXT]", tuple_name);

    tuple_name = CLI::detail::type_name<std::tuple<int, std::string, double>>();
    EXPECT_EQ("[INT,TEXT,FLOAT]", tuple_name);

    tuple_name = CLI::detail::type_name<std::tuple<int, std::string, double, unsigned int>>();
    EXPECT_EQ("[INT,TEXT,FLOAT,UINT]", tuple_name);

    tuple_name = CLI::detail::type_name<std::tuple<int, std::string, double, unsigned int, std::string>>();
    EXPECT_EQ("[INT,TEXT,FLOAT,UINT,TEXT]", tuple_name);

    tuple_name = CLI::detail::type_name<std::array<int, 10>>();
    EXPECT_EQ("[INT,INT,INT,INT,INT,INT,INT,INT,INT,INT]", tuple_name);

    std::string text_name = CLI::detail::type_name<std::string>();
    EXPECT_EQ("TEXT", text_name);

    std::string text2_name = CLI::detail::type_name<char *>();
    EXPECT_EQ("TEXT", text2_name);

    enum class test { test1, test2, test3 };
    std::string enum_name = CLI::detail::type_name<test>();
    EXPECT_EQ("ENUM", enum_name);

    vclass = CLI::detail::classify_object<std::tuple<test>>::value;
    EXPECT_EQ(vclass, CLI::detail::object_category::tuple_value);
    static_assert(CLI::detail::classify_object<std::tuple<test>>::value == CLI::detail::object_category::tuple_value,
                  "tuple<test> does not classify as a tuple");
    std::string enum_name2 = CLI::detail::type_name<std::tuple<test>>();
    EXPECT_EQ("ENUM", enum_name2);
}

TEST(Types, OverflowSmall) {
    signed char x;
    auto strmax = std::to_string(SCHAR_MAX + 1);
    EXPECT_FALSE(CLI::detail::lexical_cast(strmax, x));

    unsigned char y;
    strmax = std::to_string(UINT8_MAX + 1);
    EXPECT_FALSE(CLI::detail::lexical_cast(strmax, y));
}

TEST(Types, LexicalCastInt) {
    std::string signed_input = "-912";
    int x_signed;
    EXPECT_TRUE(CLI::detail::lexical_cast(signed_input, x_signed));
    EXPECT_EQ(-912, x_signed);

    std::string unsigned_input = "912";
    unsigned int x_unsigned;
    EXPECT_TRUE(CLI::detail::lexical_cast(unsigned_input, x_unsigned));
    EXPECT_EQ((unsigned int)912, x_unsigned);

    EXPECT_FALSE(CLI::detail::lexical_cast(signed_input, x_unsigned));

    unsigned char y;
    std::string overflow_input = std::to_string(UINT64_MAX) + "0";
    EXPECT_FALSE(CLI::detail::lexical_cast(overflow_input, y));

    char y_signed;
    EXPECT_FALSE(CLI::detail::lexical_cast(overflow_input, y_signed));

    std::string bad_input = "hello";
    EXPECT_FALSE(CLI::detail::lexical_cast(bad_input, y));

    std::string extra_input = "912i";
    EXPECT_FALSE(CLI::detail::lexical_cast(extra_input, y));
}

TEST(Types, LexicalCastDouble) {
    std::string input = "9.12";
    long double x;
    EXPECT_TRUE(CLI::detail::lexical_cast(input, x));
    EXPECT_FLOAT_EQ((float)9.12, (float)x);

    std::string bad_input = "hello";
    EXPECT_FALSE(CLI::detail::lexical_cast(bad_input, x));

    std::string overflow_input = "1" + std::to_string(LDBL_MAX);
    EXPECT_FALSE(CLI::detail::lexical_cast(overflow_input, x));

    std::string extra_input = "9.12i";
    EXPECT_FALSE(CLI::detail::lexical_cast(extra_input, x));
}

TEST(Types, LexicalCastBool) {
    std::string input = "false";
    bool x;
    EXPECT_TRUE(CLI::detail::lexical_cast(input, x));
    EXPECT_FALSE(x);

    std::string bad_input = "happy";
    EXPECT_FALSE(CLI::detail::lexical_cast(bad_input, x));

    std::string input_true = "EnaBLE";
    EXPECT_TRUE(CLI::detail::lexical_cast(input_true, x));
    EXPECT_TRUE(x);
}

TEST(Types, LexicalCastString) {
    std::string input = "one";
    std::string output;
    CLI::detail::lexical_cast(input, output);
    EXPECT_EQ(input, output);
}

TEST(Types, LexicalCastParsable) {
    std::string input = "(4.2,7.3)";
    std::string fail_input = "4.2,7.3";
    std::string extra_input = "(4.2,7.3)e";

    std::complex<double> output;
    EXPECT_TRUE(CLI::detail::lexical_cast(input, output));
    EXPECT_DOUBLE_EQ(output.real(), 4.2);  // Doing this in one go sometimes has trouble
    EXPECT_DOUBLE_EQ(output.imag(), 7.3);  // on clang + c++4.8 due to missing const

    EXPECT_TRUE(CLI::detail::lexical_cast("2.456", output));
    EXPECT_DOUBLE_EQ(output.real(), 2.456);  // Doing this in one go sometimes has trouble
    EXPECT_DOUBLE_EQ(output.imag(), 0.0);    // on clang + c++4.8 due to missing const

    EXPECT_FALSE(CLI::detail::lexical_cast(fail_input, output));
    EXPECT_FALSE(CLI::detail::lexical_cast(extra_input, output));
}

TEST(Types, LexicalCastEnum) {
    enum t1 : signed char { v1 = 5, v3 = 7, v5 = -9 };

    t1 output;
    EXPECT_TRUE(CLI::detail::lexical_cast("-9", output));
    EXPECT_EQ(output, v5);

    EXPECT_FALSE(CLI::detail::lexical_cast("invalid", output));
    enum class t2 : std::uint64_t { enum1 = 65, enum2 = 45667, enum3 = 9999999999999 };
    t2 output2{t2::enum2};
    EXPECT_TRUE(CLI::detail::lexical_cast("65", output2));
    EXPECT_EQ(output2, t2::enum1);

    EXPECT_FALSE(CLI::detail::lexical_cast("invalid", output2));

    EXPECT_TRUE(CLI::detail::lexical_cast("9999999999999", output2));
    EXPECT_EQ(output2, t2::enum3);
}

TEST(Types, LexicalConversionDouble) {
    CLI::results_t input = {"9.12"};
    long double x{0.0};
    bool res = CLI::detail::lexical_conversion<long double, double>(input, x);
    EXPECT_TRUE(res);
    EXPECT_FLOAT_EQ((float)9.12, (float)x);

    CLI::results_t bad_input = {"hello"};
    res = CLI::detail::lexical_conversion<long double, double>(input, x);
    EXPECT_TRUE(res);
}

TEST(Types, LexicalConversionDoubleTuple) {
    CLI::results_t input = {"9.12"};
    std::tuple<double> x{0.0};
    bool res = CLI::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    EXPECT_TRUE(res);
    EXPECT_DOUBLE_EQ(9.12, std::get<0>(x));

    CLI::results_t bad_input = {"hello"};
    res = CLI::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    EXPECT_TRUE(res);
}

TEST(Types, LexicalConversionVectorDouble) {
    CLI::results_t input = {"9.12", "10.79", "-3.54"};
    std::vector<double> x;
    bool res = CLI::detail::lexical_conversion<std::vector<double>, double>(input, x);
    EXPECT_TRUE(res);
    EXPECT_EQ(x.size(), 3u);
    EXPECT_DOUBLE_EQ(x[2], -3.54);

    res = CLI::detail::lexical_conversion<std::vector<double>, std::vector<double>>(input, x);
    EXPECT_TRUE(res);
    EXPECT_EQ(x.size(), 3u);
    EXPECT_DOUBLE_EQ(x[2], -3.54);
}

static_assert(!CLI::detail::is_tuple_like<std::vector<double>>::value, "vector should not be like a tuple");
static_assert(CLI::detail::is_tuple_like<std::pair<double, double>>::value, "pair of double should be like a tuple");
static_assert(CLI::detail::is_tuple_like<std::array<double, 4>>::value, "std::array<double,4> should be like a tuple");
static_assert(CLI::detail::is_tuple_like<std::array<int, 10>>::value, "std::array<int,10> should be like a tuple");
static_assert(!CLI::detail::is_tuple_like<std::string>::value, "std::string should not be like a tuple");
static_assert(!CLI::detail::is_tuple_like<double>::value, "double should not be like a tuple");
static_assert(CLI::detail::is_tuple_like<std::tuple<double, int, double>>::value, "tuple should look like a tuple");

TEST(Types, LexicalConversionTuple2) {
    CLI::results_t input = {"9.12", "19"};

    std::tuple<double, int> x{0.0, 0};
    static_assert(CLI::detail::is_tuple_like<decltype(x)>::value,
                  "tuple type must have is_tuple_like trait to be true");
    bool res = CLI::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    EXPECT_TRUE(res);
    EXPECT_EQ(std::get<1>(x), 19);
    EXPECT_DOUBLE_EQ(std::get<0>(x), 9.12);

    input = {"19", "9.12"};
    res = CLI::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    EXPECT_FALSE(res);
}

TEST(Types, LexicalConversionTuple3) {
    CLI::results_t input = {"9.12", "19", "hippo"};
    std::tuple<double, int, std::string> x;
    bool res = CLI::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    EXPECT_TRUE(res);
    EXPECT_EQ(std::get<1>(x), 19);
    EXPECT_DOUBLE_EQ(std::get<0>(x), 9.12);
    EXPECT_EQ(std::get<2>(x), "hippo");

    input = {"19", "9.12"};
    res = CLI::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    EXPECT_FALSE(res);
}

TEST(Types, LexicalConversionTuple4) {
    CLI::results_t input = {"9.12", "19", "18.6", "5.87"};
    std::array<double, 4> x;
    bool res = CLI::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    EXPECT_TRUE(res);
    EXPECT_DOUBLE_EQ(std::get<1>(x), 19);
    EXPECT_DOUBLE_EQ(x[0], 9.12);
    EXPECT_DOUBLE_EQ(x[2], 18.6);
    EXPECT_DOUBLE_EQ(x[3], 5.87);

    input = {"19", "9.12", "hippo"};
    res = CLI::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    EXPECT_FALSE(res);
}

TEST(Types, LexicalConversionTuple5) {
    CLI::results_t input = {"9", "19", "18", "5", "235235"};
    std::array<unsigned int, 5> x;
    bool res = CLI::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    EXPECT_TRUE(res);
    EXPECT_EQ(std::get<1>(x), 19u);
    EXPECT_EQ(x[0], 9u);
    EXPECT_EQ(x[2], 18u);
    EXPECT_EQ(x[3], 5u);
    EXPECT_EQ(x[4], 235235u);

    input = {"19", "9.12", "hippo"};
    res = CLI::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    EXPECT_FALSE(res);
}

TEST(Types, LexicalConversionTuple10) {
    CLI::results_t input = {"9", "19", "18", "5", "235235", "9", "19", "18", "5", "235235"};
    std::array<unsigned int, 10> x;
    bool res = CLI::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    EXPECT_TRUE(res);
    EXPECT_EQ(std::get<1>(x), 19u);
    EXPECT_EQ(x[0], 9u);
    EXPECT_EQ(x[2], 18u);
    EXPECT_EQ(x[3], 5u);
    EXPECT_EQ(x[4], 235235u);
    EXPECT_EQ(x[9], 235235u);
    input[3] = "hippo";
    res = CLI::detail::lexical_conversion<decltype(x), decltype(x)>(input, x);
    EXPECT_FALSE(res);
}

TEST(Types, LexicalConversionTuple10XC) {
    CLI::results_t input = {"9", "19", "18", "5", "235235", "9", "19", "18", "5", "235235"};
    std::array<double, 10> x;
    bool res = CLI::detail::lexical_conversion<decltype(x), std::array<unsigned int, 10>>(input, x);

    EXPECT_TRUE(res);
    EXPECT_EQ(std::get<1>(x), 19.0);
    EXPECT_EQ(x[0], 9.0);
    EXPECT_EQ(x[2], 18.0);
    EXPECT_EQ(x[3], 5.0);
    EXPECT_EQ(x[4], 235235.0);
    EXPECT_EQ(x[9], 235235.0);
    input[3] = "19.7";
    res = CLI::detail::lexical_conversion<decltype(x), std::array<unsigned int, 10>>(input, x);
    EXPECT_FALSE(res);
}

TEST(Types, LexicalConversionComplex) {
    CLI::results_t input = {"5.1", "3.5"};
    std::complex<double> x;
    bool res = CLI::detail::lexical_conversion<std::complex<double>, std::array<double, 2>>(input, x);
    EXPECT_TRUE(res);
    EXPECT_EQ(x.real(), 5.1);
    EXPECT_EQ(x.imag(), 3.5);
}

TEST(FixNewLines, BasicCheck) {
    std::string input = "one\ntwo";
    std::string output = "one\n; two";
    std::string result = CLI::detail::fix_newlines("; ", input);
    EXPECT_EQ(result, output);
}

TEST(FixNewLines, EdgesCheck) {
    std::string input = "\none\ntwo\n";
    std::string output = "\n; one\n; two\n; ";
    std::string result = CLI::detail::fix_newlines("; ", input);
    EXPECT_EQ(result, output);
}
