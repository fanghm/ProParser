#include<iostream>
#include <fstream>
#include<string>
#include <sstream>
#include <vector>
#include <iterator>

using namespace std;

enum class CSVState {
    UnquotedField,
    QuotedField,
    QuotedQuote
};

// globals
const string HEADER_START_FIELD = "Problem ID";
CSVState state = CSVState::UnquotedField;

template<typename Out>
void split(const std::string &s, char delim, Out result) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}

void show_fields(std::vector<std::string> &fields) {
    std::cout << "\t";
    for (std::string val: fields) {
        std::cout << val.substr(0, 20) << "/";
    }
    std::cout << "<<-----\n";
}

void show_table(std::vector<std::vector<std::string>> const &matrix) {
    size_t n = 0;
	for (std::vector<std::string> row: matrix) {
        std::cout << "\n-->" << ++n << "<--\n";
		for (std::string val: row) {
			std::cout << val.substr(0, 50) << "\n";
		}
	}
}

void parse_row(const std::string &row, std::vector<std::string> &fields, std::vector<std::vector<std::string>> &table) {
    size_t pos = row.find_first_of('"');

    if (state == CSVState::UnquotedField) {
        split(row, ',', std::back_inserter(fields));
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
            split(row.substr(pos+2), ',', std::back_inserter(fields));// skip ",
        }
    }

    if (state == CSVState::UnquotedField) {   // && fields.size() == FIELD_COUNT
        table.push_back(fields);
        fields.clear();
    }
}

std::vector<std::vector<std::string> > parse_csv(std::string file) {
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
    string s = "Field, with \"embedded quote\"\n and, new line";
    std::vector<std::string> fields;
    split(s, ',', std::back_inserter(fields));
    for (std::string val: fields) {
        std::cout << val << "\n";
    }
    return 0;
}
