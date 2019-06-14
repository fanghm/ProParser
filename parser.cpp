#include<iostream>
#include <fstream>
#include<string>
#include <sstream>
#include <vector>
#include <iterator>
#include "defines.h"

using namespace std;

enum class CSVState {
    UnquotedField,
    QuotedField,
    QuotedQuote
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

    string filename(argv[1]);
    vector<vector<string>> table;

    table = parse_csv(filename);
    show_table(table);

    for (size_t i=0; i<9; i++) {
        table = parse_csv(SAMPLE_FILES[i]);
        show_table(table);
        getchar();
    }
    return 0;
}
