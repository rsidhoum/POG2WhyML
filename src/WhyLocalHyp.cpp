#include "WhyMLInternal.h"

whyLocalHyp::whyLocalHyp(const XMLElement *dom, const std::string &group, unsigned num)
    : whyPredicate{dom, string("lh_") + group + "_" + std::to_string(num)}
    , m_group{group}
    , m_num{num}
    , m_duplicate{nullptr}
{
    m_translation = string("true");
}


void whyLocalHyp::declareWithoutDuplicate(const vector<whyLocalHyp*>& hypotheses, ofstream& why) {
    for (const auto& h: hypotheses) {
        if (h->m_translation == m_translation
                &&  h->m_variables.size() == m_variables.size()) {
            bool dup = true;
            for (const auto& v: m_variables) {
                if (!h->m_variables.contains(v)) {
                    dup = false;
                    break;
                }
                const auto& ith {h->m_types.find(v)};
                const auto& it {m_types.find(v)};
                bool inh {ith != h->m_types.end()};
                bool in {it != m_types.end()};
                if (!(in == inh &&
                      (!in || ith->second == it->second))) {
                    dup = false;
                    break;
                }
            }
            if (dup) {
                m_duplicate = h;
                return;
            }
        }
    }
    declare(why);
}

string whyLocalHyp::declareCallDuplicate() {
    return string{};
    /* TODO
    if (duplicate == 0) {
        return declareCall();
    } else
        return duplicate->declareCallDuplicate();
    */
}

ostream &operator<<(ostream &os, const whyLocalHyp &wp) {
    os << "--- whyLocalHyp " << std::endl
       << static_cast<const whyPredicate&>(wp);
    os << "group tag: " << wp.m_group << std::endl;
    os << "num: " << wp.m_num << std::endl;
    os << "--- end whyLocalHyp " << std::endl << std::endl;
    return os;
}
