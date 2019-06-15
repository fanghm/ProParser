#include<iostream>
#include <fstream>
#include<string>
#include <sstream>
#include <vector>
#include <iterator>
#include <regex>
#include "defines.h"

using namespace std;

enum class CSVState {
    UnquotedField,
    QuotedField
};

class PrInfo {
public:
    PrInfo():transfer_times(0), pingpong_times(0) {}
    PrInfo(vector<string> fields) {
        this->pr_id         = fields[0];
        this->gic           = fields[4];
        this->rpt_date      = fields[3];
        this->author        = fields[11];
        this->author_grp    = fields[12];
        this->state         = fields[6];

        analyse_rev_his(fields[15]);
    }

    // calc Transfer/Solving Path, Transfer/Pingpong times
    void analyse_rev_his(string& revision) {
        string pattern = "The group in charge changed from ([[:upper:]_]+) to ([[:upper:]_]+)";
        regex r(pattern);

        bool first = true;
        for(sregex_iterator it(revision.begin(), revision.end(), r), end_it; it != end_it; ++it) {
            // cout << it->format("$1") << "|" << it->format("$2") << endl;
            if (first) {
                transfer_path.push_back(it->format("$2"));
                first = false;
            }
        }

        std::reverse(transfer_path.begin(),transfer_path.end());
        solving_path = transfer_path;
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
};

// globals
const char CSV_DELIMITER = ',';
CSVState state = CSVState::UnquotedField;
const string HEADER_START_FIELD = "Problem ID";

template<typename Out>
void split(const string &s, Out result, char delim = CSV_DELIMITER) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        *(result++) = item;
    }
}

vector<string> split(const string &s, char delim = CSV_DELIMITER) {
    vector<string> elems;
    split(s, back_inserter(elems), delim);
    return elems;
}

void show_fields(vector<string> &fields) {
    size_t n = 0;
    cout << "\t@@@>[FIELDS] ";
    for (string val: fields) {
        cout << (n >= EXPORT_FIELD_NUM ? "__UNDEF__" : EXPORT_FIELDS[n++]) << ":\t\t||" << val.substr(0, 50) << "||\n";
    }
    cout << "<@@@\n\n";
}

void show_table(vector<vector<string>> const &matrix) {
    size_t m = 0, n = 0;
	for (vector<string> row: matrix) {
        cout << "\n-->" << m++ << "<--\n";
        n=0;
		for (string val: row) {
			cout << (n >= EXPORT_FIELD_NUM ? "__UNDEFINED__" : EXPORT_FIELDS[n++]) << ":\t\t||" << val.substr(0, 50) << "||\n";
		}
	}
}

void serize_header(ofstream& json_file) {
    json_file << "{\n\t\"records\": [\n";
}

void serize_footer(ofstream& json_file, size_t total) {
    json_file << "\n\t],\n\t\"total\": " << total << "\n}";
    json_file.flush();
    json_file.close();
}

void serize_vector(ofstream& json_file, vector<string> groups) {
    bool first = true;
    for (string grp: groups) {
        if (!first)
            json_file << ",\n";
        else
            first = false;

        json_file << "\t\t\t\t\""  << grp << "\"";
    }

    json_file << JSON_ARRAY_SUFFIX;
}

void serize_record(ofstream& json_file, vector<vector<string>> &table) {
    bool first = true;
    for (vector<string> record: table) {
        if (!first)
            json_file << ",\n";
        else
            first = false;

        json_file << "\t\t{\n";
        json_file << "\t\t\t" << "\"PR ID\": \""            << record[0] << JSON_LINE_SUFFIX;
        json_file << "\t\t\t" << "\"Group In Charge\": \""  << record[4] << JSON_LINE_SUFFIX;
        json_file << "\t\t\t" << "\"Reported Date\": \""    << record[3] << JSON_LINE_SUFFIX;
        json_file << "\t\t\t" << "\"Author\": \""           << record[11] << JSON_LINE_SUFFIX;
        json_file << "\t\t\t" << "\"Author Group\": \""     << record[12] << JSON_LINE_SUFFIX;
        json_file << "\t\t\t" << "\"State\": \""            << record[6] << JSON_LINE_SUFFIX;

        json_file << "\t\t\t" << "\"Transfer Path\": [\n";
        serize_vector(json_file, analyse_rev_his(record[15]));

        json_file << "\t\t\t" << "\"Transfer times\": 1,\n";

        json_file << "\t\t\t" << "\"Solving Path\": [\n";
        serize_vector(json_file, analyse_rev_his(record[15]));

        json_file << "\t\t\t" << "\"Pingpong times\": 0,\n";
        json_file << "\t\t\t" << "\"Attached PRs\": \""     << record[13] << JSON_LINE_SUFFIX;
        json_file << "\t\t\t" << "\"Attached\": \"no\",\n";
        json_file << "\t\t\t" << "\"Attached To\": \"\"\n";
        json_file << "\t\t}";
    }
}

