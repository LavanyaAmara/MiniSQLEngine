#include <bits/stdc++.h>
using namespace std;

/*
 Mini SQL engine (in-memory)
 - Supports: CREATE TABLE, INSERT INTO, SELECT ... FROM ... WHERE ...
 - Types: INT, TEXT
 - WHERE supports multiple conditions joined by AND
 - Values are stored as strings internally; conversion done for INT comparisons
*/

enum ColType { CT_INT, CT_TEXT };

struct Table {
    string name;
    vector<string> cols;
    vector<ColType> types;
    vector<vector<string>> rows; // each row: vector of column values as strings
};

unordered_map<string, Table> db;

static inline string trim(const string &s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) ++a;
    while (b > a && isspace((unsigned char)s[b-1])) --b;
    return s.substr(a, b-a);
}

// basic lower-case
static inline string lower(const string &s) {
    string r = s;
    for (auto &c: r) c = tolower((unsigned char)c);
    return r;
}

// simple tokenizer (keeps quoted strings)
vector<string> tokenize(const string &sql) {
    vector<string> toks;
    int n = sql.size();
    for (int i = 0; i < n;) {
        if (isspace((unsigned char)sql[i])) { ++i; continue; }
        if (sql[i] == '\'' ) {
            // quoted string until next single quote
            int j = i+1;
            string t;
            while (j < n) {
                if (sql[j] == '\'') { ++j; break; }
                t.push_back(sql[j++]);
            }
            toks.push_back("'" + t + "'");
            i = j;
        } else if (ispunct((unsigned char)sql[i]) && sql[i] != '_') {
            // punctuation: , ( ) ; = ! < >
            // handle two-char operators !=, <=, >=
            if (i+1 < n) {
                string two = sql.substr(i,2);
                if (two == "!=" || two == "<=" || two == ">=") {
                    toks.push_back(two);
                    i+=2; continue;
                }
            }
            string one(1, sql[i]);
            toks.push_back(one);
            ++i;
        } else {
            int j = i;
            string t;
            while (j < n && (isalnum((unsigned char)sql[j]) || sql[j] == '_' || sql[j] == '.')) {
                t.push_back(sql[j++]);
            }
            toks.push_back(t);
            i = j;
        }
    }
    return toks;
}

