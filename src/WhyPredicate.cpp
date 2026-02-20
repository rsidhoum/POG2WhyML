#include<algorithm>
using std::replace;

#include<iostream>

#include "WhyMLInternal.h"

#include "exceptions.h"
#include "utils.h"

whyPredicate::whyPredicate(const XMLElement *dom) 
    : m_translation{"true"}
    , m_dom{dom}
    , m_name{}
    , m_variables{}
    , m_types{}
    , m_functions{}
    , m_structures{}
{}

whyPredicate::whyPredicate(const XMLElement *dom, const string &name) 
    : m_translation{"true"}
    , m_dom{dom}
    , m_name{name}
    , m_variables{}
    , m_types{}
    , m_functions{}
    , m_structures{}
{
      replace( m_name.begin(), m_name.end(), ' ', '_');
}

void whyPredicate::collectVarAndTypes(const dict_t &enums,
                                      const TypingContext &tc) {
    for (const XMLElement *e1 = m_dom->FirstChildElement(); e1 != nullptr; e1 = e1->NextSiblingElement()) {
        if (isDeferredSetElement(e1)) {
            const string name = deferredSetWhyName(e1);
            if (!m_variables.contains(name)) {
                m_variables.insert(name);
                m_types.insert({name, std::string("set int")});
            }
        }
        else {
            collectVarAndTypes(m_variables, m_types, e1, enums, tc);
        }
    }
}

void whyPredicate::collectVarAndTypes(set<string> &variables,
                                      dict_t &types,
                                      const XMLElement *formula,
                                      const dict_t &enums,
                                      const TypingContext &tc) {
    if (formula == nullptr)
        return;
    const char *tag {formula->Name()};
    if (cstrEq(tag, "Attr"))
        return;
    if (cstrEq(tag, "Id")) {
        bool var;
        const string name = whyName(formula, enums, var);
        if (var && !variables.contains(name)) {
            const string &type {tc.getTypeTranslation(formula)};
            variables.insert(name);
            types.insert({name, type});
        }
        return;
    }
    if (cstrEq(tag, "Quantified_Pred") || cstrEq(tag, "Quantified_Exp") || cstrEq(tag, "Quantified_Set")) {
        set<string> qVariables;
        dict_t qTypes;
        const XMLElement *child = formula->FirstChildElement();
        while(child != nullptr) {
            const XMLElement *next = child->NextSiblingElement();
            collectVarAndTypes(qVariables, qTypes, child, enums, tc);
            child = next;
        }
        const XMLElement *qVar = formula->FirstChildElement("Variables")->FirstChildElement("Id");
        while (qVar != nullptr) {
            bool isVar;
            const string qVarName {whyName(qVar, enums, isVar)};
            qVariables.erase(qVarName);
            qTypes.erase(qVarName);
            qVar = qVar->NextSiblingElement("Id");
        }
        for (const auto &qVar: qVariables) {
            if (!variables.contains(qVar)) {
                variables.insert(qVar);
                types.insert({qVar, qTypes[qVar]});
            }
        }
       return;
    }
    if (cstrEq(tag, "Binary_Exp") || cstrEq(tag, "Ternary_Exp")) {
        string op = formula->Attribute("op");
        if (op == "'") {
            collectVarAndTypes(variables, types, formula->FirstChildElement(), enums, tc);
            return;
        }
        if (op == "<'") {
            collectVarAndTypes(variables, types, formula->FirstChildElement(), enums, tc);
            collectVarAndTypes(variables, types, formula->FirstChildElement()->NextSiblingElement()->NextSiblingElement(), enums, tc);
            return;
        }
    }

    const XMLElement *child = formula->FirstChildElement();
    while(child != nullptr) {
        const XMLElement *next = child->NextSiblingElement();
        collectVarAndTypes(variables, types, child, enums, tc);
        child = next;
    }
}

string whyPredicate::declareCall() const {
    string result;
    result.append("(");
    result.append(m_name);
    for(const auto &v: m_variables) {
        result.append(" ").append(v);
    }
    result.append(")");
    return result;
}


