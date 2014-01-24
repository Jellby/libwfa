#ifndef LIBWFA_POP_ANALYSIS_DM_H
#define LIBWFA_POP_ANALYSIS_DM_H

#include "ab_matrix.h"
#include "pop_analysis_i.h"
#include "pop_print_i.h"

namespace libwfa {

/** \brief Perform population analysis on state density matrix

    \ingroup libwfa
 **/
class pop_analysis_dm {
private:
    const pop_analysis_i &m_analysis;

public:
    /** \brief Constructor
        \param a Analysis object
     **/
    pop_analysis_dm(const pop_analysis_i &a) : m_analysis(a) { }

    /** \brief Perform population analysis
        \param[in] sdm State density matrix
        \param[out] pr Printer of population data
     **/
    void perform(const ab_matrix &sdm, pop_print_i &pr) const;
};

} // namespace libwfa

#endif // LIBWFA_POP_ANALYSIS_DM_H
