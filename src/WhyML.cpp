#include<iostream>

#include<algorithm>
#include<cstdio>
#include<fstream>
#include<sstream>
#include<map>
#include<vector>
#include<cassert>

using std::map;
using std::pair;
using std::vector;

#include "WhyML.h"
#include "TypingContext.h"
#include "WhyMLInternal.h"
#include "exceptions.h"

using tinyxml2::XMLElement;
using tinyxml2::XMLDocument;

static int nbLambda = 0;
static int nbSet = 0;
static int nbUnion = 0;
static int nbInter = 0;
static int nbStruct = 0;
static int nbSigma = 0;
static int nbPi = 0;

map<string, string> structTypesG; /** useless??? */

static void saveEnumeration(std::ofstream &out, dict_t &enums, const XMLElement *set);


/**
 * @brief appendPrelude prints Why3 prelude to import all definitions for the B theory.
 * @param out is the stream receiving the produced text
 */
static void appendPrelude(std::ofstream &out)
{
    out << "theory B_translation\n";
    out << "  use bool.Bool\n";
    out << "  use int.Int\n";
    out << "  use int.Power\n";
    out << "  use int.ComputerDivision\n";
    out << "    (* See B-Book, section 3.5.7-Arithmetic, Division, p. 164\n";
    out << "       and section 3.6-Integers, table p. 173 *)\n";
    out << "\n";
    out << "  use bpo2why_prelude.B_BOOL\n";
    out << "  use bpo2why_prelude.Interval\n";
    out << "  use bpo2why_prelude.PowerSet\n";
    out << "  use bpo2why_prelude.Relation\n";
    out << "  use bpo2why_prelude.Image\n";
    out << "  use bpo2why_prelude.Identity\n";
    out << "  use bpo2why_prelude.InverseDomRan\n";
    out << "  use bpo2why_prelude.Function\n";
    out << "  use bpo2why_prelude.Sequence\n";
    out << "  use bpo2why_prelude.IsFinite\n";
    out << "  use bpo2why_prelude.PowerRelation\n";
    out << "  use bpo2why_prelude.Restriction\n";
    out << "  use bpo2why_prelude.Overriding\n";
    out << "  use bpo2why_prelude.Composition\n";
    out << "  use bpo2why_prelude.BList\n";
    out << "  use bpo2why_prelude.MinMax\n";
    out << "  use bpo2why_prelude.Projection\n";
    out << "  use bpo2why_prelude.Iteration\n";
    out << "  use bpo2why_prelude.Generalized\n";
    out << "\n";
    //out << "  type uninterpreted_type\n";
    out << "\n";
}

static void appendConclusion(std::ofstream &out)
{
    out << "end\n";
}