void whyPredicate::declare(ofstream& os) {
    /// TODO structs
    /*
    QStringList structs;
    QMapIterator<QString, QString> i(structTypes);
    while (i.hasNext()) {
        i.next();
        int j=0;
        while (j<structs.size()) {
            if (structs.at(j).contains(i.value()))
                break;
            else
                j++;
        }
        structs.insert(j,"type " + i.value() + " = " + i.key());
    }
    out << structs.join("\n");
    out << "\n";
    */
    for (const auto f: m_functions) {
        os << f << std::endl;
    }

    os << "\n"
       << "predicate " << m_name << " ";
    for (const auto &v: m_variables) {
        os << "(" << v << " : " + m_types.find(v)->second + ")";
    }
    os << " =\n   " << m_translation << "\n\n";
}

void whyPredicate::translate(const dict_t &enums, const TypingContext &tc) {
    bool first {true};
    stringstream ss;
    Translator translator{ss, enums, tc};
    for (const XMLElement *e1 = m_dom->FirstChildElement();
        nullptr != e1;
        e1 = e1->NextSiblingElement()) {
        const auto tag {e1->Name()};
        if (cstrEq(tag, "Set") || cstrEq(tag, "Tag") || cstrEq(tag, "Proof_State") || cstrEq(tag, "iapa") || cstrEq(tag, "Ref_Hyp"))
            continue;
        if (first) {
            first = false;
        }
        else {
            ss << " /\\\n";
        }
        translator.translate(e1);
    }
    m_translation = translator.translation();
    if (m_translation.empty()) {
        m_translation = "true";
    }
}

/* The translation of the following operators requires ad hoc rewrites. */
const set<string> rewriteBOp {
    R"(:)",
    R"(/:)",
    R"(<:)",
    R"(/<:)",
    R"(<<:)",
    R"(/<<:)",
    R"(=)",
    R"(/=)",
    R"(<')",
    "succ",
    "pred",
    "{",
    "]"
};

/* These translated operators use prefix notation. */
const set<string> prefixedWOp {
    "times",
    "Power.power",
    "diff",
    "insert_in_front",
    "Interval.mk",
    "div",
    "inter",
    "restriction_head",
    "semicolon",
    "iterate",
    "relation",
    "insert_at_tail",
    "domain_substraction",
    "domain_restriction",
    "direct_product",
    "parallel_product",
    "union",
    "restriction_tail",
    "concatenation",
    "mod",
    "range_restriction",
    "range_substraction",
    "image",
    "apply",
    "prj1",
    "prj2"
};

