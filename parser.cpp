#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include "defines.h"
#include "pr_info.h"
#include "parser.h"

using namespace std;

// globals
enum class CSVState {
    UnquotedField,
    QuotedField
};
CSVState state = CSVState::UnquotedField;
const string HEADER_START_FIELD = "Problem ID";

bool _DEBUG_ = false;   // true for debugging
void show_fields(vector<string>& fields) {
    if (!_DEBUG_) return;

    size_t n = 0;
    cout << "\t@@@>[FIELDS] ";
    for (string val: fields) {
        cout << (n >= EXPORT_FIELD_NUM ? "__UNDEF__" : EXPORT_FIELDS[n++]) << ":\t\t||"
            << setw(4) << val << "\n";
    }
    cout << "<@@@\n\n";
}

/* --- CsvParser class implementation ---*/
void CsvParser::jsonize(ostream& out) const {
    jsonize_start(out);

    bool first = true;
    for (PrInfo* pr_info: _pr_list) {
        if (!first)
            out << ",\n";
        else
            first = false;

        pr_info->jsonize(out);
    }

    jsonize_end(out, _pr_list.size());
}

CsvParser::~CsvParser() {
    reset();
}

void CsvParser::reset() {
    for (PrInfo* pr: _pr_list)
        delete pr;

    _pr_list.clear();
}

void CsvParser::jsonize_start(ostream& json_file) const {
    json_file << "{\n  \"records\": [\n";
}

void CsvParser::jsonize_end(ostream& json_file, size_t total) const {
    json_file << "\n  ],\n  \"total\": " << total << "\n}";
    json_file.flush();
}

void CsvParser::parse(string& csv_fname) {
    string line;
    vector<string> fields;

    bool skip_header = true;
    ifstream in(csv_fname);

    while (!in.eof()) {
        getline(in, line);
        if (in.bad() || in.fail()) {
            break;
        }

        if (line.empty()) continue;

        if (skip_header) {
            if(0 == HEADER_START_FIELD.compare(line.substr(0, HEADER_START_FIELD.size()))) {
                skip_header = false;
            }
            continue;
        }

        if (_DEBUG_) cout << "\n\nParse line: " << line << endl;
        parse_str(line, fields);
    }
}

template<typename Out>
void CsvParser::split(const string &s, Out result, char delim) const {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        *(result++) = item;
    }
}

bool CsvParser::save(vector<string> &fields) {
    if (fields.size() != EXPORT_FIELD_NUM) {
        cerr << "ERROR: failed to parse out correct field number!" << endl;
        for (auto field: fields)
            cerr << field << endl;

        return false;
    }

    _pr_list.push_back(new PrInfo(fields));
    //_prlist.front()->jsonize(cout); getchar();  // 4dbg
    fields.clear();

    return true;
}

// recursively parse a string to exact fields
void CsvParser::parse_str(const string &str, vector<string>& fields, bool righAfterQuotation) {
    if (_DEBUG_) cout << "\t@Parse " << ((state == CSVState::UnquotedField) ? "unquoted": "quoted") << " string: "
        << setw(30) << str << ((str.size() > 30) ? "..." : "") << endl;

    size_t pos = str.find_first_of('"');
    if (string::npos == pos) {
        if (state == CSVState::UnquotedField) {
            split(str, back_inserter(fields));
            show_fields(fields);

            if(!save(fields)) return;
        } else {
            if (righAfterQuotation) {
                fields.push_back(str);
                show_fields(fields);
            } else {
                fields.back() = fields.back() + "\n" + str; // append this new row to the last field
            }
        }
    } else {
        if (state == CSVState::UnquotedField) {
            // handle left part
            if (pos > 0)
                split(str.substr(0, pos), back_inserter(fields));

            // recursively handle right part
            state = CSVState::QuotedField;
            show_fields(fields);
            parse_str(str.substr(pos+1), fields, true);
        } else {
            if (str.size() > pos + 4 && str[pos+1] == '"') { // double quote, parse to end of it
                size_t p = str.find("\"\"", pos+2);
                if (_DEBUG_) cout << "@Double-quoted: " << pos << "|" << p << endl;

                if (string::npos != p) {
                    if (righAfterQuotation) {
                        fields.push_back(str.substr(0, p+2));
                    } else {
                        fields.back() = fields.back() + "\n" + str.substr(0, p+2); // append this new row to the last field
                    }
                    parse_str(str.substr(p+2), fields, righAfterQuotation);
                } else {
                    cerr << "ERROR: no match double quotation mark at same line" << endl;   // cross line?
                    return;
                }
            } else {
                // handle left part
                if (righAfterQuotation) {
                    fields.push_back(str.substr(0, pos));
                    show_fields(fields);
                } else {
                    fields.back() = fields.back() + "\n" + str.substr(0, pos); // append quotation-mark's left part to the last field
                }

                if (pos == str.size()-1) {    // quotation mark is at eol
                    state = CSVState::UnquotedField;
                    if(!save(fields)) return;
                } else {
                    // recursively handle right part
                    state = CSVState::UnquotedField;
                    parse_str(str.substr(pos+2), fields);    // TODO: better to find next comma after quotation mark
                }
            }
        }
    }
}

void CsvParser::jsonize_field(ostream& out, const string key, const string value, bool first) {
    if (!first) out << ",\n";
    out << setw(6) << " " << format_string("%s\": \"%s\"", key.c_str(), value.c_str());
}
