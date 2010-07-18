#!/usr/bin/python
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Library General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# gen_callbacks.py
# Copyright (C) 2010 Simon Newton


import textwrap


def Header():
  print textwrap.dedent("""\
  /*
   *  This program is free software; you can redistribute it and/or modify
   *  it under the terms of the GNU General Public License as published by
   *  the Free Software Foundation; either version 2 of the License, or
   *  (at your option) any later version.
   *
   *  This program is distributed in the hope that it will be useful,
   *  but WITHOUT ANY WARRANTY; without even the implied warranty of
   *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   *  GNU Library General Public License for more details.
   *
   *  You should have received a copy of the GNU General Public License
   *  along with this program; if not, write to the Free Software
   *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   *
   * Callback.h
   * Callback classes, these are similar to closures but can take arguments at
   * exec time.
   * Copyright (C) 2005-2010 Simon Newton
   *
   * THIS FILE IS AUTOGENERATED!
   * Please run edit & run gen_callbacks.py if you need to add more types.
   */

  #ifndef INCLUDE_OLA_CALLBACK_H_
  #define INCLUDE_OLA_CALLBACK_H_

  namespace ola {
  """)

def Footer():
  print textwrap.dedent("""\
  }  // ola
  #endif  // INCLUDE_OLA_CALLBACK_H_""")

def GenerateBase(number_of_args):
  """Generate the base Callback classes."""
  typenames = ', '.join('typename Arg%d' % i for i in xrange(number_of_args))
  arg_list = ', '.join('Arg%d arg%d' % (i, i) for i in xrange(number_of_args))
  args = ', '.join('arg%d' % i for i in xrange(number_of_args))
  arg_types = ', '.join('Arg%d' % i for i in xrange(number_of_args))

  # generate the base callback class
  print '// %d argument callbacks' % number_of_args
  print 'template <typename ReturnType, %s>' % typenames
  print 'class BaseCallback%d {' % number_of_args
  print '  public:'
  print '    virtual ~BaseCallback%d() {}' % number_of_args
  print '    virtual ReturnType Run(%s) = 0;' % arg_list
  print '    virtual ReturnType DoRun(%s) = 0;' % arg_list
  print '};'
  print ''
  print ''

  # generate the multi-use version of the callback
  print '// A callback, this can be called multiple times'
  print 'template <typename ReturnType, %s>' % typenames
  print ('class Callback%d: public BaseCallback%d<ReturnType, %s> {' %
         (number_of_args, number_of_args, arg_types))
  print '  public:'
  print '    virtual ~Callback%d() {}' % number_of_args
  print ('    ReturnType Run(%s) { return DoRun(%s); }' %
         (arg_list, args))
  print '};'
  print ''
  print ''

  # generate the single-use version of the callback
  print "// A single use callback, this deletes itself after it's run."
  print 'template <typename ReturnType, %s>' % typenames
  print ('class SingleUseCallback%d: public BaseCallback%d<ReturnType, %s> {' %
         (number_of_args, number_of_args, arg_types))
  print '  public:'
  print '    virtual ~SingleUseCallback%d() {}' % number_of_args
  print '    ReturnType Run(%s) {' % arg_list
  print '      ReturnType ret = DoRun(%s);' % args
  print '      delete this;'
  print '      return ret;'
  print '    }'
  print '};'
  print ''
  print ''

  # the void specialization
  print "// A single use callback returning void."
  print 'template <%s>' % typenames
  print ('class SingleUseCallback%d<void, %s>: public BaseCallback%d<void, %s> {' %
         (number_of_args, arg_types, number_of_args, arg_types))
  print '  public:'
  print '    virtual ~SingleUseCallback%d() {}' % number_of_args
  print '    void Run(%s) {' % arg_list
  print '      DoRun(%s);' % args
  print '      delete this;'
  print '    }'
  print '};'
  print ''
  print ''

