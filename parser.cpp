#include <iostream>
#include <iomanip>
#include <fstream>
#include<string>
#include <sstream>
#include <vector>
#include <iterator>
#include <regex>
#include "defines.h"

using namespace std;

// globals
const char CSV_DELIMITER = ',';
const size_t EXPORT_FIELD_NUM = 17;
const string HEADER_START_FIELD = "Problem ID";

enum class CSVState {
    UnquotedField,
    QuotedField
};

CSVState state = CSVState::UnquotedField;

template <typename ...Args>
inline std::string format_string(const char* format, Args... args) {
    constexpr size_t oldlen = BUFSIZ;
    char buffer[oldlen];  // 默认栈上的缓冲区

    size_t newlen = snprintf(&buffer[0], oldlen, format, args...);
    newlen++;  // 算上终止符'\0'

    if (newlen > oldlen) {  // 默认缓冲区不够大，从堆上分配
        std::vector<char> newbuffer(newlen);
        snprintf(newbuffer.data(), newlen, format, args...);
        return std::string(newbuffer.data());
    }

    return buffer;
}

void show_fields(vector<string> &fields) {
    size_t n = 0;
    cout << "\t@@@>[FIELDS] ";
    for (string val: fields) {
        cout << (n >= EXPORT_FIELD_NUM ? "__UNDEF__" : EXPORT_FIELDS[n++]) << ":\t\t||" << val.substr(0, 50) << "||\n";
    }
    cout << "<@@@\n\n";
}

/*--- Class to analyze, store and output field information ---*/
class PrInfo {
public:
    PrInfo():transfer_times(0), pingpong_times(0) {}

    PrInfo(vector<string>& fields) {
        cout << "###Within PrInfo: " << endl;
        show_fields(fields);

        this->pr_id         = fields[0];
        this->gic           = fields[4];
        this->rpt_date      = fields[3];
        this->author        = fields[11];
        this->author_grp    = fields[12];
        this->state         = fields[6];
        this->attached_prs  = fields[13];

        analyse_rev_his(fields[15]);
        analyse_attach();

        this->jsonize(cout);
        cout << "###Within PrInfo###" << endl;
    }

    void jsonize(ostream& out) const {
        out << setw(4) << " " << "{\n";
        //out.setf(ios::unitbuf);

        jsonize_field(out, "PR ID", this->pr_id, true);
        jsonize_field(out, "Group In Charge", this->gic);
        jsonize_field(out, "Reported Date", this->rpt_date);
        jsonize_field(out, "Author", this->author);
        jsonize_field(out, "Author Group", this->author_grp);
        jsonize_field(out, "State", this->state);

        jsonize_field(out, "Transfer Path", this->transfer_path);
        jsonize_field(out, "Transfer times", this->transfer_times);
        jsonize_field(out, "Solving Path", this->solving_path);
        jsonize_field(out, "Pingpong times", this->pingpong_times);

        jsonize_field(out, "Attached PRs", this->attached_prs);
        jsonize_field(out, "Attached", this->attached);
        jsonize_field(out, "Attached To", this->attached_to);

        out << "\n" << setw(4) << " " << "}";
    }

private:
    // Analyze revision history to get Transfer/Solving Path & Transfer/Pingpong times
    void analyse_rev_his(string& revision) {
        string pattern = "The group in charge changed from ([[:upper:]_[:digit:]]+) to ([[:upper:]_[:digit:]]+)";
        regex r(pattern);

        bool first = true;
        string last_group;
        for(sregex_iterator it(revision.begin(), revision.end(), r), end_it; it != end_it; ++it) {
            cout << "###Groups###" << it->format("$1") << "->" << it->format("$2") << endl;
            if (first) {
                transfer_path.push_back(it->format("$2"));
                transfer_path.push_back(it->format("$1"));
                first = false;
            } else {
                if (0 != last_group.compare(it->format("$2")))
                    transfer_path.push_back(it->format("$2"));

                transfer_path.push_back(it->format("$1"));
            }
            last_group = it->format("$1");
        }

        std::reverse(transfer_path.begin(),transfer_path.end());
        transfer_times = transfer_path.size();

        solving_path = transfer_path;   //TODO
        pingpong_times = 0;
        jsonize_field(cout, "@@path: ", transfer_path);//for dbg
    }

