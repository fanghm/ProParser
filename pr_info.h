#ifndef PR_INFO_H_INCLUDED
#define PR_INFO_H_INCLUDED

#include <string>
#include <ostream>
#include <vector>
#include <map>

using namespace std;

class PrInfo {
public:
    PrInfo();
    PrInfo(vector<string>& fields);

    void jsonize(ostream& out) const;

private:
    void analyze_transfer(string& revision);
    void analyse_resolving_path(map<string, size_t>& last_pingpong);
    void analyze_pingpong();
    void analyse_attach();


    void jsonize_field(ostream& out, const string key, const string value, bool first = false) const;
    void jsonize_field(ostream& out, const string& key, size_t value, bool first = false) const;
    void jsonize_field(ostream& out, const string& key, const vector<string>& values) const;

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
#endif // PR_INFO_H_INCLUDED