const dict_t whyPredicate::opDict{
    {"", "EMPTY_OPERATOR"}
    , {"#", "exists"}
    , {"!", "forall"}
    /* <xs:simpleType name="binary_pred_op"> */
    /* <xs:enumeration value="=>" /> */
    , {"=>", "->"}
    /*  <xs:enumeration value="&lt;=>" /> */
    , {"<=>", "<->"}
    /*  <!-- integer comparison --> */
    /*  <xs:enumeration value=">=i" /> */
    , {">=i", ">="}
    /*  <xs:enumeration value=">i" /> */
    , {">i", ">"}
    /*  <xs:enumeration value="&lt;i" /> */
    , {"<i", "<"}
    /*  <xs:enumeration value="&lt;=i" /> */
    , {"<=i", "<="}
    /*  <!-- real comparison --> */
    /*  <xs:enumeration value=">=r" /> */
    /*  <xs:enumeration value=">r" /> */
    /*  <xs:enumeration value="&lt;r" /> */
    /*  <xs:enumeration value="&lt;=r" /> */
    /*  <!-- float comparison --> */
    /*  <xs:enumeration value=">=f" /> */
    /*  <xs:enumeration value=">f" /> */
    /*  <xs:enumeration value="&lt;f" /> */
    /*  <xs:enumeration value="&lt;=f" /> */
    /*  <xs:simpleType name="binary_exp_op"> */
    /*  <xs:enumeration value="," /> */
    /*  <xs:enumeration value="*" /> */
    /*  <xs:enumeration value="*i" /> */
    , {"*i", "*"}
    /*  <xs:enumeration value="*r" /> */
    /*  <xs:enumeration value="*f" /> */
    /*  <xs:enumeration value="*s" /> */
    , {"*s", "times"}
    /*  <xs:enumeration value="**" /> */
    /*  <xs:enumeration value="**i" /> */
    , {"**i", "Power.power"}
    /*  <xs:enumeration value="**r" /> */
    /*  <xs:enumeration value="+" /> */
    /*  <xs:enumeration value="+i" /> */
    , {"+i", "+"}
    /*  <xs:enumeration value="+r" /> */
    /*  <xs:enumeration value="+f" /> */
    /*  <xs:enumeration value="+->" /> */
    , {"+->", "+->"}
    /*  <xs:enumeration value="+->>" /> */
    , {"+->>", "+->>"}
    /*  <xs:enumeration value="-" /> */
    , {"-", "diff"}
    /*  <xs:enumeration value="-i" /> */
    , {"-i", "-"}
    /*  <xs:enumeration value="-r" /> */
    /*  <xs:enumeration value="-f" /> */
    /*  <xs:enumeration value="-s" /> */
    , {"-s", "diff"}
    /*  <xs:enumeration value="-->" /> */
    , {"-->", "-->"}
    /*  <xs:enumeration value="-->>" />*/
    , {"-->>", "-->>"}
    /*  <xs:enumeration value="->" /> */
    , {"->", "insert_in_front"}
    /*  <xs:enumeration value=".." /> */
    , {"..", "Interval.mk"}
    /*  <xs:enumeration value="/" /> */
    /*  <xs:enumeration value="/i" /> */
    , {"/i", "div"}
    /*  <xs:enumeration value="/r" /> */
    /*  <xs:enumeration value="/f" /> */
    /*  <xs:enumeration value="/\" /> */
    , {R"(/\)", "inter"}
    /*  <xs:enumeration value="/|\" /> */
    , {R"(/|\)", "restriction_head"}
    /*  <xs:enumeration value=";" /> */
    , {";", "semicolon"}
    /*  <xs:enumeration value="&lt;+" /> */
    , {"<+", "<+"}
    /*  <xs:enumeration value="&lt;->" /> */
    , {"<->", "relation"}
    /*  <xs:enumeration value="&lt;-" /> */
    , {"<-", "insert_at_tail"}
    /*  <xs:enumeration value="&lt;&lt;|" /> */
    , {"<<|", "domain_substraction"}
    /*  <xs:enumeration value="&lt;|" /> */
    , {"<|", "domain_restriction"}
    /*  <xs:enumeration value=">+>" /> */
    , {">+>", ">+>"}
    /*  <xs:enumeration value=">->" /> */
    , {">->", ">->"}
    /*  <xs:enumeration value=">+>>" /> */
    , {">+>>", ">+>>"}
    /*  <xs:enumeration value=">->>" /> */
    , {">->>", ">->>"}
    /*  <xs:enumeration value=">&lt;" />*/
    , {"><", "direct_product"}
    /*  <xs:enumeration value="||" /> */
    , {"||", "parallel_product"}
    /*  <xs:enumeration value="\/" /> */
    , {R"(\/)", "union"}
    /*  <xs:enumeration value="\|/" /> */
    , {R"(\/)", "restriction_tail"}
    /*  <xs:enumeration value="^" /> */
    , {"^", "concatenation"}
    /*  <xs:enumeration value="mod" /> */
    , {"mod", "mod"}
    /*  <xs:enumeration value="|->" /> */
    , {"|->", ","}
    /*  <xs:enumeration value="|>" /> */
    , {"|>", "range_restriction"}
    /*  <xs:enumeration value="|>>" /> */
    , {"|>>", "range_substraction"}
    /*  <xs:enumeration value="[" /> */
    , {"[", "image"}
    /*  <xs:enumeration value="(" /> */
    , {"(", "apply"}
    /*  <xs:enumeration value="&lt;'" /> */
    /*  <xs:enumeration value="prj1" /> */
    , {"prj1", "prj1"}
    /*  <xs:enumeration value="prj2" /> */
    , {"prj2", "prj2"}
    /*  <xs:enumeration value="iterate" /> */
    , {"iterate", "iterate"}
    /*  <xs:enumeration value="const" /> */
    /*  <xs:enumeration value="rank" /> */
    /*  <xs:enumeration value="father" /> */
    /*  <xs:enumeration value="subtree" /> */
    /*  <xs:enumeration value="arity" /> */
    /*  <xs:simpleType name="nary_pred_op"> */
    /*  <xs:enumeration value="&amp;" /> */
    , {"&", R"(/\)"}
    /*  <xs:enumeration value="or" /> */
    , {"or", R"(\/)"}
    /* <xs:simpleType name="unary_pred_op"> */
    /* <xs:enumeration value="not" /> */
    , {"not", "not"}
    /* <xs:simpleType name="unary_exp_op"> */
    /*  <xs:enumeration value="max" /> */
    , {"max", "max"}
    /*  <xs:enumeration value="imax" /> */
    , {"imax", "max"}
    /*  <xs:enumeration value="rmax" /> */
    /*  <xs:enumeration value="min" /> */
    , {"min", "min"}
    /*  <xs:enumeration value="imin" /> */
    , {"imin", "min"}
    /*  <xs:enumeration value="rmin" /> */
    /*  <xs:enumeration value="card" /> */
    , {"card", "card"}
    /*  <xs:enumeration value="dom" /> */
    , {"dom", "dom"}
    /*  <xs:enumeration value="ran" /> */
    , {"ran", "ran"}
    /*  <xs:enumeration value="POW" /> */
    , {"POW", "power"}
    /*  <xs:enumeration value="POW1" /> */
    , {"POW1", "non_empty_power"}
    /*  <xs:enumeration value="FIN" /> */
    , {"FIN", "finite_subsets"}
    /*  <xs:enumeration value="FIN1" /> */
    , {"FIN1", "non_empty_finite_subsets"}
    /*  <xs:enumeration value="union" /> */
    , {"union", "generalized_union"}
    /*  <xs:enumeration value="inter" /> */
    , {"inter", "generalized_inter"}
    /*  <xs:enumeration value="seq" /> */
    , {"seq", "seq"}
    /*  <xs:enumeration value="seq1" /> */
    , {"seq1", "seq1"}
    /*  <xs:enumeration value="iseq" /> */
    , {"iseq", "iseq"}
    /*  <xs:enumeration value="iseq1" /> */
    , {"iseq1", "iseq1"}
    /*  <xs:enumeration value="-" /> */
    , {"-", "-"}
    /*  <xs:enumeration value="-i" /> */
    , {"-i", "-"}
    /*  <xs:enumeration value="-r" /> */
    /*  <xs:enumeration value="~" /> */
    , {"~", "inverse"}
    /*  <xs:enumeration value="size" /> */
    , {"size", "size"}
    /*  <xs:enumeration value="perm" /> */
    , {"perm", "perm"}
    /*  <xs:enumeration value="first" /> */
    , {"first", "first"}
    /*  <xs:enumeration value="last" /> */
    , {"last", "last"}
    /*  <xs:enumeration value="id" /> */
    , {"id", "id"}
    /*  <xs:enumeration value="closure" /> */
    , {"closure", "closure"}
    /*  <xs:enumeration value="closure1" /> */
    , {"closure1", "closure1"}
    /*  <xs:enumeration value="tail" /> */
    , {"tail", "tail"}
    /*  <xs:enumeration value="front" /> */
    , {"front", "front"}
    /*  <xs:enumeration value="rev" /> */
    , {"rev", "rev"}
    /*  <xs:enumeration value="conc" /> */
    , {"struct", "struct"}
    /*  <xs:enumeration value="succ" /> */
    /*  <xs:enumeration value="pred" /> */
    /*  <xs:enumeration value="rel" /> */
    , {"rel", "to_relation"}
    /*  <xs:enumeration value="fnc" /> */
    , {"fnc", "to_function"}
    /*  <xs:enumeration value="real" /> */
    /*  <xs:enumeration value="floor" /> */
    /*  <xs:enumeration value="ceiling" /> */
    /*  <xs:enumeration value="tree" /> */
    /*  <xs:enumeration value="btree" /> */
    /*  <xs:enumeration value="top" /> */
    /*  <xs:enumeration value="sons" /> */
    /*  <xs:enumeration value="prefix" /> */
    /*  <xs:enumeration value="postfix" /> */
    /*  <xs:enumeration value="sizet" /> */
    /*  <xs:enumeration value="mirror" /> */
    /*  <xs:enumeration value="left" /> */
    /*  <xs:enumeration value="right" /> */
    /*  <xs:enumeration value="infix" /> */
    /*  <xs:enumeration value="bin" /> */
};

