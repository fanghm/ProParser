#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <iterator>
#include <regex>

using namespace std;

// globals
bool DEBUG = false;
const char CSV_DELIMITER = ',';

const string HEADER_START_FIELD = "Problem ID";
const std::string EXPORT_FIELDS[] = {
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

const size_t EXPORT_FIELD_NUM = sizeof(EXPORT_FIELDS) / sizeof(std::string);    //17

enum class CSVState {
    UnquotedField,
    QuotedField
};

CSVState state = CSVState::UnquotedField;

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

void show_fields(vector<string> &fields) {
    if (!DEBUG) return;

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
    PrInfo():_transfer_times(0), _pingpong_times(0) {}

    PrInfo(vector<string>& fields) {
        show_fields(fields);

        this->_pr_id         = fields[0];
        this->_gic           = fields[4];
        this->_rpt_date      = fields[3];
        this->_author        = fields[11];
        this->_author_grp    = fields[12];
        this->_state         = fields[6];
        this->_attached_prs  = fields[13];

        analyze_transfer(fields[15]);
        analyse_attach();

        if (DEBUG) this->jsonize(cout);
    }

    void jsonize(ostream& out) const {
        out << setw(4) << " " << "{\n";
        //out.setf(ios::unitbuf);

        jsonize_field(out, "PR ID",             this->_pr_id, true);
        jsonize_field(out, "Group In Charge",   this->_gic);
        jsonize_field(out, "Reported Date",     this->_rpt_date);
        jsonize_field(out, "Author",            this->_author);
        jsonize_field(out, "Author Group",      this->_author_grp);
        jsonize_field(out, "State",             this->_state);

        jsonize_field(out, "Transfer Path",     this->_transfer_path);
        jsonize_field(out, "Transfer times",    this->_transfer_times);
        jsonize_field(out, "Solving Path",      this->_solving_path);
        jsonize_field(out, "Pingpong times",    this->_pingpong_times);

        jsonize_field(out, "Attached PRs",      this->_attached_prs);
        jsonize_field(out, "Attached",          this->_attached);
        jsonize_field(out, "Attached To",       this->_attached_to);

        out << "\n" << setw(4) << " " << "}";
    }

private:
    // Analyze revision history to get Transfer/Solving Path & Transfer/Pingpong times
    void analyze_transfer(string& revision) {
        string pattern = "The group in charge changed from ([[:upper:]_[:digit:]]+) to ([[:upper:]_[:digit:]]+)";
        regex r(pattern);

        bool first = true;
        string last_group;
        for(sregex_iterator it(revision.begin(), revision.end(), r), end_it; it != end_it; ++it) {
            //cout << "@Transfer" << it->format("$1") << "->" << it->format("$2") << endl;
            if (first) {
                _transfer_path.push_back(it->format("$2"));
                _transfer_path.push_back(it->format("$1"));
                first = false;
            } else {
                if (0 != last_group.compare(it->format("$2")))
                    _transfer_path.push_back(it->format("$2"));

                _transfer_path.push_back(it->format("$1"));
            }
            last_group = it->format("$1");
        }

        if (_transfer_path.empty()) {
            _transfer_path.push_back(this->_gic);
            _solving_path.push_back(this->_gic);

            _transfer_times = 1;
            _pingpong_times = 0;
        } else {
            reverse(_transfer_path.begin(),_transfer_path.end());
            _transfer_times = _transfer_path.size();

            analyze_pingpong();
        }

        if (DEBUG) jsonize_field(cout, "@@path:\n", _transfer_path);
    }

    void analyse_resolving_path(map<string, size_t>& last_pingpong) {
        string terminator = _transfer_path.back();
        vector<string>::iterator it_beg = _transfer_path.begin();
        vector<string>::iterator it_end = find(_transfer_path.begin(), _transfer_path.end(), terminator);

        size_t pos = 0;
        size_t next = 0;
        for (vector<string>::iterator it = _transfer_path.begin(); it <= it_end;) {
            string path = *it;
            if (0 == terminator.compare(path)) {
                _solving_path.push_back(path);
                break;
            }

            if (last_pingpong.find(path) != last_pingpong.end()) {
                if (last_pingpong[path] > next) {
                    next = last_pingpong[path];
                    _solving_path.push_back(path);
                }
            } else {
                if (0 == next || pos > next)
                    _solving_path.push_back(path);
            }
            ++it;
            ++pos;
        }

        cout << "\n@Solving path:" << endl;
        for (string path: _solving_path)
            cout << path << endl;
    }

    // TODO: normalize path names
    void analyze_pingpong() {
        size_t pos = 0;
        _pingpong_times = 0;

        set<string> path_set;
        map<string, size_t> last_pingpong;  // the last occurrence (0-based) where a group pingpongs in trs path

        vector<string>::iterator it;
        set<string>::iterator iter;
        string path;
        for (it = _transfer_path.begin(); it != _transfer_path.end();) {
            path = *it;
            if (path_set.empty()) {
                path_set.insert(path);
            } else {
                iter = path_set.find(path);
                if (iter != path_set.end()) {
                    ++_pingpong_times;
                    last_pingpong[path] = pos;  // insert or update
                } else {
                    path_set.insert(path);
                }
            }
            ++it;
            ++pos;
        }

        cout << "\n@Pingpong:" << _pingpong_times << endl;
        for (const auto &pair : last_pingpong) {
            std::cout << pair.first << ": " << pair.second << '\n';
        }

        analyse_resolving_path(last_pingpong);
    }

    void analyse_attach() {
        this->_attached = "no";
        this->_attached_to = "";
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
    string _pr_id;
    string _gic;
    string _rpt_date;
    string _author;
    string _author_grp;
    string _state;

    vector<string> _transfer_path;
    vector<string> _solving_path;

    size_t _transfer_times;
    size_t _pingpong_times;

    string _attached_prs;
    string _attached;
    string _attached_to;
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
    inline void parse_str(const string& str, vector<string>& fields, bool righAfterQuotation = false);

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

        if (DEBUG) cout << "\n\nParse line: " << line << endl;
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
    if (DEBUG) cout << "\t@Handle " << ((state == CSVState::UnquotedField) ? "unquoted": "quoted") << "string: " << str.substr(0, 50) << "..." << endl;
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
                if (DEBUG) cout << "@DOUBLE QUOTATION: " << pos << "|" << p << endl;
                if (string::npos != p) {
                    if (righAfterQuotation) {
                        fields.push_back(str.substr(0, p+2));
                    } else {
                        fields.back() = fields.back() + "\n" + str.substr(0, p+2); // append this new row to the last field
                    }
                    parse_str(str.substr(p+2), fields, righAfterQuotation);
                } else {
                    cerr << "ERROR: no match double quotation mark at same line" << endl;
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

    string csv_fname(argv[1]);

    CsvParser parser;
    parser.parse(csv_fname);
//    parser.jsonize(cout);
//    return 0;

    string json_fname = csv_fname.substr(0, csv_fname.find_last_of('.')) + "_result_my.json";
    ofstream json_file(json_fname);
    parser.jsonize(json_file);
    json_file.close();

    return 0;
}