bool isIntegerString(const string &s) {
    if (s.empty()) return false;
    int i = 0;
    if (s[0] == '+' || s[0]=='-') i = 1;
    for (; i < (int)s.size(); ++i) if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

// Condition struct: col op literal
struct Cond {
    string col;
    string op;
    string lit; // literal as string (no surrounding quotes)
};

// Evaluate single condition on a row
bool evalCond(const Cond &c, const Table &t, const vector<string> &row) {
    // find column index
    auto it = find(t.cols.begin(), t.cols.end(), c.col);
    if (it == t.cols.end()) return false;
    int idx = int(it - t.cols.begin());
    const string &val = row[idx];
    ColType ct = t.types[idx];

    // literal might be quoted (we stored as raw content in parsing), use ct to interpret
    if (ct == CT_INT) {
        // compare as integer
        if (!isIntegerString(val) || !isIntegerString(c.lit)) return false;
        long long a = stoll(val);
        long long b = stoll(c.lit);
        if (c.op == "=") return a == b;
        if (c.op == "!=") return a != b;
        if (c.op == "<") return a < b;
        if (c.op == "<=") return a <= b;
        if (c.op == ">") return a > b;
        if (c.op == ">=") return a >= b;
        return false;
    } else {
        // TEXT: compare lexicographically for <, >; equality by string
        if (c.op == "=") return val == c.lit;
        if (c.op == "!=") return val != c.lit;
        if (c.op == "<") return val < c.lit;
        if (c.op == "<=") return val <= c.lit;
        if (c.op == ">") return val > c.lit;
        if (c.op == ">=") return val >= c.lit;
        return false;
    }
}

// Parse CREATE TABLE
void handleCreate(const vector<string> &toks) {
    // "CREATE" "TABLE" name "(" col TYPE , ... ")"
    if (toks.size() < 6) { cout << "Syntax error in CREATE\n"; return; }
    string name = toks[2];
    if (db.count(name)) { cout << "Table already exists\n"; return; }
    int i = 3;
    if (toks[i] != "(") { cout << "Expected (\n"; return; }
    ++i;
    vector<string> cols;
    vector<ColType> types;
    while (i < (int)toks.size() && toks[i] != ")") {
        string col = toks[i++]; 
        if (i >= (int)toks.size()) { cout << "Syntax CREATE\n"; return; }
        string typ = lower(toks[i++]);
        ColType ct;
        if (typ == "int") ct = CT_INT;
        else if (typ == "text") ct = CT_TEXT;
        else { cout << "Unknown type " << typ << "\n"; return; }
        cols.push_back(col);
        types.push_back(ct);
        if (i < (int)toks.size() && toks[i] == ",") ++i;
    }
    if (i >= (int)toks.size() || toks[i] != ")") { cout << "Missing )\n"; return; }
    Table tab;
    tab.name = name; tab.cols = cols; tab.types = types;
    db[name] = move(tab);
    cout << "Table " << name << " created\n";
}

// Parse INSERT
void handleInsert(const vector<string> &toks) {
    // INSERT INTO name (c1,c2) VALUES (v1,v2)
    if (toks.size() < 6) { cout << "Syntax error in INSERT\n"; return; }
    string name = toks[2];
    if (!db.count(name)) { cout << "No such table\n"; return; }
    Table &t = db[name];
    int i = 3;
    if (toks[i] != "(") { cout << "Expected (\n"; return; }
    ++i;
    vector<string> cols;
    while (i < (int)toks.size() && toks[i] != ")") {
        cols.push_back(toks[i++]);
        if (i < (int)toks.size() && toks[i] == ",") ++i;
    }
    if (i >= (int)toks.size() || toks[i] != ")") { cout << "Missing )\n"; return; }
    ++i;
    if (i >= (int)toks.size() || lower(toks[i]) != "values") { cout << "Expected VALUES\n"; return; }
    ++i;
    if (i >= (int)toks.size() || toks[i] != "(") { cout << "Expected (\n"; return; }
    ++i;
    vector<string> vals;
    while (i < (int)toks.size() && toks[i] != ")") {
        string v = toks[i++];
        if (v.size() >=2 && v.front() == '\'' && v.back() == '\'') v = v.substr(1, v.size()-2);
        vals.push_back(v);
        if (i < (int)toks.size() && toks[i] == ",") ++i;
    }
    if (i >= (int)toks.size() || toks[i] != ")") { cout << "Missing )\n"; return; }
    // map columns to table layout
    vector<string> row(t.cols.size(), "");
    if (cols.size() != vals.size()) { cout << "Columns and values count mismatch\n"; return; }
    for (size_t k = 0; k < cols.size(); ++k) {
        auto it = find(t.cols.begin(), t.cols.end(), cols[k]);
        if (it == t.cols.end()) { cout << "Unknown column " << cols[k] << "\n"; return; }
        int idx = int(it - t.cols.begin());
        // basic type check
        if (t.types[idx] == CT_INT && !isIntegerString(vals[k])) {
            cout << "Type mismatch: expected INT for column " << cols[k] << "\n"; return;
        }
        row[idx] = vals[k];
    }
    // fill unspecified with empty
    t.rows.push_back(row);
    cout << "Inserted into " << name << "\n";
}

// Parse WHERE clause into vector<Cond> joined by AND only
vector<Cond> parseWhere(const vector<string> &toks, int start) {
    vector<Cond> res;
    int i = start;
    while (i < (int)toks.size()) {
        // expect: col op literal
        if (i+2 >= (int)toks.size()) break;
        Cond c;
        c.col = toks[i++];
        c.op = toks[i++];
        string lit = toks[i++];
        if (lit.size() >= 2 && lit.front() == '\'' && lit.back() == '\'') lit = lit.substr(1, lit.size()-2);
        res.push_back({c.col, c.op, lit});
        if (i < (int)toks.size() && lower(toks[i]) == "and") { ++i; continue; }
        else break;
    }
    return res;
}

void handleSelect(const vector<string> &toks) {
    // SELECT col1, col2 FROM table WHERE ...
    int i = 1;
    vector<string> selCols;
    if (toks[i] == "*") { selCols.push_back("*"); ++i; }
    else {
        while (i < (int)toks.size() && lower(toks[i]) != "from") {
            if (toks[i] != ",") selCols.push_back(toks[i]);
            ++i;
        }
    }
    if (i >= (int)toks.size() || lower(toks[i]) != "from") { cout << "Expected FROM\n"; return; }
    ++i;
    if (i >= (int)toks.size()) { cout << "Expected table name\n"; return; }
    string name = toks[i++]; 
    if (!db.count(name)) { cout << "No such table\n"; return; }
    Table &t = db[name];
    // WHERE?
    vector<Cond> conds;
    if (i < (int)toks.size() && lower(toks[i]) == "where") {
        ++i;
        conds = parseWhere(toks, i);
    }
    // compute result rows
    vector<int> projIdx;
    bool star = false;
    if (selCols.size()==1 && selCols[0] == "*") { star = true; }
    else {
        for (auto &c : selCols) {
            auto it = find(t.cols.begin(), t.cols.end(), c);
            if (it == t.cols.end()) { cout << "Unknown column in SELECT: " << c << "\n"; return; }
            projIdx.push_back(int(it - t.cols.begin()));
        }
    }
    // header
    if (star) {
        for (size_t k=0;k<t.cols.size();++k) {
            if (k) cout << " | ";
            cout << t.cols[k];
        }
        cout << "\n";
    } else {
        for (size_t k=0;k<projIdx.size();++k) {
            if (k) cout << " | ";
            cout << t.cols[projIdx[k]];
        }
        cout << "\n";
    }
    // rows
    int outCount = 0;
    for (auto &row : t.rows) {
        bool ok = true;
        for (auto &c : conds) {
            if (!evalCond(c, t, row)) { ok = false; break; }
        }
        if (!ok) continue;
        ++outCount;
        if (star) {
            for (size_t k=0;k<row.size();++k) {
                if (k) cout << " | ";
                cout << row[k];
            }
            cout << "\n";
        } else {
            for (size_t k=0;k<projIdx.size();++k) {
                if (k) cout << " | ";
                cout << row[projIdx[k]];
            }
            cout << "\n";
        }
    }
    cout << outCount << " row(s)\n";
}

// Split input into statements terminated by ';'
vector<string> splitStatements(const string &input) {
    vector<string> out;
    string cur;
    for (char ch : input) {
        cur.push_back(ch);
        if (ch == ';') {
            out.push_back(cur);
            cur.clear();
        }
    }
    if (!trim(cur).empty()) out.push_back(cur);
    return out;
}

int main() {
    cout << "Mini SQL engine (type 'exit;' to quit)\n";
    string line, input;
    while (true) {
        cout << "sql> ";
        if (!getline(cin, line)) break;
        input += line + "\n";
        // process if a ';' exists
        if (line.find(';') == string::npos) continue;
        auto stmts = splitStatements(input);
        input.clear();
        for (auto &stmt : stmts) {
            string s = trim(stmt);
            if (s.empty()) continue;
            auto toks = tokenize(s);
            if (toks.empty()) continue;
            string kw = lower(toks[0]);
            if (kw == "exit") return 0;
            if (kw == "create" && toks.size() > 1 && lower(toks[1]) == "table") {
                handleCreate(toks);
            } else if (kw == "insert" && toks.size() > 1 && lower(toks[1]) == "into") {
                handleInsert(toks);
            } else if (kw == "select") {
                handleSelect(toks);
            } else {
                cout << "Unsupported or syntax error\n";
            }
        }
    }
    return 0;
}