// TODO: Build a table with the translation of every type in the TypeInfo table, 
// so that the translation is computed once and only once.
string whyPredicate::translateTypeInfo
(const XMLElement *ti, const dict_t& enums)
{
    const char *tag {ti->Name()};
    vector<string> localLabels;
    if (cstrEq(tag, "Struct")) {
        return std::string(); /// TODO translateTypeStruct(ti, enums);
    }
    if (cstrEq(tag, "Unary_Exp") && cstrEq(ti->Attribute("op"), "POW")) {
        return "(set " + translateTypeInfo(child1(ti),enums) + ")";
    }
    if (cstrEq(tag, "Binary_Exp") && cstrEq(ti->Attribute("op"), "*")) {
        return "(" + translateTypeInfo(child1(ti),enums) + ","
            + translateTypeInfo(childX(child1(ti)),enums) + ")";
    }
    if (cstrEq(tag, "Id") && cstrEq(ti->Attribute("value"), "INTEGER"))
        return "int";
    if (cstrEq(tag, "Id") && cstrEq(ti->Attribute("value"), "BOOL"))
        return "bool";
    if (cstrEq(tag, "Id") && cstrEq(ti->Attribute("value"), "STRING"))
        return "set (int, int)";
    string valueAttr{};
    if (cstrEq(tag, "Id") && enums.contains(ti->Attribute("value"))) {
        string s {enums.find(ti->Attribute("value"))->second};
        s.erase(0,4); // Pour supprimer le préfixe set_
        return s;
    }
    if (cstrEq(tag, "Id")) {
       return "int";// (*" + ti->Attribute("value") + "*)";
    }

    throw WhyTranslateException(string("Unknown type element ") + tag);
}

