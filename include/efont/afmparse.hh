#ifndef AFMPARSE_HH
#define AFMPARSE_HH
#ifdef __GNUG__
#pragma interface
#endif
#include "slurper.hh"
#include "permstr.hh"
#include <stdarg.h>

class AfmParser {
  
  Slurper &_slurper;
  bool _save_line;
  
  unsigned char *_line;
  unsigned char *_pos;
  int _length;
  
  PermString _message;
  int _fail_field;
  
  void static_initialize();
  void trim_end();
  unsigned char *vis(const char *, va_list);
  
 public:
  
  AfmParser(Slurper &);
  
  bool ok() const			{ return _slurper.ok(); }
  operator Landmark() const		{ return _slurper.landmark(); }
  Landmark landmark() const		{ return _slurper.landmark(); }
  unsigned lineno() const		{ return _slurper.lineno(); }
  const Filename &filename() const	{ return _slurper.filename(); }
  
  static void set_ends_names(unsigned char, bool);
  
  bool key_matched() const		{ return _fail_field >= 0; }
  int fail_field() const		{ return _fail_field; }
  PermString message() const		{ return _message; }
  void clear_message()			{ _message = PermString(); }
  
  PermString keyword() const;
  bool is(const char *, ...);
  bool isall(const char *, ...);
  
  bool next_line();
  void save_line()			{ _slurper.save_line(); }
  void skip_until(unsigned char);
  
  unsigned char first() const		{ return _pos[0]; }
  unsigned char operator[](int i) const	{ return _pos[i]; }
  bool left() const			{ return *_pos != 0; }
  
};


inline bool
AfmParser::next_line()
{
  _pos = _line = (unsigned char *)_slurper.next_line();
  _length = _slurper.length();
  return _line != 0;
}

#endif