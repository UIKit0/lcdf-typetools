#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef __GNUG__
# pragma implementation "psres.hh"
#endif
#include "psres.hh"
#include "slurper.hh"
/* Get the correct functions for directory searching */ 
#if HAVE_DIRENT_H
# include <dirent.h>
# define DIR_NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define DIR_NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif


static String empty_string;


PsresDatabase::PsresDatabase()
  : _section_map(0)
{
  _sections.append(0);
}

PsresDatabase::~PsresDatabase()
{
  for (int i = 1; i < _sections.count(); i++)
    delete _sections[i];
}


PsresDatabaseSection::PsresDatabaseSection(PermString section_name)
  : _section_name(section_name), _map(0)
{
  _directories.append(PermString());
  _values.append(String());
  _value_escaped.append(true);
}


/* read_ps_line - gathers a line in `slurper', returns true if it is a normal
   line, false on end of file or a separator line (starts with `.'). Handles
   continuation lines but not backslash escaping. Stores location of first
   non-backslashed equals sign in `*equals_pos', or -1 if none. */

static bool
read_psres_line(Slurper &slurper, int *equals_pos)
{
  if (equals_pos) *equals_pos = -1;
  
  char *s = slurper.next_line();
  if (!s) return false;
  
  bool is_terminator = s[0] == '.';
  unsigned len, pos = 0;
  bool found_eq = false;
  
  while (true) {
    len = slurper.cur_line_length();
    if (len == 0) break;
    
    // process backslash escapes
    for (; pos < len - 1; pos++) {
      if (s[pos] == '\\') {
	// quote the next character
	pos++;
      } else if (!found_eq && s[pos] == '=') {
	// an equals sign: store its position
	if (equals_pos) *equals_pos = pos;
	found_eq = true;
      } else if (s[pos] == '%') {
	// unescaped '%' is a comment; return immediately
	s[pos] = 0;
	goto done;
      }
    }
    
    // stop processing if not a continuation line
    if (pos == len || s[pos] != '\\') break;
    
    s = slurper.append_line(s + pos);
  }
  
  if (pos < len && !found_eq && s[pos] == '=' && equals_pos)
    *equals_pos = pos;
  
 done:
  return !is_terminator;
}

/* psres_escape: backslash escaping */

static unsigned
psres_escape(char *s, unsigned len)
{
  unsigned pos = 0, delta = 0;
  while (pos < len) {
    if (s[pos] == '\\') pos++, delta++;
    if (delta) s[pos-delta] = s[pos];
    pos++;
  }
  s[pos-delta] = 0;
  return len-delta;
}

void
PsresDatabaseSection::add_psres_file_section
	(Slurper &slurper, PermString directory, bool override)
{
  int equals_pos;
  bool first_line = true;
  
  while (read_psres_line(slurper, &equals_pos)) {
    char *s = slurper.cur_line();
    
    // check for a directory line
    if (first_line) {
      first_line = false;
      if (s[0] == '/') {
	psres_escape(s + 1, slurper.cur_line_length() - 1);
	directory = PermString(s + 1);
	continue;
      }
    }
    
    if (equals_pos < 0) {
      // report error?
      continue;
    }
    
    // get the key
    unsigned len = psres_escape(s, equals_pos);
    PermString key = PermString(s, len);
    int index = _map[key];
    if (!override && index > 0)
      continue;
    
    // double equals means absolute pathname
    PermString this_directory = directory;
    if (s[equals_pos + 1] == '=') {
      equals_pos++;
      this_directory = PermString();
    }
    
    // get the value. Don't escape it yet
    len = slurper.cur_line_length() - (equals_pos + 1);
    String value = String(s + equals_pos + 1, len);
    
    // stick key and value into our data structure
    if (index == 0) {
      index = _directories.append(directory);
      _values.append(value);
      _value_escaped.append(false);
      _map.insert(key, index);
    } else {
      _directories[index] = directory;
      _values[index] = value;
      _value_escaped[index] = false;
    }
  }
}


PsresDatabaseSection *
PsresDatabase::force_section(PermString name)
{
  if (_section_map[name] > 0)
    return _sections[_section_map[name]];
  else {
    PsresDatabaseSection *s = new PsresDatabaseSection(name);
    int index = _sections.append(s);
    _section_map.insert(name, index);
    return s;
  }
}