bool whyPredicate::isEnumeratedSetElement(const XMLElement *e)
{
    return e != nullptr && 
        cstrEq(e->Name(), "Set") && 
        e->FirstChildElement("Enumerated_Values") != nullptr;
}

bool whyPredicate::isDeferredSetElement(const XMLElement *e)
{
    return e != nullptr && 
        cstrEq(e->Name(), "Set") && 
        e->FirstChildElement("Enumerated_Values") == nullptr;
}

string whyPredicate::whyName(const XMLElement *ident, const dict_t& enums, bool& var) {
    var = false;
    const char *val = ident->Attribute("value");
    if (cstrEq(val, "BOOL"))
        return std::string("b_bool");
    if (cstrEq(val, "INTEGER"))
        return std::string("integer");
    if (cstrEq(val, "NATURAL"))
        return std::string("natural");
    if (cstrEq(val, "NATURAL1"))
        return std::string("natural1");
    if (cstrEq(val, "NAT"))
        return std::string("nat");
    if (cstrEq(val, "NAT1"))
        return std::string("nat1");
    if (cstrEq(val, "INT"))
        return std::string("bounded_int");
    if (cstrEq(val, "STRING"))
        return std::string("string");
    if (cstrEq(val, "MAXINT")) {
        return whyMAXINT;
    }
    if (cstrEq(val, "MININT")) {
        return whyMININT;
    }
    string sval{val};
    if (enums.contains(sval))
        return enums.find(sval)->second;

    var = true;
    string name;
    if (sval.find('.') != std::string::npos) {
        std::replace(sval.begin(), sval.end(), '.', '_');
        name = std::string("renamed_v_");
    } else {
        name = std::string("v_");
    }
    name.append(sval).append("_");
    const auto suffix {ident->Attribute("suffix")};
    if (nullptr != suffix)
        name.append("_").append(suffix);
    return name;
}