void saveWhy3(XMLDocument &pog, std::ofstream &os, bool xml) {
    XMLElement *root {pog.RootElement()};
    const TypingContext tc {pog};

    nbLambda = 0;
    nbSet = 0;

    os << "(* This file has been generated *)\n"
        << "\n"
        << "(*** prelude ***)\n";

    appendPrelude(os);

    dict_t enums;       // maps B names to why3 names.
    map<string, whyDefine*> defines; // map Define identifiers to corresponding whyDefine structure


    os << "\n(*** definitions ***)\n";
    // Enumerated sets declaration
    const XMLElement *elem;
    for (elem = root->FirstChildElement("Define"); nullptr != elem; elem = elem->NextSiblingElement("Define")) {
        for (const XMLElement *setElem = elem->FirstChildElement("Set"); nullptr != setElem; setElem = setElem->NextSiblingElement("Set")) {
            if (whyPredicate::isEnumeratedSetElement(setElem))
                saveEnumeration(os, enums, setElem);
        }
    }

    // Defines declaration
    int count;
    for (count = 1, elem = root->FirstChildElement("Define");
         nullptr != elem;
         ++count, elem = elem->NextSiblingElement("Define")) {
        whyDefine* wd = new whyDefine(elem, count);
        wd->collectVarAndTypes(enums, tc);
        wd->translate(enums, tc);
        wd->declare(os);
        defines.insert({elem->Attribute("name"), wd});
    }
    os << "\n";

    os << "(*** local hypothesis ***)\n";
    // Local hypothesis declaration
    //   localHyps has one entry per Simple_Goal in POG
    //   position in vector is the position in POG
    vector<vector<whyLocalHyp *>> localHyps;
    int goal = 0;
    for (elem = root->FirstChildElement("Proof_Obligation"); nullptr != elem; elem = elem->NextSiblingElement("Proof_Obligation")) {
        if (elem->FirstChildElement("Simple_Goal") == nullptr) {
            localHyps.push_back(vector<whyLocalHyp *>());
            continue;
        }
        const XMLElement *tag {elem->FirstChildElement("Tag")};
        const string groupName {nullptr != tag ? string(tag->GetText()) : std::to_string(goal)};
        vector<whyLocalHyp *> vlh{};
        for (const XMLElement *lh = elem->FirstChildElement("Local_Hyp"); nullptr != lh; lh = lh->NextSiblingElement("Local_Hyp")) {
            whyLocalHyp* wlh = new whyLocalHyp(lh, groupName, lh->UnsignedAttribute("num"));
            wlh->collectVarAndTypes(enums, tc);
            wlh->translate(enums, tc);
            wlh->declareWithoutDuplicate(vlh, os);
            vlh.push_back(wlh);
        }
        localHyps.push_back(std::move(vlh));
        ++goal;
    }

    os << "\n(*** goals ***)\n";
    // Goals declaration
    for (goal = 0, elem = root->FirstChildElement("Proof_Obligation");
         nullptr != elem;
         ++goal, elem = elem->NextSiblingElement("Proof_Obligation")) {
        if (elem->FirstChildElement("Simple_Goal") == nullptr)
            continue;

        const XMLElement *tag {elem->FirstChildElement("Tag")};
        const string groupName {nullptr != tag ? string(tag->GetText()) : std::to_string(goal)};

        os << "\n(*** goal ***)\n" ;
        
        vector<whyDefine*> sgDefinitions;
        for (const XMLElement *definition = elem->FirstChildElement("Definition");
                 definition != nullptr;
                 definition = definition->NextSiblingElement("Definition")) {
            const auto& it {defines.find(definition->Attribute("name"))};
            if (it == defines.end()) {
                throw WhyTranslateException(string("Unknown definition ") + definition->Attribute("name"));
            }
            sgDefinitions.push_back(it->second);
        }

        // Builds the local hypotheses for the Simple_Goal.
        // Two cases: there is an Hypothesis in the DOM, or not
        // In the latter case, the constructor is called with the Hypothesis,
        // in the former, it is called with the Proof_Obligation.
        // In this second case, the whyLocalHyp object constructed shall
        // represent a tautology.
        whyLocalHyp* wh;
        if (elem->FirstChildElement("Hypothesis") != nullptr) {
            wh = new whyLocalHyp(elem->FirstChildElement("Hypothesis"), groupName, 0);
            wh->collectVarAndTypes(enums, tc);
            wh->translate(enums, tc);
        } else {
            wh = new whyLocalHyp(elem, groupName, 0);
        }
        wh->declare(os);

        unsigned simple_goal {0u};
        for(const XMLElement *sg = elem->FirstChildElement("Simple_Goal");
            sg != nullptr;
            sg = sg->NextSiblingElement("Simple_Goal")) {
            whySimpleGoal* wsg = new whySimpleGoal(sg, groupName, simple_goal, localHyps.at(goal), sgDefinitions, wh);
            wsg->collectVarAndTypes(enums, tc);
            wsg->translate(enums, tc);
            wsg->declare(os);
            simple_goal++;
        }
    }

    appendConclusion(os);

}

void saveWhy3(XMLDocument &pog, std::ofstream &why, const std::map<int, std::vector<int>>& goals) {
    const XMLElement *root {pog.RootElement()};
    const TypingContext tc {pog};

    nbLambda = 0;
    nbSet = 0;

    appendPrelude(why);

    appendConclusion(why);

}

/**
 * @brief saveEnumeration prints type definitions corresponding to enumerated sets and
 * builds translation dictionary from B names to why3 names.
 * @param out is the text stream receiving the text produced
 * @param enums is map where the B to why3 translation dictionary is stored
 * @param set is a DOM element containing enumerations to be translated
 * @details For the B enumeration
 *   s1 = { e1, e2, e3 }
 * The following why3 output is produced
*   type enum_s1 = E_e1 | E_e2 | E_e3
 *   function set_enum_s1 : set enum_s1
 *   axiom set_enum_s1_axiom :
 *     forall x:enum_s1. mem x set_enum_s1
 *
 */
void saveEnumeration(std::ofstream &out, dict_t &enums, const XMLElement *enumeration)
{
    assert(whyPredicate::isEnumeratedSetElement(enumeration));
    const string setName {enumeration->FirstChildElement("Id")->Attribute("value")};

    auto it {enums.find(setName)};
    if (it != enums.end())
        return;

    const XMLElement *enumE = enumeration->FirstChildElement("Enumerated_Values");
    const string enumName = "enum_" + setName;
    const string whyName = "set_enum_" + setName;
    const string axiomName = whyName + "_axiom";
    string elemStringList;
    enums.insert({setName, whyName});
    for (const XMLElement *elem = enumE->FirstChildElement("Id"); elem != nullptr; elem = elem->NextSiblingElement("Id")) {
        string stringB = elem->Attribute("value");
        string stringW = "E_" + stringB;
        if (elemStringList.empty()) {
            elemStringList.append(stringW);
        }
        else {
            elemStringList.append(" | ").append(stringW);
        }
        enums.insert({stringB, stringW});
    }
    out << "type " << enumName << " = " << elemStringList << "\n"
        << "function " << enums[setName] << " : set " << enumName << "\n"
        << "axiom " << axiomName << ":\n"
        << "forall x:" << enumName << ". mem x " << enums[setName] << "\n"
        << "\n";
}
