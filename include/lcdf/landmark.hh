#ifndef LANDMARK_HH
#define LANDMARK_HH
#ifdef __GNUG__
#pragma interface
#endif
#include "permstr.hh"
#include <stdio.h>

class Landmark {
  
  PermString _file;
  unsigned _line;
  
 public:
  
  Landmark()				: _file(0), _line(~0U) { }
  Landmark(PermString f, unsigned l)	: _file(f), _line(l) { }
  
  operator bool() const			{ return _file; }
  bool has_line() const			{ return _line != ~0U; }
  
  PermString file() const		{ return _file; }
  unsigned line() const			{ return _line; }
  
  Landmark next_line() const		{ return Landmark(_file, _line + 1); }
  
  void print(FILE *) const;
  
};

#endif
