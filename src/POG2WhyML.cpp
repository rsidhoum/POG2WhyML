#include<cstdlib>
#include<cstring>
#include<iostream>
#include<fstream>
#include<map>
#include<algorithm>
#include<filesystem>
#include<utility>
#include<vector>

#include "version.h"

using std::cout;
using std::endl;
using std::make_pair;
using std::map;
using std::pair;
using std::vector;

#include "tinyxml2.h"

#include "WhyML.h"

static void display_help()
{

    cout << "pog2why" << endl
         << "\tversion "
         << POG2WHY_VERSION << endl
         << "\tcopyright CLEARSY Systems Engineering "
         << POG2WHY_COPYRIGHT_YEAR << endl
         << "Translates Atelier B proof obligation file to Why3 format." << endl
         << "\tpog2why -a N1 M1 -a N2 M2 ... -a Nk Mk -i file.pog -o file.why" << endl
         << "\t\t-a N M" << endl
         << "\t\t\tselects the N-th Simple_Goal child element from the M-th Proof_Obligation" << endl
         << "\t\t\telement for translation" << endl
         << "\t\t-A" << endl
         << "\t\t\ttranslates all goals" << endl
         << "\t\t-i FILE" << endl
         << "\t\t\tspecifies the path for the input file" << endl
         << "\t\t-o FILE" << endl
         << "\t\t\tspecifies the path for the output file" << endl
         << "\t\t-h" << endl
         << "\t\t\tprints help" << endl
         << "\tNote: options -A and -a are exclusive" << endl;
}

static void classifyGoals(const vector<pair<int,int>>& goals, map<int, vector<int>>& sgoals) {
    for(auto& goal: goals) {
        if(sgoals.find(goal.first) == sgoals.end()) {
            sgoals.insert(pair<int,vector<int>>(goal.first, vector<int>()));
        }
        sgoals.at(goal.first).push_back(goal.second);
    }
    for(auto& sgoal: sgoals) {
        std::sort(sgoal.second.begin(),
                  sgoal.second.end(),
                  [](int i, int j) { return i < j; }
        );
    }
}

int main(int argc, char **argv)
{
    char* input{nullptr};
    char* output{nullptr};
    bool  opt_a{false};
    bool  opt_A{false};
    vector<pair<int, int>> goals;
    map<int, vector<int>> sgoals;
    int arg;

    arg = 1;
    while (arg < argc) {
        if (strcmp(argv[arg], "-a") == 0) {
            if (opt_A) {
                cout << "Usage error." << endl;
                display_help();
                return EXIT_FAILURE;
            }
            opt_a = true;
            goals.push_back(make_pair(atoi(argv[arg+1]), atoi(argv[arg+2])));
            arg += 3;
        } else if (strcmp(argv[arg], "-A") == 0) {
            if (opt_a) {
                cout << "Usage error." << endl;
                display_help();
                return EXIT_FAILURE;
            }
            opt_A = true;
            arg += 1;
        } else if (strcmp(argv[arg], "-i") == 0) {
            input = argv[arg+1];
            arg += 2;
        } else if (strcmp(argv[arg], "-o") == 0) {
            output = argv[arg+1];
            arg += 2;
        } else if (strcmp(argv[arg], "-h") == 0) {
            display_help();
            arg += 1;
        } else {
            display_help();
            return EXIT_FAILURE;
        }
    }
    if (opt_a == false && opt_A == false) {
        display_help();
        return EXIT_FAILURE;
    }
    if (nullptr == input) {
        display_help();
        return EXIT_FAILURE;
    }
    if (nullptr == output) {
        display_help();
        return EXIT_FAILURE;
    }

    std::ifstream infile;

    XMLDocument doc;
    std::filesystem::path inpath {input};
    std::filesystem::path outpath {output};

    if (!std::filesystem::exists(inpath)) {
        exit(EXIT_FAILURE);
    }

    auto status = doc.LoadFile(input);
    if (status != tinyxml2::XML_SUCCESS) {
        std::cerr << doc.ErrorStr();
        exit(EXIT_FAILURE);
    }

    std::ofstream outfile (outpath);

    try {
        if (opt_A) {
            saveWhy3(doc, outfile, false);
        } else {
            classifyGoals(goals, sgoals);
            saveWhy3(doc, outfile, sgoals);
        }
    } catch(const std::exception &e){
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
