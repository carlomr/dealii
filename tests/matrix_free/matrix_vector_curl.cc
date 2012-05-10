//------------------  matrix_vector_curl.cc  ------------------------
//    $Id$
//    Version: $Name$
//
//------------------  matrix_vector_curl.cc  ------------------------


// this function tests the correctness of the implementation of matrix free
// matrix-vector products by comparing with the result of deal.II sparse
// matrix. The mesh uses a hyperball mesh with hanging nodes for a
// vector-valued problem (curl-curl operator which does not really make a lot
// of sense from a problem point of view, though).

#include "../tests.h"

std::ofstream logfile("matrix_vector_curl/output");

#include <deal.II/matrix_free/matrix_free.h>
#include <deal.II/matrix_free/fe_evaluation.h>

#include <deal.II/base/logstream.h>
#include <deal.II/base/utilities.h>
#include <deal.II/lac/block_vector.h>
#include <deal.II/grid/tria.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria_boundary_lib.h>
#include <deal.II/dofs/dof_tools.h>
#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_renumbering.h>
#include <deal.II/lac/constraint_matrix.h>
#include <deal.II/lac/block_sparse_matrix.h>
#include <deal.II/lac/block_sparsity_pattern.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/numerics/vectors.h>

#include <fstream>
#include <iostream>
#include <complex>

const double global_coefficient = 0.1;


template <int dim, int degree, typename VectorType>
class MatrixFreeTest
{
 public:
  typedef typename DoFHandler<dim>::active_cell_iterator CellIterator;
  typedef double Number;

  MatrixFreeTest(const MatrixFree<dim,Number> &data_in):
    data (data_in)
  {};

  void
  local_apply (const MatrixFree<dim,Number> &data,
               VectorType          &dst,
               const VectorType    &src,
               const std::pair<unsigned int,unsigned int> &cell_range) const
  {
    typedef VectorizedArray<Number> vector_t;
    FEEvaluation<dim,degree,degree+1,dim,Number> phi (data);
    vector_t coeff = make_vectorized_array(global_coefficient);

    for(unsigned int cell=cell_range.first;cell<cell_range.second;++cell)
      {
        phi.reinit (cell);
        phi.read_dof_values (src);
        phi.evaluate (false,true,false);

        for (unsigned int q=0; q<phi.n_q_points; ++q)
          phi.submit_curl (coeff * phi.get_curl(q), q);

        phi.integrate (false,true);
        phi.distribute_local_to_global (dst);
      }
  }

  void vmult (VectorType &dst,
              const VectorType &src) const
  {
    AssertDimension (dst.size(), dim);
    for (unsigned int d=0; d<dim; ++d)
      dst[d] = 0;
    data.cell_loop (&MatrixFreeTest<dim,degree,VectorType>::local_apply,
                    this, dst, src);
  };

private:
  const MatrixFree<dim,Number> &data;
};



