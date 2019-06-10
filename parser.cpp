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
const string HEADER_START_FIELD = "Problem ID";
const char CSV_DELIMITER = ',';
CSVState state = CSVState::UnquotedField;

template<typename Out>
void split(const std::string &s, Out result, char delim = CSV_DELIMITER) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, std::back_inserter(elems), delim);
    return elems;
}

void show_fields(std::vector<std::string> &fields) {
    std::cout << "\t-->[FIELDS]";
    for (std::string val: fields) {
        std::cout << val.substr(0, 20) << "||";
    }
    std::cout << "<--\n";
}

void show_table(std::vector<std::vector<std::string>> const &matrix) {
    size_t m = 1, n = 1;
	for (std::vector<std::string> row: matrix) {
        std::cout << "\n-->" << m++ << "<--\n";
		for (std::string val: row) {
			std::cout << (n > EXPORT_FIELD_NUM ? "__UNDEFINED__" : EXPORT_FIELDS[n++]) << ":\t\t||" << val.substr(0, 50) << "||\n";
			++n;
		}
	}
}

void parse_row(const std::string &row, std::vector<std::string> &fields, std::vector<std::vector<std::string>> &table) {
    size_t pos = row.find_first_of('"');

    if (state == CSVState::UnquotedField) {
        split(row, std::back_inserter(fields));
        if ( string::npos != pos) {
            // TODO: remove starting quotation mark of last field
            state = CSVState::QuotedField;
        }
    } else if (state == CSVState::QuotedField) {
        if (string::npos == pos) {
            fields.back() = fields.back() + "\n" + row;
        } else {
            fields.back().append("\n" + row.substr(0, pos)); // remove closing quotation mark

            state = CSVState::UnquotedField;
            split(row.substr(pos+2), std::back_inserter(fields));// skip ",
        }
    }

    if (state == CSVState::UnquotedField) {   // && fields.size() == FIELD_COUNT
        table.push_back(fields);
        fields.clear();
    }
}

std::vector<std::vector<std::string> > parse_csv(std::string& file) {
    std::vector<std::vector<std::string> > table;
    std::vector<std::string> fields;
    std::string row;

    bool skip_header = true;
    std::ifstream in(file);
    while (!in.eof()) {
        std::getline(in, row);
        if (in.bad() || in.fail()) {
            break;
        }

        if (skip_header) {
            if(0 == HEADER_START_FIELD.compare(row.substr(0, HEADER_START_FIELD.size()))) {
                // TODO: get field number
                skip_header = false;
            }
            continue;
        }

        std::cout << "\n\n==>ROW: " << row << std::endl;
        parse_row(row, fields, table);
    }

    return table;
}

int main() {
    std::string filename = "./problem-1_no-transfer-no-attach.csv";
    std::vector<std::vector<std::string>> table = parse_csv(filename);
    show_table(table);

    return 0;
}