def GenerateHelperFunction(bind_count, exec_count, function_name, parent_class):
  """Generate the helper functions which create callbacks."""
  typenames = (['typename A%d' % i for i in xrange(bind_count)] +
               ['typename Arg%d' % i for i in xrange(exec_count)])
  bind_types = ['A%d' % i for i in xrange(bind_count)]
  exec_types = ['Arg%d' % i for i in xrange(exec_count)]
  method_types = ', '.join(bind_types + exec_types)


  # The single use helper function
  print '// Helper method to create a new %s.' % parent_class
  print ('template <typename Class, typename ReturnType, %s>' %
         ', '.join(typenames))
  print ('inline %s%d<ReturnType, %s>* %s(' %
         (parent_class, exec_count, ', '.join(exec_types), function_name))
  print '    Class* object,'
  if bind_count:
    print '    ReturnType (Class::*method)(%s),' % method_types
    for i in xrange(bind_count):
      suffix = ','
      if i == bind_count - 1:
        suffix = ') {'
      print '    A%d a%d%s' % (i, i, suffix)
  else:
    print '    ReturnType (Class::*method)(%s)) {' % method_types
  print '  return new MethodCallback%d_%d<Class,' % (bind_count, exec_count)
  print ('                               %s%d<ReturnType, %s>,'
         % (parent_class, exec_count, ', '.join(exec_types)))
  print '                               ReturnType,'
  for i in xrange(bind_count):
    print '                               A%d,' % i
  for i in xrange(exec_count):
    suffix = ','
    if i == exec_count - 1:
      suffix = '>('
    print '                               Arg%d%s' % (i, suffix)
  print '      object,'
  if bind_count:
    print '      method,'
  else:
    print '      method);'
  for i in xrange(bind_count):
    suffix = ','
    if i == bind_count - 1:
      suffix = ');'
    print '      a%d%s' % (i, suffix)
  print '}'
  print ''


def GenerateCallback(bind_count, exec_count):
  """Generate the specific Callback classes & helper methods."""
  typenames = (['typename A%d' % i for i in xrange(bind_count)] +
               ['typename Arg%d' % i for i in xrange(exec_count)])

  bind_types = ['A%d' % i for i in xrange(bind_count)]
  exec_types = ['Arg%d' % i for i in xrange(exec_count)]

  method_types = ', '.join(bind_types + exec_types)
  method_args = (['m_a%d' % i for i in xrange(bind_count)] +
                  ['arg%d' % i for i in xrange(exec_count)])

  exec_args = ', '.join(['Arg%d arg%d' % (i, i) for i in xrange(exec_count)])
  bind_args = ', '.join(['A%d a%d' % (i, i) for i in xrange(bind_count)])

  print ('// An method callback with %d create-time args, and %d exec time arg' %
         (bind_count, exec_count))
  print ('template <typename Class, typename Parent, typename ReturnType, %s>' %
         ', '.join(typenames))
  print 'class MethodCallback%d_%d: public Parent {' % (bind_count, exec_count)
  print '  public:'
  print '    typedef ReturnType (Class::*Method)(%s);' % method_types
  if bind_count:
    print ('    MethodCallback%d_%d(Class *object, Method callback, %s):' %
           (bind_count, exec_count, bind_args))
  else:
    print ('    MethodCallback%d_%d(Class *object, Method callback):' %
           (bind_count, exec_count))
  print '      Parent(),'
  print '      m_object(object),'
  if bind_count:
    print '      m_callback(callback),'
    for i in xrange(bind_count):
      suffix = ','
      if i == bind_count - 1:
        suffix = ' {}'
      print '      m_a%d(a%d)%s' % (i, i, suffix)
  else:
    print '      m_callback(callback) {}'
  print '    ReturnType DoRun(%s) {' % exec_args
  print '      return (m_object->*m_callback)(%s);' % ', '.join(method_args)
  print '    }'

  print '  private:'
  print '    Class *m_object;'
  print '    Method m_callback;'
  for i in xrange(bind_count):
    print '  A%d m_a%d;' % (i, i)
  print '};'
  print ''

  # generate the helper methods
  GenerateHelperFunction(bind_count, exec_count, 'NewSingleCallback', 'SingleUseCallback')
  GenerateHelperFunction(bind_count, exec_count, 'NewCallback', 'Callback')


def main():
  Header()

  # exec_time : [bind time args]
  calback_types = {1: [0, 1, 2, 3],
                   2: [0, 1],
                   3: [0, 1],
                   4: [0, 1],
                  }

  for exec_time in sorted(calback_types):
    GenerateBase(exec_time)
    for bind_time in calback_types[exec_time]:
      GenerateCallback(bind_time, exec_time)
  Footer()


main()