    void analyse_attach() {
        this->attached = "no";
        this->attached_to = "";
    }

    void jsonize_field(ostream& out, const string key, const string value, bool first = false) const {
        if (!first) out << ",\n";
        out << setw(6) << " " << format_string("\"%s\": \"%s\"", key.c_str(), value.c_str());
    }

    void jsonize_field(ostream& out, const string& key, size_t value, bool first = false) const {
        if (!first) out << ",\n";
        out << setw(6) << " " << format_string("\"%s\": %d", key.c_str(), value);
    }

    void jsonize_field(ostream& out, const string& key, const vector<string>& values) const {
        out << ",\n";
        out << setw(6) << " " << format_string("\"%s\": [\n", key.c_str());

        bool first = true;
        for (string grp: values) {
            if (!first)
                out << ",\n";
            else
                first = false;

            out << setw(8) << " " << format_string("\"%s\"", grp.c_str());
        }

        out << "\n" << setw(6) << " " << "]";
    }

private:
    string pr_id;
    string gic;
    string rpt_date;
    string author;
    string author_grp;
    string state;

    vector<string> transfer_path;
    vector<string> solving_path;

    size_t transfer_times;
    size_t pingpong_times;

    string attached_prs;
    string attached;
    string attached_to;
};

/*--- CSV file Parser ---*/
class CsvParser {
public:
    CsvParser() {}

    ~CsvParser() {
        for (PrInfo* pr: _pr_list)
            delete pr;
    }

    void parse(string& csv_fname);
    void jsonize(ostream& out) const;

private:
    bool save(vector<string>& fields);
    void parse_str(const string& str, vector<string>& fields, bool righAfterQuotation = false);

    template<typename Out>
    void split(const string &s, Out result, char delim = CSV_DELIMITER) const;

    // for output as json
    void jsonize_start(ostream& json_file) const;
    void jsonize_end(ostream& json_file, size_t total) const;

    // for debugging
    //void show_fields(vector<string> &fields) const;

private:
    vector<PrInfo*> _pr_list;
};

//===============================================================================
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

        cout << "\n\nParse line: " << line << endl;
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
        cout << "ERROR: failed to parse out correct field number!" << endl;
        return false;
    }

    _pr_list.push_back(new PrInfo(fields));
    //_prlist.front()->jsonize(cout);
    //getchar();  //dbg
    fields.clear();

    return true;
}

// recursively parse a string to exact fields
void CsvParser::parse_str(const string &str, vector<string>& fields, bool righAfterQuotation) {
    cout << "\t@Handle " << ((state == CSVState::UnquotedField) ? "unquoted": "quoted") << "string: " << str.substr(0, 50) << "..." << endl;
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
                cout << "######### DOUBLE QUOTATION #########" << pos << "|" << p << endl;
                if (string::npos != p) {
                    if (righAfterQuotation) {
                        fields.push_back(str.substr(0, p+2));
                    } else {
                        fields.back() = fields.back() + "\n" + str.substr(0, p+2); // append this new row to the last field
                    }
                    parse_str(str.substr(p+2), fields, righAfterQuotation);
                } else {
                    cout << "ERROR: no match double quotation mark at same line" << endl;
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

void jsonize_field(ostream& out, const string key, const string value, bool first = false) {
    if (!first) out << ",\n";
    out << setw(6) << " " << format_string("%s\": \"%s\"", key.c_str(), value.c_str());
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cout << "Usage: \n\t" << argv[0] << " <path_to_csv_file>\n" << endl;
        return -1;
    }

//    jsonize_field(cout, "TEST", "TEST-VALUE");
//    return 0;

    string csv_fname(argv[1]);
    string json_fname = csv_fname.substr(0, csv_fname.find_last_of('.')) + "_result_my.json";

    CsvParser parser;
    parser.parse(csv_fname);
    //parser.jsonize(cout);

    ofstream json_file(json_fname);
    parser.jsonize(json_file);
    json_file.close();

    return 0;
}
