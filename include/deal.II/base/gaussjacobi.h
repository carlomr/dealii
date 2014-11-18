#ifndef __gaussjacobi_h__
#define __gaussjacobi_h__

#include <deal.II/base/config.h>
#include <deal.II/base/quadrature.h>

DEAL_II_NAMESPACE_OPEN

template <int dim>
class QGaussJacobi : public Quadrature<dim>
{
public:
  /**
   * Generate a formula with
   * <tt>n</tt> quadrature points (in
   * each space direction), exact for
   * polynomials of degree
   * <tt>2n-1</tt>.
   */
  QGaussJacobi (const unsigned int n, const double alf, const double bet);

};

template <> QGaussJacobi<1>::QGaussJacobi (const unsigned int n,const double alf, const double bet);

DEAL_II_NAMESPACE_CLOSE



#endif

