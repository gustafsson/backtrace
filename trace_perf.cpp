#include "trace_perf.h"
#include "detectgdb.h"

#include <vector>
#include <fstream>
#include <map>
#include <algorithm>
#include <sstream>
#include <iostream>

#include <sys/stat.h>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // gethostname
#else
#include <unistd.h> // gethostname
#endif

bool PRINT_ATTEMPTED_DATABASE_FILES = true;

using namespace std;

class performance_traces {
private:
    struct Entry {
        string info;
        double elapsed;
    };

    map<string, vector<Entry>> entries;

    static void read_database(map<string, double>& db, string filename);
    static vector<string> get_database_names(string sourcefilename);
    static void load_db(map<string, map<string, double>>& dbs, string sourcefilename);
    static void compare_to_db(map<string, double> &db, const vector<Entry>& entries, string sourcefilename);
    static void dump_entries(const vector<Entry>& entries, string sourcefilaname);

    void compare_to_db();
    void dump_entries();

public:
    ~performance_traces();

    void log(string filename, string info, double elapsed) {
        entries[filename].push_back(Entry{info, elapsed});
    }
};

static performance_traces traces;


performance_traces::
        ~performance_traces()
{
    compare_to_db();
    dump_entries();
}


void performance_traces::
        load_db(map<string, map<string, double>>& dbs, string sourcefilename)
{
    if (dbs.find (sourcefilename) != dbs.end ())
        return;

    map<string, double> db;

    vector<string> dbnames = get_database_names(sourcefilename);
    for (unsigned i=0; i<dbnames.size (); i++)
        read_database(db, dbnames[i]);

    if (db.empty ())
        cerr << "Couldn't read databases in folder trace_perf" << endl;

    dbs[sourcefilename] = db;
}


void performance_traces::
        compare_to_db()
{
    map<string, map<string, double>> dbs;
    for (auto a = entries.begin (); a!=entries.end (); a++)
    {
        string sourcefilename = a->first;
        load_db(dbs, sourcefilename);
    }

    for (auto a = dbs.begin (); a!=dbs.end (); a++)
    {
        string sourcefilename = a->first;
        map<string, double>& db = a->second;
        vector<Entry>& entries = this->entries[sourcefilename];

        compare_to_db(db, entries, sourcefilename);
    }

    bool missing_test_printed = false;
    for (auto i = dbs.begin (); i!=dbs.end (); i++)
    {
        map<string, double>& db = i->second;

        if (!db.empty () && !missing_test_printed) {
            cerr << endl << "Missing tests ..." << endl;
            missing_test_printed = true;
        }

        for (auto i = db.begin (); i!=db.end (); i++)
            cerr << '\'' << i->first  << '\'' << endl;
    }
}


void performance_traces::
        compare_to_db(map<string, double> &db, const vector<Entry>& entries, string sourcefilename)
{
    bool expected_miss = false;
    for (unsigned i=0; i<entries.size (); i++)
    {
        string info = entries[i].info;
        double elapsed = entries[i].elapsed;

        double expected = -1;

        auto j = db.find (info);
        if (j != db.end ())
        {
            expected = j->second;
            db.erase (j);
        }

        if (elapsed > expected)
        {
            if (!expected_miss) {
                cerr << endl << sourcefilename << " wasn't fast enough ..." << endl;
                if (PRINT_ATTEMPTED_DATABASE_FILES) {
                    vector<string> dbnames = get_database_names(sourcefilename);
                    for (unsigned i=0; i<dbnames.size (); i++)
                        cerr << dbnames[i] << endl;
                }
            }

            cerr << endl;
            cerr << info << endl;
            cerr << elapsed << " > " << expected << endl;
            expected_miss = true;
        }
    }

    if (expected_miss)
        cerr << endl;
}


void performance_traces::
        dump_entries()
{
    for (auto i=entries.begin (); i!=entries.end (); i++)
        dump_entries (i->second, i->first);
}


void performance_traces::
        dump_entries(const vector<Entry>& entries, string sourcefilaname)
{
    // requires boost_filesystem
    //boost::filesystem::create_directory("trace_perf");
    //boost::filesystem::create_directory("trace_perf/dump");

    // require posix
    mkdir("trace_perf", S_IRWXU|S_IRGRP|S_IXGRP);
    mkdir("trace_perf/dump", S_IRWXU|S_IRGRP|S_IXGRP);

    int i=0;
    string filename;
    while (true) {
        stringstream ss;
        ss << "trace_perf/dump/" << sourcefilaname << ".db" << i;
        filename = ss.str ();
        ifstream file(filename);
        if (!file)
            break;
        i++;
    }

    ofstream o(filename);
    if (!o)
        cerr << "Couldn't dump performance entries to " << filename << endl;

    for (unsigned i=0; i<entries.size (); i++)
    {
        if (0 < i)
            o << endl;

        o << entries[i].info << endl
          << entries[i].elapsed << endl;
    }
}


void performance_traces::
        read_database(map<string, double>& db, string filename)
{
    std::string info;
    double expected;

    ifstream a(filename);
    if (!a.is_open ())
        return;

    while (!a.eof() && !a.fail())
    {
        getline(a,info);
        a >> expected;
        db[info] = expected;

        getline(a,info); // finish line
        getline(a,info); // read empty line
    }
}


vector<string> performance_traces::
        get_database_names(string sourcefilename)
{
    string hostname("unknown");
    hostname.reserve (256);
    gethostname(&hostname[0], hostname.capacity ());

    vector<string> config;

#if defined(__APPLE__)
    config.push_back ("-apple");
#elif defined(_MSC_VER)
    config.push_back ("-windows");
#endif

#ifdef _DEBUG
    config.push_back ("-debug");
#endif

    if (DetectGdb::is_running_through_gdb())
        config.push_back ("-gdb");

    vector<string> db;
    for (int i=0; i<(1 << config.size ()); i++)
    {
        string perm;
        for (unsigned j=0; j<config.size (); j++)
        {
            if ((i>>j) % 2)
                perm += config[j];
        }
        db.push_back (sourcefilename + ".db" + perm);
    }

    int n = db.size();
    if (!hostname.empty ())
        for (int i=0; i<n; i++)
            db.push_back (hostname + "/" + db[i]);

    for (unsigned i=0; i<db.size (); i++)
        db[i] = "trace_perf/" + db[i];

    return db;
}


trace_perf::trace_perf(const char* filename, const string& info)
    :
      filename(filename)
{
    reset(info);
}


trace_perf::
        ~trace_perf()
{
    reset();
}


void trace_perf::
        reset()
{
    double d = timer.elapsed ();
    if (!info.empty ())
        traces.log (filename, info, d);
}


void trace_perf::
        reset(const string& info)
{
    reset();

    this->info = info;
    this->timer.restart ();
}