template <int dim, int fe_degree>
void test ()
{
  Triangulation<dim>   tria;
  GridGenerator::hyper_ball (tria);
  static const HyperBallBoundary<dim> boundary;
  tria.set_boundary (0, boundary);
  tria.refine_global(4-dim);

                                // refine a few cells
  for (unsigned int i=0; i<10-3*dim; ++i)
    {
      typename Triangulation<dim>::active_cell_iterator
        cell = tria.begin_active (),
        endc = tria.end();
      unsigned int counter = 0;
      for (; cell!=endc; ++cell, ++counter)
        if (counter % (7-i) == 0)
          cell->set_refine_flag();
      tria.execute_coarsening_and_refinement();
    }


  FE_Q<dim>            fe_sca (QGaussLobatto<1>(fe_degree+1));
  FESystem<dim>        fe (fe_sca, dim);
  DoFHandler<dim>      dof_handler_sca (tria);
  DoFHandler<dim>      dof_handler (tria);

  MatrixFree<dim,double> mf_data;

  ConstraintMatrix     constraints;

  BlockSparsityPattern      sparsity_pattern;
  BlockSparseMatrix<double> system_matrix;

  BlockVector<double> solution;
  BlockVector<double> system_rhs;
  std::vector<Vector<double> > vec1, vec2;

  dof_handler.distribute_dofs (fe);
  dof_handler_sca.distribute_dofs (fe_sca);
  DoFRenumbering::component_wise (dof_handler);

  DoFTools::make_hanging_node_constraints (dof_handler, constraints);
  constraints.close ();

  const unsigned int dofs_per_block = dof_handler_sca.n_dofs();
  {
    BlockCompressedSimpleSparsityPattern csp (dim,dim);
    for (unsigned int d=0; d<dim; ++d)
      for (unsigned int e=0; e<dim; ++e)
        csp.block(d,e).reinit (dofs_per_block, dofs_per_block);

    csp.collect_sizes();

    DoFTools::make_sparsity_pattern (dof_handler, csp, constraints, false);
    sparsity_pattern.copy_from (csp);
  }

  system_matrix.reinit (sparsity_pattern);

  solution.reinit (dim);
  for (unsigned int i=0; i<dim; ++i)
    solution.block(i).reinit (dofs_per_block);
  solution.collect_sizes ();

  system_rhs.reinit (solution);

  vec1.resize (dim);
  vec2.resize (dim);
  vec1[0].reinit (dofs_per_block);
  vec2[0].reinit (vec1[0]);
  for (unsigned int i=1; i<dim; ++i)
    {
      vec1[i].reinit (vec1[0]);
      vec2[i].reinit (vec1[0]);
    }

                                // assemble curl-curl operator
  {
    QGauss<dim>   quadrature_formula(fe_degree+1);

    FEValues<dim> fe_values (fe, quadrature_formula,
                             update_values    |
                             update_JxW_values |
                             update_gradients);

    const unsigned int   dofs_per_cell   = fe.dofs_per_cell;
    const unsigned int   n_q_points      = quadrature_formula.size();

    FullMatrix<double>   local_matrix (dofs_per_cell, dofs_per_cell);

    std::vector<unsigned int> local_dof_indices (dofs_per_cell);

    const FEValuesExtractors::Vector sc (0);

    const unsigned int curl_dim = dim==2 ? 1 : dim;
    std::vector<Tensor<1,curl_dim> > phi_curl (dofs_per_cell);

    typename DoFHandler<dim>::active_cell_iterator
      cell = dof_handler.begin_active(),
      endc = dof_handler.end();
    for (; cell!=endc; ++cell)
      {
        fe_values.reinit (cell);
        local_matrix = 0;

        for (unsigned int q=0; q<n_q_points; ++q)
          {
            for (unsigned int k=0; k<dofs_per_cell; ++k)
              {
                const Tensor<2,dim> phi_grad = fe_values[sc].gradient(k,q);
                if (dim == 2)
                  phi_curl[k][0] = phi_grad[1][0] - phi_grad[0][1];
                else
                  {
                    phi_curl[k][0] = phi_grad[2][1] - phi_grad[1][2];
                    phi_curl[k][1] = phi_grad[0][2] - phi_grad[2][0];
                    phi_curl[k][2] = phi_grad[1][0] - phi_grad[0][1];
                  }
              }

            for (unsigned int i=0; i<dofs_per_cell; ++i)
              {
                for (unsigned int j=0; j<=i; ++j)
                  {
                    local_matrix(i,j) += (phi_curl[i] * phi_curl[j] *
                                          global_coefficient)
                      * fe_values.JxW(q);
                  }
              }
          }
        for (unsigned int i=0; i<dofs_per_cell; ++i)
          for (unsigned int j=i+1; j<dofs_per_cell; ++j)
            local_matrix(i,j) = local_matrix(j,i);

        cell->get_dof_indices (local_dof_indices);
        constraints.distribute_local_to_global (local_matrix,
                                                local_dof_indices,
                                                system_matrix);
      }
  }

                                // first system_rhs with random numbers
  for (unsigned int i=0; i<dim; ++i)
    for (unsigned int j=0; j<system_rhs.block(i).size(); ++j)
      {
        const double val = -1. + 2.*(double)rand()/double(RAND_MAX);
        system_rhs.block(i)(j) = val;
      }
  constraints.condense(system_rhs);
  for (unsigned int i=0; i<dim; ++i)
    vec1[i] = system_rhs.block(i);

                                // setup matrix-free structure
  {
    QGauss<1> quad(fe_degree+1);
    mf_data.reinit (dof_handler_sca, constraints, quad,
                    typename MatrixFree<dim>::AdditionalData
                    (MPI_COMM_WORLD,
                     MatrixFree<dim>::AdditionalData::none));
  }

  system_matrix.vmult (solution, system_rhs);

  typedef std::vector<Vector<double> > VectorType;
  MatrixFreeTest<dim,fe_degree,VectorType> mf (mf_data);
  mf.vmult (vec2, vec1);

                                // Verification
  double error = 0.;
  for (unsigned int i=0; i<dim; ++i)
    for (unsigned int j=0; j<system_rhs.block(i).size(); ++j)
      error += std::fabs (solution.block(i)(j)-vec2[i](j));
  double relative = solution.block(0).l1_norm();
  deallog << "  Verification fe degree " << fe_degree  <<  ": "
          << error/relative << std::endl << std::endl;
}



int main ()
{
  deallog.attach(logfile);
  deallog.depth_console(0);

  deallog << std::setprecision (3);

  {
    deallog << std::endl << "Test with doubles" << std::endl << std::endl;
    deallog.threshold_double(1.e-12);
    deallog.push("2d");
    test<2,1>();
    test<2,2>();
    deallog.pop();
    deallog.push("3d");
    test<3,1>();
    test<3,2>();
    deallog.pop();
  }
}