void serize_table(string& json_fname, vector<vector<string>> &table) {
    ofstream json_file(json_fname);
    serize_header(json_file);
    serize_record(json_file, table);
    serize_footer(json_file, table.size());
}

bool saveRecord(vector<string> &fields, vector<vector<string>> &table) {
    if (fields.size() != EXPORT_FIELD_NUM) {
        cout << "Size ERROR!" << endl;
        return false;
    }

    table.push_back(fields);
    fields.clear();

    return true;
}

// recursively parse a string to exact fields
void parse_row(const string &str, vector<string> &fields, vector<vector<string>> &table, bool righAfterQuotation = false) {
    cout << "\t@Handle " << ((state == CSVState::UnquotedField) ? "unquoted": "quoted") << "string: " << str.substr(0, 50) << "..." << endl;
    size_t pos = str.find_first_of('"');
    if (string::npos == pos) {
        if (state == CSVState::UnquotedField) {
            split(str, back_inserter(fields));
            show_fields(fields);

            if(!saveRecord(fields, table)) return;
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
            parse_row(str.substr(pos+1), fields, table, true);
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
                    parse_row(str.substr(p+2), fields, table, righAfterQuotation);
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
                    if(!saveRecord(fields, table)) return;
                } else {
                    // recursively handle right part
                    state = CSVState::UnquotedField;
                    parse_row(str.substr(pos+2), fields, table);    // TODO: better to find next comma after quotation mark
                }
            }
        }
    }
}

vector<vector<string> > parse_csv(string& file) {
    vector<vector<string> > table;
    vector<string> fields;
    string row;

    bool skip_header = true;
    ifstream in(file);
    while (!in.eof()) {
        getline(in, row);
        if (in.bad() || in.fail()) {
            break;
        }

        if (row.empty()) continue;

        if (skip_header) {
            if(0 == HEADER_START_FIELD.compare(row.substr(0, HEADER_START_FIELD.size()))) {
                // TODO: parser header?
                skip_header = false;
            }
            continue;
        }

        cout << "\n\n==>ROW: " << row << endl;
        parse_row(row, fields, table);
    }

    return table;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cout << "Usage: \n\t" << argv[0] << " <exported_pr_csv_file>\n" << endl;
        return -1;
    }

//	string pattern="The group in charge changed from ([[:upper:]_]+) to ([[:upper:]_]+)";
//	std::string str("Oulu) The group in charge changed from ECE_DEV_FOU_OAM_MZ to NIESSCBTSMZOAM. Reason for Transfer: Correction is available aready by cBTS team. Just to find the right CB., 2018-06-08 09:45 Hautala, Mika (Nokia - FI/Oulu) edited. Changed field(s): R&D Information, 2018-06-08 09:30 Romppanen, Reijo (Nokia - FI/Oulu) The group in charge changed from RCPSEC to ECE_DEV_FOU_OAM_MZ. Re");
//	regex r(pattern);
//
//	for(sregex_iterator it(str.begin(), str.end(), r), end_it; it!=end_it; ++it) {
//		std::cout << it->str() << std::endl;
//		cout << it->format("$1") << "|" << it->format("$2") << endl;
//	}
//
//    return 0;

    string csv_fname(argv[1]);
    string json_fname = csv_fname.substr(0, csv_fname.find_last_of('.')) + "_result_my.json";
    vector<vector<string>> table;

    table = parse_csv(csv_fname);
    //show_table(table);
    serize_table(json_fname, table);
//    for (size_t i=0; i<9; i++) {
//        table = parse_csv(SAMPLE_FILES[i]);
//        show_table(table);
//        getchar();
//    }
    return 0;
}
