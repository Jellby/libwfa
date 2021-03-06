#include <iostream>
#include <iomanip>
#include <fstream>
#include <libwfa/libwfa.h>
#include <libwfa/molcas/molcas_wf_analysis.h>
#include "H5Cpp.h"

namespace libwfa {
using namespace H5;

const char molcas_wf_analysis::k_clazz[] = "molcas_wf_analysis";

void molcas_wf_analysis::run_analysis() {

    std::string molcas_module = m_mdata->molcas_module();

    if (molcas_module=="SCF") {
        scf_analysis();
    }
    else if (molcas_module=="RASSCF") {
        rasscf_analysis(m_mdata->input()->refstate);
    }
    else if (molcas_module=="RASSI") {
        rassi_analysis(m_mdata->input()->refstate);
    }
    else {
        std::ostringstream os;
        os << std::endl << "Unsupported MOLCAS module: " << molcas_module;
        const std::string errmsg = os.str();
        throw libwfa_exception("main", "main", __FILE__, __LINE__, errmsg.c_str());
    }
}

void molcas_wf_analysis::scf_analysis() {
    header1("SCF MO Analysis");
    ab_matrix dm0 = m_mdata->build_dm(0, 0, true);
    analyse_opdm_ai("GS", "SCF ground state", dm0);
}

void molcas_wf_analysis::rasscf_analysis(size_t refstate) {
    header1("RASSCF Density Matrix Analysis");
    std::string label = m_mdata->rasscf_label();
    std::string labele = label;
    labele.erase(std::remove(labele.begin(), labele.end(), ' '), labele.end());

    // Density matrix
    arma::cube dens = m_mdata->read_dens_raw("DENSITY_MATRIX");
    if (refstate >= dens.n_slices)
        throw libwfa_exception(k_clazz, "rasscf_analysis", __FILE__, __LINE__, "refstate > nstate");

    double *dens_buf = dens.memptr();

    size_t dens_offs = dens.n_rows * dens.n_cols;

    // Spin-density matrix
    arma::cube sdens = m_mdata->read_dens_raw("SPINDENSITY_MATRIX");
    double *sdens_buf = sdens.memptr();

    double *smin = std::min_element(sdens_buf, sdens_buf + sdens.size());
    double *smax = std::max_element(sdens_buf, sdens_buf + sdens.size());
    bool aeqb_dens = true;
    if (*smax-*smin > 1.e-6) {
        std::cout << "Found non-vanishing spin-density, activating spin-analysis." << std::endl << std::endl;
        aeqb_dens = false;
    }

    ab_matrix dm0 = m_mdata->build_dm(dens_buf + refstate*dens_offs, sdens_buf + refstate*dens_offs, aeqb_dens);

    // Read the energies
    arma::vec ener = m_mdata->read_vec_h5("ROOT_ENERGIES");
    ener -= ener(refstate);

    // Loop over all states
    for (int istate = 0; istate < dens.n_slices; istate++) {
        std::ostringstream name, descr, header;
        descr << istate+1 << " " << label;
        name << istate+1 << labele;

        if (istate == refstate) {
            header << "RASSCF analysis for reference state " << descr.str();
            header2(header.str());
            m_mdata->energy_print(ener(istate), std::cout);

            analyse_opdm_ai(name.str(), descr.str(), dm0);
        }
        else {
            header << "RASSCF analysis for state " << descr.str();
            header2(header.str());
            m_mdata->energy_print(ener(istate), std::cout);

            ab_matrix ddm = m_mdata->build_dm(dens_buf + istate*dens_offs, sdens_buf + istate*dens_offs, aeqb_dens);
            ddm -= dm0;
            analyse_opdm_ai(name.str(), descr.str(), ddm, dm0);
        }
    }
}

void molcas_wf_analysis::rassi_analysis(size_t refstate) {
    header1("RASSI (Transition) Density Matrix Analysis");

    // Read the densities
    arma::cube tden = m_mdata->read_dens_raw("SFS_TRANSITION_DENSITIES");
    if (refstate >= tden.n_slices)
        throw libwfa_exception(k_clazz, "rassi_analysis", __FILE__, __LINE__, "refstate > nstate");

    double *tden_buf = tden.memptr();

    // Read the spin densities
    arma::cube tsden = m_mdata->read_dens_raw("SFS_TRANSITION_SPIN_DENSITIES");
    double *tsden_buf = tsden.memptr();

    //ab_matrix dm0 = m_mdata->build_dm_ao(tden_buf + (refstate + refstate * tden.n_cols)*tden.n_rows, tsden_buf + (refstate + refstate * tden.n_cols)*tden.n_rows, tden.n_rows);
    ab_matrix dm0 = m_mdata->build_dm_ao(tden_buf + (refstate + refstate * tden.n_cols)*tden.n_rows, NULL, tden.n_rows);

    // Read the energies
    arma::vec ener = m_mdata->read_vec_h5("SFS_ENERGIES");
    ener -= ener(refstate);

    // Loop over all transition densities
    for (int istate = 0; istate < tden.n_slices; istate++) {
        if (istate == refstate) {
            std::ostringstream name, descr, header;

            name << "R_" << refstate+1;
            descr << name.str() << " " << std::setprecision(5) << ener(istate);

            header << "RASSI analysis for reference state " << name.str();
            header2(header.str());
            m_mdata->energy_print(ener(istate), std::cout);

            analyse_opdm_ai(name.str(), descr.str(), dm0);
        }
        else {
            { // State/difference density analysis
                std::ostringstream name, descr, header;

                name << "R_" << istate+1;
                descr << name.str() << " " << std::setprecision(5) << ener(istate);

                header << "RASSI analysis for state " << name.str();
                header2(header.str());
                m_mdata->energy_print(ener(istate), std::cout);

                std::cout << "Using spin-traced density matrices ..." << std::endl << std::endl;

                const double *itden_buf  = tden_buf + (istate + istate * tden.n_cols)*tden.n_rows;

                // The state spin-densities are not understood consistently.
                //   Probably because of the contributions from inactive orbitals.
                //   They are ignored here
                const double *itsden_buf = NULL; //tsden_buf + (istate + istate * tden.n_cols)*tden.n_rows;
                //const double *itsden_buf = tsden_buf + (istate + istate * tden.n_cols)*tden.n_rows;

                ab_matrix ddm = m_mdata->build_dm_ao(itden_buf, itsden_buf, tden.n_rows);
                ddm -= dm0;

                analyse_opdm_ai(name.str(), descr.str(), ddm, dm0);
            }
            { // Transition density analysis
                std::ostringstream name, descr, header;

                name << "Tr_" << refstate+1 << "-" << istate+1;
                descr << name.str() << " " << std::setprecision(5) << ener(istate);

                header << "RASSI analysis for transiton from state " << refstate+1 << " to " << istate+1 << " (" << name.str() << ")";
                header2(header.str());
                m_mdata->energy_print(ener(istate), std::cout);

                int jstate = std::min(istate, (int)refstate);
                int kstate = std::max(istate, (int)refstate);

                const double *itden_buf  = tden_buf + (kstate + jstate * tden.n_cols)*tden.n_rows;
                const double *itsden_buf = tsden_buf + (kstate + jstate * tden.n_cols)*tden.n_rows;
                ab_matrix tdm = m_mdata->build_dm_ao(itden_buf, itsden_buf, tden.n_rows);
                if (istate > (int)refstate) // Transpose of the indices are switched
                    tdm.inplace_trans();

                analyse_optdm_ai(name.str(), descr.str(), tdm);
            }
        }
    }
}

void molcas_wf_analysis::header1(std::string title) {
    int lspace = (76 - title.size()) / 2;

    std::cout << "  " << std::string(76, '-') << std::endl;
    std::cout << std::string(lspace, ' ') << title << std::endl;
    std::cout << "  " << std::string(76, '-') << std::endl << std::endl;
}

void molcas_wf_analysis::header2(std::string title) {
    std::cout << std::endl << "  " << title << std::endl;
    std::cout << "  " << std::string(title.size(), '-') << std::endl;
}

void molcas_wf_analysis::analyse_opdm_ai(const std::string &name, const std::string &desc,
        const ab_matrix &ddm, const ab_matrix &dm0) {

    std::stringstream out;
    analyse_opdm(out, name, desc, ddm, dm0);
    std::cout << out.str();

    if (m_mdata->input()->add_info) add_molcas_info(out);
}

void molcas_wf_analysis::analyse_opdm_ai(const std::string &name, const std::string &desc,
        const ab_matrix &sdm) {

    std::stringstream out;
    analyse_opdm(out, name, desc, sdm);
    std::cout << out.str();

    if (m_mdata->input()->add_info) add_molcas_info(out);
}

void molcas_wf_analysis::analyse_optdm_ai(const std::string &name, const std::string &desc,
        const ab_matrix &tdm) {

    std::stringstream out;
    analyse_optdm(out, name, desc, tdm);
    std::cout << out.str();

    if (m_mdata->input()->add_info) add_molcas_info(out);
}

void molcas_wf_analysis::add_molcas_info(std::stringstream &out) {
    size_t prec = 6;

    std::ofstream finfo;
    finfo.open("molcas_info", std::ofstream::app);

    double x;
    std::string str;

    finfo << std::setw(6);
    while (out >> str) {
        if (std::stringstream(str) >> x)
            if (x*x > 2.e-12)
            {
                finfo <<    "LIBWFA[" << m_info << "]=\"" << std::setprecision(prec)
                    << std::fixed << x << "\"" << std::endl;
                finfo << "#> LIBWFA[" << m_info << "]=\"" << std::setprecision(prec)
                    << std::fixed << x << "\"/" << prec << std::endl;
                m_info += 1;
            }
    }
    finfo << "export LIBWFA" << std::endl;

}

} // namespace libwfa