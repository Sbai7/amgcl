#include <iostream>
#include <string>

#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <amgcl/backend/vexcl.hpp>
#include <amgcl/runtime.hpp>
#include <amgcl/make_solver.hpp>
#include <amgcl/adapter/crs_tuple.hpp>
#include <amgcl/profiler.hpp>

#include "sample_problem.hpp"

namespace amgcl {
    profiler<> prof;
}

int main(int argc, char *argv[]) {
    using amgcl::prof;

    // Read configuration from command line
    int m = 32;
    bool just_relax = false;
    amgcl::runtime::coarsening::type coarsening = amgcl::runtime::coarsening::smoothed_aggregation;
    amgcl::runtime::relaxation::type relaxation = amgcl::runtime::relaxation::spai0;
    amgcl::runtime::solver::type     solver     = amgcl::runtime::solver::bicgstab;
    std::string parameter_file;

    namespace po = boost::program_options;
    po::options_description desc("Options");

    desc.add_options()
        ("help,h", "show help")
        (
         "size,n",
         po::value<int>(&m)->default_value(m),
         "domain size"
        )
        (
         "coarsening,c",
         po::value<amgcl::runtime::coarsening::type>(&coarsening)->default_value(coarsening),
         "ruge_stuben, aggregation, smoothed_aggregation, smoothed_aggr_emin"
        )
        (
         "relaxation,r",
         po::value<amgcl::runtime::relaxation::type>(&relaxation)->default_value(relaxation),
         "parallel_ilu0, damped_jacobi, spai0, chebyshev"
        )
        (
         "just-relax,0",
         po::bool_switch(&just_relax),
         "Do not create AMG hierarchy, use relaxation as preconditioner"
        )
        (
         "solver,s",
         po::value<amgcl::runtime::solver::type>(&solver)->default_value(solver),
         "cg, bicgstab, bicgstabl, gmres"
        )
        (
         "params,p",
         po::value<std::string>(&parameter_file),
         "parameter file in json format"
        )
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
    }

    boost::property_tree::ptree prm;
    if (vm.count("params")) read_json(parameter_file, prm);

    vex::Context ctx(vex::Filter::Env && vex::Filter::DoublePrecision);
    std::cout << ctx << std::endl;

    // Assemble problem
    prof.tic("assemble");
    std::vector<int>    ptr;
    std::vector<int>    col;
    std::vector<double> val;
    std::vector<double> rhs;

    int n = sample_problem(m, val, col, ptr, rhs);
    prof.toc("assemble");

    // Solve the problem
    vex::vector<double> f(ctx, rhs);
    vex::vector<double> x(ctx, n);
    x = 0;

    size_t iters;
    double resid;

    typedef amgcl::backend::vexcl<double> Backend;
    typename Backend::params bprm;
    bprm.q = ctx;

    prm.put("solver.type", solver);

    if (just_relax) {
        prm.put("precond.type", relaxation);

        prof.tic("setup");
        amgcl::make_solver<
            amgcl::runtime::relaxation::as_preconditioner<Backend>,
            amgcl::runtime::iterative_solver<Backend>
            > solve(boost::tie(n, ptr, col, val), prm, bprm);
        prof.toc("setup");

        std::cout << "Using relaxation as preconditioner" << std::endl;

        prof.tic("solve");
        boost::tie(iters, resid) = solve(f, x);
        prof.toc("solve");
    } else {
        prm.put("precond.coarsening.type", coarsening);
        prm.put("precond.relax.type", relaxation);

        prof.tic("setup");
        amgcl::make_solver<
            amgcl::runtime::amg<Backend>,
            amgcl::runtime::iterative_solver<Backend>
                > solve(boost::tie(n, ptr, col, val), prm, bprm);
        prof.toc("setup");

        std::cout << solve.precond() << std::endl;

        prof.tic("solve");
        boost::tie(iters, resid) = solve(f, x);
        prof.toc("solve");
    }

    std::cout << "Iterations: " << iters << std::endl
              << "Error:      " << resid << std::endl
              << std::endl      << prof  << std::endl;
}
