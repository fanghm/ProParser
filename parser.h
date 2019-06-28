#ifndef CSV_PARSER_H_INCLUDED
#define CSV_PARSER_H_INCLUDED
#include <string>
#include <ostream>
#include <vector>

using namespace std;

const char CSV_DELIMITER = ',';
class PrInfo;

/*--- CSV file Parser ---*/
class CsvParser {
public:
    CsvParser() {}
    ~CsvParser();

    void parse(string& csv_fname);
    void jsonize(ostream& out) const;
    void reset();

private:
    bool save(vector<string>& fields);
    inline void parse_str(const string& str, vector<string>& fields, bool righAfterQuotation = false);

    template<typename Out>
    void split(const string &s, Out result, char delim = CSV_DELIMITER) const;

    // for output as json
    void jsonize_start(ostream& json_file) const;
    void jsonize_end(ostream& json_file, size_t total) const;
    void jsonize_field(ostream& out, const string key, const string value, bool first = false);

private:
    vector<PrInfo*> _pr_list;
};

#endif // CSV_PARSER_H_INCLUDED
