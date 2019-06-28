#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <iterator>
#include <regex>
#include "pr_info.h"
#include "defines.h"

/*--- Class to analyze, store and output field information ---*/
PrInfo::PrInfo():_transfer_times(0), _pingpong_times(0) {
}

PrInfo::PrInfo(vector<string>& fields) {
    //show_fields(fields);

    this->_pr_id         = fields[0];
    this->_gic           = fields[4];
    this->_rpt_date      = fields[3];
    this->_author        = fields[11];
    this->_author_grp    = fields[12];
    this->_state         = fields[6];
    this->_attached_prs  = fields[13];

    analyze_transfer(fields[15]);
    analyse_attach();

    //if (_DEBUG_) this->jsonize(cout);
}

void PrInfo::jsonize(ostream& out) const {
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


// Analyze revision history to get Transfer/Solving Path & Transfer/Pingpong times
void PrInfo::analyze_transfer(string& revision) {
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

    //if (_DEBUG_) jsonize_field(cout, "@@path:\n", _transfer_path);
}

void PrInfo::analyse_resolving_path(map<string, size_t>& last_pingpong) {
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
void PrInfo::analyze_pingpong() {
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

void PrInfo::analyse_attach() {
    this->_attached = "no";
    this->_attached_to = "";
}

void PrInfo::jsonize_field(ostream& out, const string key, const string value, bool first) const {
    if (!first) out << ",\n";
    out << setw(6) << " " << format_string("\"%s\": \"%s\"", key.c_str(), value.c_str());
}

void PrInfo::jsonize_field(ostream& out, const string& key, size_t value, bool first) const {
    if (!first) out << ",\n";
    out << setw(6) << " " << format_string("\"%s\": %d", key.c_str(), value);
}

void PrInfo::jsonize_field(ostream& out, const string& key, const vector<string>& values) const {
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
