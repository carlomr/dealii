//----------------------------  pointer_03.cc  ---------------------------
//    $Id$
//    Version: $Name$
//
//    Copyright (C) 2010, 2011 by the deal.II authors
//
//    This file is subject to QPL and may not be  distributed
//    without copyright and license information. Please refer
//    to the file deal.II/doc/license.html for the  text  and
//    further information on this license.
//
//----------------------------  pointer_03.cc  ---------------------------

// test what happens when serializing two objects that have pointers to the
// same common object. the two objects here are the members of a std::pair

#include "serialization.h"

#include <boost/serialization/utility.hpp>
#include <typeinfo>

int object_number = 1;

class C
{
  public:
    C ()
      {
	object_number = ::object_number++;
	deallog << "Default constructor. Object number "
		<< object_number
		<< std::endl;
      }

    C (const C&)
      {
	object_number = ::object_number++;
	deallog << "copy constructor. Object number "
		<< object_number
		<< std::endl;
      }

    ~C ()
      {
	deallog << "destructor. Object number "
		<< object_number
		<< std::endl;
      }
    
    template <typename Archive>
    void serialize (Archive &ar, const unsigned int version)
      {
	deallog << "Serializing object number "
		<< object_number
		<< " via " << typeid(Archive).name()
		<< std::endl;
      }

    bool operator == (const C &) const
      {
	return true;
      }

  private:
    unsigned int object_number;
};


template <typename T>
bool compare (const std::pair<T*,T*> &t1,
	      const std::pair<T*,T*> &t2)
{
  return (*t1.first == *t2.first) && (*t1.second == *t2.second);
}


void test ()
{
  {
    C *p = new C();
    std::pair<C*,C*> pair_1(p, p);
    std::pair<C*,C*> pair_2;
  
    verify (pair_1, pair_2);

				     // boost::serialize should have
				     // recognized that the two pointers in
				     // pair_1 point to the same object and
				     // consequently re-create only one object
				     // that the two components of the
				     // re-created pair point to
    Assert (pair_2.first == pair_2.second, ExcInternalError());
    Assert (object_number == 3, ExcInternalError());

				     // destroy the newly created object. this
				     // must succeed
    delete pair_2.first;
    
    delete p;
  }
}


int main ()
{
  std::ofstream logfile("pointer_03/output");
  deallog << std::setprecision(3);
  deallog.attach(logfile);
  deallog.depth_console(0);
  deallog.threshold_double(1.e-10);

  test ();

  deallog << "OK" << std::endl;
}