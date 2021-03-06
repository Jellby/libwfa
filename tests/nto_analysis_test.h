#ifndef LIBWFA_NTO_ANALYSIS_TEST_H
#define LIBWFA_NTO_ANALYSIS_TEST_H

#include "test_base.h"
#include <libwfa/analyses/nto_analysis.h>

namespace libwfa {


/** \brief Tests the libwfa::nto_analysis class

    \ingroup libwfa_tests
 **/
class nto_analysis_test : public test_base {
public:
    virtual void perform() throw(libtest::test_exception);

private:
    void test_form_eh_1() throw(libtest::test_exception);
    void test_form_eh_2() throw(libtest::test_exception);

    template<typename TestData>
    void test_1() throw(libtest::test_exception);
};


} // namespace libwfa

#endif // LIBWFA_NTO_ANALYSIS_TEST_H