bool
PsresDatabase::add_one_psres_file(Slurper &slurper, bool override)
{
  if (!read_psres_line(slurper, 0))
    return /* error */ false;
  
  char *s = slurper.cur_line();
  unsigned len = slurper.cur_line_length();
  if (len < 12 || memcmp(s, "PS-Resources", 12) != 0)
    return /* error */ false;
  
  bool exclusive = (len >= 22 && memcmp(s+12, "-Exclusive", 10) == 0);
  
  // skip list of sections
  while (read_psres_line(slurper, 0))
    /* nada */;
  
  // now, read each section
  PermString directory = slurper.filename().directory();
  
  while (read_psres_line(slurper, 0)) {
    s = slurper.cur_line();
    len = psres_escape(s, slurper.cur_line_length());
    PsresDatabaseSection *section = force_section(PermString(s, len));
    section->add_psres_file_section(slurper, directory, override);
  }
  
  return exclusive;
}

bool
PsresDatabase::add_psres_file(Filename &filename, bool override)
{
  Slurper slurpy(filename);
  return add_one_psres_file(slurpy, override);
}

void
PsresDatabase::add_psres_directory(PermString directory)
{
  DIR *dir = opendir(directory.cc());
  if (!dir) return;
  
  while (struct dirent *dirent = readdir(dir)) {
    int len = DIR_NAMLEN(dirent);
    if (len > 4 && dirent->d_name[0] != '.'
	&& memcmp(dirent->d_name + len - 4, ".upr", 4) == 0
	&& (len != 9 || memcmp(dirent->d_name, "PSres.upr", 9) != 0)) {
      Filename fn(directory, PermString(dirent->d_name, len));
      add_psres_file(fn, false);
    }
  }
  
  closedir(dir);
}

void
PsresDatabase::add_psres_path(const char *path, const char *default_path,
			      bool override)
{
  if (!path && !default_path)
    return;
  else if (!path) {
    path = default_path;
    default_path = 0;
  }
  
  if (override && _sections.count() > 1) {
    PsresDatabase new_db;
    new_db.add_psres_path(path, default_path, false);
    add_database(&new_db, true);
    return;
  }
  
  while (*path) {
    const char *epath = path;
    while (*epath && *epath != ':') epath++;
    
    PermString directory(path, epath - path);
    Filename filename(directory, "PSres.upr");
    if (epath == path) {
      add_psres_path(default_path, 0, false);
      default_path = 0;	// don't use default path twice
    } else if (!filename.readable() || !add_psres_file(filename, false))
      add_psres_directory(directory);
    
    path = (*epath ? epath+1 : epath);
  }
}


void
PsresDatabase::add_database(PsresDatabase *db, bool override)
{
  for (int i = 1; i < db->_sections.count(); i++) {
    PermString section_name = db->_sections[i]->section_name();
    PsresDatabaseSection *section = force_section(section_name);
    section->add_section(db->_sections[i], override);
  }
}

void
PsresDatabaseSection::add_section(PsresDatabaseSection *s, bool override)
{
  int eacher = 0;
  PermString key;
  int index;
  while (s->_map.each(eacher, key, index)) {
    if (_map[key] > 0) {
      if (!override) continue;
      int my_index = _map[key];
      _directories[my_index] = s->_directories[index];
      _values[my_index] = s->_values[index];
      _value_escaped[my_index] = s->_value_escaped[index];
    } else {
      int my_index = _directories.append(s->_directories[index]);
      _values.append(s->_values[index]);
      _value_escaped.append(s->_value_escaped[index]);
      _map.insert(key, my_index);
    }
  }
}


const String &
PsresDatabaseSection::value(int index) const
{
  if (_value_escaped[index])
    return _values[index];
  else {
    char *data = _values[index].mutable_data();
    int len = psres_escape(data, _values[index].length());
    _values[index] = _values[index].substring(0, len);
    _value_escaped[index] = true;
    return _values[index];
  }
}

Filename
PsresDatabaseSection::filename_value(PermString key) const
{
  int index = _map[key];
  if (!index)
    return Filename();
  else if (!_directories[index])
    return Filename(value(index));
  else
    return Filename(_directories[index], value(index));
}

const String &
PsresDatabase::value(PermString sec, PermString key) const
{
  PsresDatabaseSection *s = section(sec);
  if (s)
    return s->value(key);
  else
    return empty_string;
}

const String &
PsresDatabase::unescaped_value(PermString sec, PermString key) const
{
  PsresDatabaseSection *s = section(sec);
  if (s)
    return s->unescaped_value(key);
  else
    return empty_string;
}

Filename
PsresDatabase::filename_value(PermString sec, PermString key) const
{
  PsresDatabaseSection *s = section(sec);
  if (s)
    return s->filename_value(key);
  else
    return Filename();
}
