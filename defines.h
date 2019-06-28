#ifndef DEFINES_H_INCLUDED
#define DEFINES_H_INCLUDED
#include <string>
#include <sstream>
#include <vector>

using namespace std;

const string EXPORT_FIELDS[] = {
    "Problem ID",       //0: PR ID
    "Title",
    "Description",
    "Reported Date",    //3
    "Group in Charge",  //4
    "Feature",
    "State",            //6
    "Severity",
    "Top Importance",
    "Release",
    "R&D Information",
    "Author",           //11
    "Author Group",     //12
    "Attached PRs",     //13
    "Responsible Person",
    "Revision History", //15
    "Target Build"
};

const size_t EXPORT_FIELD_NUM = sizeof(EXPORT_FIELDS) / sizeof(string);    //17

const string TEST_SAMPLES[] = {
    "../Sample/problem-1_no-transfer-no-attach.csv",
    "../Sample/problem-1_simple.csv",
    "../Sample/problem-2_transfer-without-pingpong-no-attach.csv",
    "../Sample/problem-3_transfer-with-pingpong-no-attach.csv",
    "../Sample/problem-4_many-transfers-no-attach.csv",
    "../Sample/problem-5_attached-no-transfer-no-dangling.csv",
    "../Sample/problem-6_attach-no-transfer-with-dangling-references.csv",
    "../Sample/problem-7_attached-with-transfer-no-pingpong.csv",
    "../Sample/problem-8_attached-with-transfer-pingpong-exist-for-sure.csv",
    "../Sample/problem-9_attached-pingpong-combined.csv"
};


template <typename ...Args>
inline string format_string(const char* format, Args... args) {
    constexpr size_t len = BUFSIZ;
    char buffer[len];  // def buf on stack

    size_t length = snprintf(&buffer[0], len, format, args...);
    length++;   // count in '\0'

    if (length > len) {  // alloc from heap when def buf insufficient
        vector<char> buf(length);
        snprintf(buf.data(), length, format, args...);
        return string(buf.data());
    }

    return buffer;
}



#endif // DEFINES_H_INCLUDED