string whyPredicate::deferredSetWhyName(const XMLElement *e1)
{
    const XMLElement *id = e1->FirstChildElement("Id");
    return std::string("v_") + id->Attribute("value") + std::string("_");
}

const std::string whyPredicate::whyMAXINT {"b_maxint32_value"};
const std::string whyPredicate::whyMININT {"b_minint32_value"};

void whyPredicate::Translator::translate(const XMLElement *formula)
{
    
    const char *tag {formula->Name()};
    const char *op {formula->Attribute("op")};

    bool rewrites;
    string wop {};
    string bop {};


    if (nullptr != op) {
        bop = string(op);
        rewrites = rewriteBOp.contains(bop);
        if (!rewrites) {
            const auto lookupOp {opDict.find(bop)};
            if (lookupOp == opDict.end()) {
                    throw WhyTranslateException(string("Unknown op attribute: ") + bop);
            }
            wop = lookupOp->second;
        }
    }
    if (cstrEq(tag, "Unary_Pred")) {
        const XMLElement *fce = formula->FirstChildElement();
        if (nullptr == fce) {
            throw WhyTranslateException(string("Ill-formed unary predicate"));
        }
        m_acc << wop << "(";
        translate(fce);
        m_acc << ")";
    }
    else if  (cstrEq(tag, "Binary_Pred")) {
        const XMLElement *fce = formula->FirstChildElement();
        const XMLElement *sce {nullptr};
        if (fce != nullptr)
            sce = fce->NextSiblingElement();
        if (nullptr == fce || nullptr == sce) {
            throw WhyTranslateException(string("Ill-formed binary predicate"));
        }
        m_acc << "(";
        translate(fce);
        m_acc << " " << wop << " ";
        translate(sce);
        m_acc << ")";
    }
    else if  (cstrEq(tag, "Nary_Pred")) {
        const XMLElement *ce = formula->FirstChildElement();
        bool isConj {bop.compare("&") == 0};
        bool isDisj {bop.compare("or") == 0};
        if (!isConj && !isDisj)
            throw WhyTranslateException(string("Nary predicate operator not handled: ") + bop);
        if (nullptr == ce) {
            if (isConj) {
                m_acc << "true";
            }
            else {
                m_acc << "false";
            }
        }
        else {
            if (isConj) {
                bool first {true};
                while(nullptr != ce) {
                    if (first) {
                        first = false;
                    }
                    else {
                        m_acc << "\n " << wop << " ";
                    }
                    translate(ce);
                    ce = ce -> NextSiblingElement();
                }
            }
            else {
                m_acc << "(";
                bool first {true};
                while(nullptr != ce) {
                    if (first) {
                        first = false;
                    }
                    else {
                        m_acc << " " << wop << " ";
                    }
                    translate(ce);
                    ce = ce -> NextSiblingElement();
                }
                m_acc << ")";
            }
        }

    }
    else if (cstrEq(tag, "Quantified_Pred")) {
        const char *bquant {formula->Attribute("type")};
        const auto lookupOp {opDict.find(bquant)};
        if (lookupOp == opDict.end()) {
                throw WhyTranslateException(string("Unknown quantifier: ") + bquant);
        }
        const std::string &wquant {lookupOp->second};
        m_acc << "(" << wquant << " ";
        this->translateVariables(formula->FirstChildElement("Variables"));
        m_acc << ".";
        this->translate(formula->FirstChildElement("Body"));
        m_acc << ")";
    }
    else if  (cstrEq(tag, "Exp_Comparison")) {
        const XMLElement *fce = formula->FirstChildElement();
        const XMLElement *sce {nullptr};
        if (fce != nullptr)
            sce = fce->NextSiblingElement();
        if (nullptr == fce || nullptr == sce) {
            throw WhyTranslateException(string("Ill-formed comparison expression"));
        }
        if (rewrites) {
            if(sstrEq(bop, "=")) {
                const string weq = m_typeinfo.isSet(fce) ? "==" : "=";
                m_acc << "(";
                this->translate(fce);
                m_acc << " " << weq << " ";
                this->translate(sce);
                m_acc << ")";

            } else if(sstrEq(bop, ":")) {
                m_acc << "(mem ";
                translate(fce);
                m_acc << " ";
                translate(sce);
                m_acc << ")";
            }
            else if (sstrEq(bop, "/:")) {
                m_acc << "not(mem ";
                translate(fce);
                m_acc << " ";
                translate(sce);
                m_acc << ")";
            }
            else if (sstrEq(bop, "<:")) {
                m_acc << "(mem ";
                translate(fce);
                m_acc << " (power ";
                translate(sce);
                m_acc << "))";
            }
            else if (sstrEq(bop, "/<:")) {
                m_acc << "not(mem ";
                translate(fce);
                m_acc << " (power ";
                translate(sce);
                m_acc << "))";
            }
            else if (sstrEq(bop, "<<:")) {
                m_acc << "(strictsubset ";
                translate(fce);
                m_acc << " ";
                translate(sce);
                m_acc << ")";
            }
            else if (sstrEq(bop, "/<<:")) {
                m_acc << "not(strictsubset ";
                translate(fce);
                m_acc << " ";
                translate(sce);
                m_acc << ")";
            }
        }
        else {
            m_acc << "(";
            translate(fce);
            m_acc << " " << wop << " ";
            translate(sce);
            m_acc << ")";
        }
    }
    else if (cstrEq(tag, "Unary_Exp")) {
        const XMLElement *fce = formula->FirstChildElement();
        if (nullptr == fce) {
            throw WhyTranslateException(string("Ill-formed unary predicate"));
        }
        if (rewriteBOp.contains(bop)) {
            if (sstrEq(bop, "succ")) {
                m_acc << "(";
                translate(fce);
                m_acc << " + 1)";
            }
            else if (sstrEq(bop, "pred")) {
                m_acc << "(";
                translate(fce);
                m_acc << " - 1)";
            }
            else 
                throw WhyTranslateException(string("Unknown unary expression operator: ") + bop);
        }
        else {
            m_acc << "(" << wop << " ";
            translate(fce);
            m_acc << ")";
        }
    }
    else if (cstrEq(tag, "Binary_Exp")) {
        const XMLElement *fce = formula->FirstChildElement();
        const XMLElement *sce {nullptr};
        if (fce != nullptr)
            sce = fce->NextSiblingElement();
        if (nullptr == fce || nullptr == sce) {
            throw WhyTranslateException(string("Ill-formed binary expression"));
        }
        if (rewriteBOp.contains(bop)) {
            if (sstrEq(bop, "<'")) {
                m_acc << "{";
                translate(fce);
                m_acc << " with l_" << sce->Attribute("value")
                    << " = ";
                translate(sce);
                m_acc << "}";
            }
            else
                throw WhyTranslateException(string("Operator not recognized: ") + bop);
        }
        else if (prefixedWOp.contains(wop)) {
            m_acc << "(" << wop << " ";
            translate(fce);
            m_acc << " ";
            translate(sce);
            m_acc << ")";
        }
        else {
            m_acc << "(";
            translate(fce);
            m_acc << " " << wop << " ";
            translate(sce);
            m_acc << ")";
        }
    }
    else if  (cstrEq(tag, "Ternary_Exp")) {
        const XMLElement *fce = formula->FirstChildElement();
        const XMLElement *sce {nullptr};
        const XMLElement *tce {nullptr};
        if (fce != nullptr)
            sce = fce->NextSiblingElement();
        if (sce != nullptr)
            tce = sce->NextSiblingElement();
    }
    else if (cstrEq(tag, "Nary_Exp")) {
        const XMLElement *fce = formula->FirstChildElement();
        if (nullptr == fce) {
            throw WhyTranslateException(string("Ill-formed n-ary expression"));
        }
        if (sstrEq(bop, "{")) {
            translateExtensionSet(fce);
        }
        else if (sstrEq(bop, "[")) {
            translateExtensionSeq(fce, 1);
        }
        else {
            throw WhyTranslateException(string("Unknown n-ary expression operator: " + bop));
        }
    }
    else if (cstrEq(tag, "Quantified_Exp")) {
        // TODO
    }
    else if (cstrEq(tag, "Quantified_Set")) {
        // TODO
    }
    else if (cstrEq(tag, "Boolean_Exp")) {
        const XMLElement *fce = formula->FirstChildElement();
        if (nullptr == fce) {
            throw WhyTranslateException(string("Ill-formed unary predicate"));
        }
        m_acc << "(if ";
        this->translate(fce);
        m_acc << " then True else False)";
    }
    else if (cstrEq(tag, "Id")) {
        bool dummy;
        m_acc << whyName(formula, m_enums, dummy);
    }
    else if (cstrEq(tag, "Integer_Literal") || cstrEq(tag, "Real_Literal")) {
        const auto val {formula->Attribute("value")};
        if (nullptr != val && *val == '-')
            m_acc << "(" << val << ")";
        else 
            m_acc << val;
    }
    else if (cstrEq(tag, "Boolean_Literal")) {
        const string val {formula->Attribute("value")};
        if (sstrEq(val, "TRUE"))
            m_acc << "True";
        else if (sstrEq(val, "FALSE")) {
            m_acc << "False";
        }
        else {
            throw WhyTranslateException(string("Unknown boolean literal: ") + val);
        }
    }
    else if (cstrEq(tag, "Goal")) {
        translate(formula->FirstChildElement());
    }
    else if (cstrEq(tag, "Boolean_Exp") || cstrEq(tag, "EmptySet")) {
        m_acc << "(empty : " << m_typeinfo.getTypeTranslation(formula) << ")";
    }
    else {
        std::cerr << "whyPredicate::translate : tag not handled: " << tag << std::endl;
    }
}

