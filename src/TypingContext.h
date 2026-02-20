#ifndef TYPING_CONTEXT_H
#define TYPING_CONTEXT_H

#include<iostream>
using std::ostream;

#include<string>
using std::string;

#include<vector>
using std::vector;

#include "tinyxml2.h"
using tinyxml2::XMLDocument;
using tinyxml2::XMLElement;

/**
 * @brief The TypingContext class is used to translate B types to Why3 types.
 * 
 * The TypingContext class is used to translate B types to Why3 types.
 * It is initialized with the TypeInfos section of the DOM.
 * It provides functions to get the type of a B expression and to obtain the Why3 translation of this type.
*/
class TypingContext {

public:
    /// @param doc DOM representation of a B POG file
    explicit TypingContext(XMLDocument &doc);

    /// @param context the TypeInfos section of the DOM
    /// @param dom DOM representation of a B expression
    /// @return 
    const XMLElement * getTypeElement(const XMLElement *dom) const;
    const XMLElement * getTypeElement(unsigned typref) const;
    string getTypeTranslation(const XMLElement *dom) const;
    string getTypeTranslation(unsigned typref) const;

    bool isInt(const XMLElement *dom) const;
    bool isReal(const XMLElement *dom) const;
    bool isFloat(const XMLElement *dom) const;
    bool isSet(const XMLElement *dom) const;

    friend std::ostream& operator<<(ostream& os, const TypingContext& tc);

private:
    vector<const XMLElement *> m_dom;
    vector<string> m_translations;

    unsigned m_INTEGER;
    unsigned m_REAL;
    unsigned m_FLOAT;

    static unsigned getTypRef(const XMLElement *typref);
};

#endif // TYPING_CONTEXT_H
