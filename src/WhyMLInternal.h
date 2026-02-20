#ifndef WHYMLINTERNAL_H
#define WHYMLINTERNAL_H

#include<string>
using std::string;

#include<sstream>
using std::stringstream;

#include<map>
using std::map;

#include<set>
using std::set;

#include<vector>
using std::vector;

#include<fstream>
using std::ofstream;

#include "tinyxml2.h"
using tinyxml2::XMLElement;

#include "TypingContext.h"

using dict_t = map<string, string>;

class whyLocalHyp;

class whyPredicate {
public:
    whyPredicate(const XMLElement *e);
    whyPredicate(const XMLElement *e, const std::string &name);

    /// @brief collect the free symbols in the predicate and their types
    /// @param enums 
    /// @param typeInfos 
    virtual void collectVarAndTypes(const dict_t &enums,
                                      const TypingContext &typeInfos);

    virtual void translate(const dict_t &enums, const TypingContext &typeinfo);

    virtual void declare(ofstream &os);
    string declareCall() const;

    string translateTypeInfo (const XMLElement *ti, const dict_t& enums);

    const set<string> &variables() const {
        return m_variables;
    }

    const dict_t &types() const {
        return m_types;
    }

    /// @brief the whyml expression for the predicate
    string m_translation;

    static bool isEnumeratedSetElement(const XMLElement *e);
    static bool isDeferredSetElement(const XMLElement *e);

    static string deferredSetWhyName(const XMLElement *e1);
    static string whyName(const XMLElement *ident, const dict_t& enums, bool& var);

    static const std::string whyMAXINT;
    static const std::string whyMININT;

    friend ostream& operator<<(ostream &os, const whyPredicate &wp);

protected:
    const XMLElement *m_dom;

    string m_name;

    /// @brief the set of free symbols in the predicate
    set<string> m_variables;

    /// @brief associates to each free symbol a string representing the whyml type
    dict_t m_types; /// the domain is variables

    vector<string> m_functions;
    dict_t m_structures;

    static const dict_t opDict;

    virtual void collectVarAndTypes(set<string> &variables,
                                      dict_t &types,
                                      const XMLElement *formula,
                                      const dict_t &enums,
                                      const TypingContext &typeInfos);
    
    class Translator {
    public:
        Translator(stringstream &acc, const dict_t& enums, const TypingContext &typeinfo)
            : m_acc(acc), m_enums(enums), m_typeinfo(typeinfo) {}
        void translate(const XMLElement *formula);
        void translateExtensionSet(const XMLElement *ce);
        void translateExtensionSeq(const XMLElement *ce, unsigned index);
        void translateVariables(const XMLElement *formula);
        string translation();
    private:
        stringstream &m_acc;
        const dict_t& m_enums;
        const TypingContext &m_typeinfo;
    };
};

class whyDefine : public whyPredicate {
public:
    whyDefine(const XMLElement *d, int count);

    virtual void declare(ofstream& out) override;

private:
    string m_label;
};

class whyLocalHyp : public whyPredicate {
public:
    /// @brief creates a representation for a Local_Hyp element from the POG file
    /// @param dom a Local_Hyp child of a Proof_Obligation element of the DOM
    /// @param group the Tag text for the Proof_Obligation element
    /// @param num the num (index) of the Local_Hyp child for the Proof_Obligation element
    whyLocalHyp(const XMLElement *dom, const string &group, unsigned num);

    void declareWithoutDuplicate(const vector<whyLocalHyp*>&, ofstream& why);
    string declareCallDuplicate();

    bool is(unsigned num) const {
        return m_num == num;
    }

    friend ostream& operator<<(ostream &os, const whyLocalHyp &wp);

private:
    const string m_group;
    const unsigned m_num;
    whyLocalHyp* m_duplicate;
};

class whySimpleGoal : public whyPredicate {
public:
    whySimpleGoal(const XMLElement *dom, 
                const string &groupName,
                unsigned goal, 
                const vector<whyLocalHyp *> &, 
                const vector<whyDefine *> &, 
                whyLocalHyp * hypothesis);
    virtual void collectVarAndTypes(const dict_t &enums,
                                      const TypingContext &tc) override;
    virtual void declare(ofstream &out) override;
    virtual void translate(const dict_t &enums, const TypingContext &typeinfo) override;

    friend ostream& operator<<(ostream &os, const whySimpleGoal &wp);

private:
    const vector<whyDefine *> m_defines;
    const whyLocalHyp* m_hypothesis;
    const vector<whyLocalHyp *> m_localHypotheses;
};

extern const std::string whyMAXINT;
extern const std::string whyMININT;


#endif // WHYMLINTERNAL_H