void whyPredicate::Translator::translateVariables(const XMLElement *formula) {
    string res;
    bool first = true;
    const XMLElement *fce = formula->FirstChildElement("Id");
    while (fce != nullptr) {
        bool dummy;
        if (first) {
            first = false;
        } else {
            m_acc << ", ";
        }
        m_acc << whyName(fce, m_enums, dummy);
        m_acc << ": ";
        m_acc << m_typeinfo.getTypeTranslation(fce);

        fce = fce->NextSiblingElement("Id");
    }
}

void whyPredicate::Translator::translateExtensionSet(const XMLElement *ce)
{
    const XMLElement *ne = ce->NextSiblingElement();
    if (nullptr == ne) {
        m_acc << "(singleton ";
        this->translate(ce);
        m_acc << ")";
    }
    else {
        m_acc << "(union (singleton ";
        this->translate(ce);
        m_acc << ") ";
        this->translateExtensionSet(ne);
        m_acc << ") ";
    }
}

void whyPredicate::Translator::translateExtensionSeq(const XMLElement *ce, unsigned index)
{
    const XMLElement *ne = ce->NextSiblingElement();
    if (nullptr == ne) {
        m_acc << "(singleton (" << index << ", ";
        this->translate(ce);
        m_acc << "))";
    }
    else {
        m_acc << "(union (singleton " << index << ", ";
        this->translate(ce);
        m_acc << ")) ";
        this->translateExtensionSeq(ne, index+1);
        m_acc << ") ";
    }
}

string whyPredicate::Translator::translation() {
    return m_acc.str();
}

ostream& operator<<(ostream &os, const whyPredicate &wp) {
    os << "--- whyPredicate --- " << std::endl
         << "name: " << wp.m_name << std::endl
         << "variables: " << std::endl;
    for (const auto &v : wp.m_variables) {
        os << "  " << v << std::endl;
    }
    os << "types: " << std::endl;
    for (const auto &t : wp.m_types) {
        os << "  " << t.first << " -> " << t.second << std::endl;
    }
    os << "functions: " << std::endl;
    for (const auto &f : wp.m_functions) {
        os << "  " << f << std::endl;
    }
    os << "translation: " << wp.m_translation << std::endl;
    os << "---" << std::endl;
    return os;
}
