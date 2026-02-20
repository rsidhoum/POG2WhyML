#include "TypingContext.h"

#include <sstream>
using std::stringstream;

#include <cassert>
#include <cstdint>

#include "exceptions.h"
#include "utils.h"

#include <iostream>

static void translateType(stringstream& ss, const XMLElement *elem);

TypingContext::TypingContext(XMLDocument &doc)
{
    const XMLElement *typeElem {doc.RootElement()->FirstChildElement("TypeInfos")->FirstChildElement("Type")};
    uint64_t counter = 0;
    m_INTEGER = m_REAL = m_FLOAT = UINT_MAX;
    while(nullptr != typeElem) {
        uint64_t id = typeElem->Unsigned64Attribute("id");
        assert(id == counter);
        if (m_INTEGER == UINT_MAX && isInt(typeElem)) {
            m_INTEGER = counter;
        }
        else if (m_REAL == UINT_MAX && isReal(typeElem)) {
            m_REAL = counter;
        }
        else if (m_FLOAT == UINT_MAX && isFloat(typeElem)) {
            m_FLOAT = counter;
        }
        m_dom.push_back(typeElem);
        typeElem = typeElem->NextSiblingElement("Type");
        ++counter;
    }
    for (unsigned i = 0; i < m_dom.size(); ++i) {
        stringstream ss;
        translateType(ss, m_dom.at(i)->FirstChildElement()
        );
        m_translations.push_back(ss.str());
    }
 }

static void translateType(stringstream& ss, const XMLElement *elem)
{
    const char *tag {elem->Name()};
    const char *op {elem->Attribute("op")};
    const char *value {elem->Attribute("value")};
    vector<string> localLabels;
    if (cstrEq(tag, "Unary_Exp") && cstrEq(op, "POW")) {
        ss << "(set ";
        translateType(ss, child1(elem));
        ss << ")";
    }
    else if (cstrEq(tag, "Binary_Exp") && cstrEq(op, "*")) {
        ss << "(";
        translateType(ss, child1(elem));
        ss << ",";
        translateType(ss, childX(child1(elem)));
        ss << ")";
    }
    else if (cstrEq(tag, "Id") && cstrEq(value, "INTEGER"))
        ss << "int";
    else if (cstrEq(tag, "Id") && cstrEq(value, "BOOL"))
        ss << "bool";
    else if (cstrEq(tag, "Id") && cstrEq(value, "STRING"))
        ss << "set (int, int)";
    else 
        WhyTranslateException(string("Unknown type element ") + tag);
}

unsigned TypingContext::getTypRef(const XMLElement *dom) {
    if (nullptr != dom->Attribute("typref")) {
        return dom->UnsignedAttribute("typref");
    }
    //assert(false);
    return UINT_MAX;
}
const XMLElement *TypingContext::getTypeElement(const XMLElement *dom) const {
    return m_dom.at(this->getTypRef(dom));
}

const XMLElement *TypingContext::getTypeElement(unsigned typref) const {
    assert(typref < m_dom.size());
    return m_dom.at(typref);
}

string TypingContext::getTypeTranslation(const XMLElement *dom) const {
    return this->getTypeTranslation(this->getTypRef(dom));
}

string TypingContext::getTypeTranslation(unsigned typref) const {
    assert(typref < m_dom.size());
    return m_translations.at(typref);
}

bool TypingContext::isInt(const XMLElement *dom) const  {
    return this->getTypRef(dom) == m_INTEGER;
}

bool TypingContext::isReal(const XMLElement *dom) const {
    return this->getTypRef(dom) == m_REAL;
}

bool TypingContext::isFloat(const XMLElement *dom) const {
    return this->getTypRef(dom) == m_FLOAT;
}

bool TypingContext::isSet(const XMLElement *dom) const {
    const XMLElement *typeElem {this->getTypeElement(dom)};
    return cstrEq(typeElem->Name(), "Unary_Exp") && cstrEq(typeElem->Attribute("op"), "POW");
}

ostream & operator<<(ostream &os, const TypingContext &tc) {

    std::cout << "--- TypingContext ---" << std::endl;
    for (unsigned i = 0; i < tc.m_translations.size(); ++i) {
        std::cout << "typref " << i << " " << tc.m_translations.at(i) << std::endl;
    }
    std::cout << "---" << std::endl;

    return os;
}
