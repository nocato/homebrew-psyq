/* GNU Objective C Runtime initialization 
   Copyright (C) 1993, 1995, 1996, 1997 Free Software Foundation, Inc.
   Contributed by Kresten Krab Thorup

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2, or (at your option) any later version.

GNU CC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along with
GNU CC; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* As a special exception, if you link this library with files compiled with
   GCC to produce an executable, this does not cause the resulting executable
   to be covered by the GNU General Public License. This exception does not
   however invalidate any other reasons why the executable file might be
   covered by the GNU General Public License.  */

#include "runtime.h"

/* The version number of this runtime.  This must match the number 
   defined in gcc (objc-act.c) */
#define OBJC_VERSION 8
#define PROTOCOL_VERSION 2

/* This list contains all modules currently loaded into the runtime */
static struct objc_list* __objc_module_list = 0; 	/* !T:MUTEX */

/* This list contains all proto_list's not yet assigned class links */
static struct objc_list* unclaimed_proto_list = 0; 	/* !T:MUTEX */

/* List of unresolved static instances.  */
static struct objc_list *uninitialized_statics = 0; 	/* !T:MUTEX */

/* Global runtime "write" mutex. */
objc_mutex_t __objc_runtime_mutex;

/* Number of threads that are alive. */
int __objc_runtime_threads_alive = 1;			/* !T:MUTEX */

/* Check compiler vs runtime version */
static void init_check_module_version (Module_t);

/* Assign isa links to protos */
static void __objc_init_protocols (struct objc_protocol_list* protos);

/* Add protocol to class */
static void __objc_class_add_protocols (Class, struct objc_protocol_list*);

/* This is a hook which is called by __objc_exec_class every time a class
   or a category is loaded into the runtime.  This may e.g. help a
   dynamic loader determine the classes that have been loaded when
   an object file is dynamically linked in */
void (*_objc_load_callback)(Class class, Category* category) = 0; /* !T:SAFE */

/* Is all categories/classes resolved? */
BOOL __objc_dangling_categories = NO;           /* !T:UNUSED */

extern SEL
__sel_register_typed_name (const char *name, const char *types, 
			   struct objc_selector *orig, BOOL is_const);

/* Send +load to all classes and categories from a module that implement
   this method */
static void __objc_send_load(Module_t module);

/* This list contains all the classes in the runtime system for whom their
   superclasses are not yet know to the runtime. */
static struct objc_list* unresolved_classes = 0;

/* Static function used to references the Object and NXConstantString classes. */
static void
__objc_force_linking (void)
{
  extern void __objc_linking (void);
  __objc_linking ();

  /* Call the function to avoid compiler warning */
  __objc_force_linking ();
}

/* Run through the statics list, removing modules as soon as all its statics
   have been initialized.  */
static void
objc_init_statics ()
{
  struct objc_list **cell = &uninitialized_statics;
  struct objc_static_instances **statics_in_module;

  objc_mutex_lock(__objc_runtime_mutex);

  while (*cell)
    {
      int module_initialized = 1;

      for (statics_in_module = (*cell)->head;
	   *statics_in_module; statics_in_module++)
	{
	  struct objc_static_instances *statics = *statics_in_module;
	  Class class = objc_lookup_class (statics->class_name);

	  if (!class)
	    module_initialized = 0;
	  /* Actually, the static's class_pointer will be NULL when we
             haven't been here before.  However, the comparison is to be
             reminded of taking into account class posing and to think about
             possible semantics...  */
	  else if (class != statics->instances[0]->class_pointer)
	    {
	      id *inst;

	      for (inst = &statics->instances[0]; *inst; inst++)
		{
		  (*inst)->class_pointer = class;

		  /* ??? Make sure the object will not be freed.  With
                     refcounting, invoke `-retain'.  Without refcounting, do
                     nothing and hope that `-free' will never be invoked.  */

		  /* ??? Send the object an `-initStatic' or something to
                     that effect now or later on?  What are the semantics of
                     statically allocated instances, besides the trivial
                     NXConstantString, anyway?  */
		}
	    }
	}
      if (module_initialized)
	{
	  /* Remove this module from the uninitialized list.  */
	  struct objc_list *this = *cell;
	  *cell = this->tail;
	  objc_free(this);
	}
      else
	cell = &(*cell)->tail;
    }

  objc_mutex_unlock(__objc_runtime_mutex);
} /* objc_init_statics */

/* This function is called by constructor functions generated for each
   module compiled.  (_GLOBAL_$I$...) The purpose of this function is to
   gather the module pointers so that they may be processed by the
   initialization routines as soon as possible */

void
__objc_exec_class (Module_t module)
{
  /* Have we processed any constructors previously?  This flag is used to
     indicate that some global data structures need to be built.  */
  static BOOL previous_constructors = 0;

  static struct objc_list* unclaimed_categories = 0;

  /* The symbol table (defined in objc-api.h) generated by gcc */
  Symtab_t symtab = module->symtab;

  /* The statics in this module */
  struct objc_static_instances **statics
    = symtab->defs[symtab->cls_def_cnt + symtab->cat_def_cnt];

  /* Entry used to traverse hash lists */
  struct objc_list** cell;

  /* The table of selector references for this module */
  SEL selectors = symtab->refs; 

  /* dummy counter */
  int i;

  DEBUG_PRINTF ("received module: %s\n", module->name);

  /* check gcc version */
  init_check_module_version(module);

  /* On the first call of this routine, initialize some data structures.  */
  if (!previous_constructors)
    {
	/* Initialize thread-safe system */
      __objc_init_thread_system();
      __objc_runtime_threads_alive = 1;
      __objc_runtime_mutex = objc_mutex_allocate();

      __objc_init_selector_tables();
      __objc_init_class_tables();
      __objc_init_dispatch_tables();
      previous_constructors = 1;
    }

  /* Save the module pointer for later processing. (not currently used) */
  objc_mutex_lock(__objc_runtime_mutex);
  __objc_module_list = list_cons(module, __objc_module_list);

  /* Replace referenced selectors from names to SEL's.  */
  if (selectors)
    {
      for (i = 0; selectors[i].sel_id; ++i)
	{
	  const char *name, *type;
	  name = (char*)selectors[i].sel_id;
	  type = (char*)selectors[i].sel_types;
	  /* Constructors are constant static data so we can safely store
	     pointers to them in the runtime structures. is_const == YES */
	  __sel_register_typed_name (name, type, 
				     (struct objc_selector*)&(selectors[i]),
				     YES);
	}
    }

  /* Parse the classes in the load module and gather selector information.  */
  DEBUG_PRINTF ("gathering selectors from module: %s\n", module->name);
  for (i = 0; i < symtab->cls_def_cnt; ++i)
    {
      Class class = (Class) symtab->defs[i];
      const char* superclass = (char*)class->super_class;

      /* Make sure we have what we think.  */
      assert (CLS_ISCLASS(class));
      assert (CLS_ISMETA(class->class_pointer));
      DEBUG_PRINTF ("phase 1, processing class: %s\n", class->name);

      /* Store the class in the class table and assign class numbers.  */
      __objc_add_class_to_hash (class);

      /* Register all of the selectors in the class and meta class.  */
      __objc_register_selectors_from_class (class);
      __objc_register_selectors_from_class ((Class) class->class_pointer);

      /* Install the fake dispatch tables */
      __objc_install_premature_dtable(class);
      __objc_install_premature_dtable(class->class_pointer);

      if (class->protocols)
	__objc_init_protocols (class->protocols);

      /* Check to see if the superclass is known in this point. If it's not
	 add the class to the unresolved_classes list. */
      if (superclass && !objc_lookup_class (superclass))
	unresolved_classes = list_cons (class, unresolved_classes);
   }

  /* Process category information from the module.  */
  for (i = 0; i < symtab->cat_def_cnt; ++i)
    {
      Category_t category = symtab->defs[i + symtab->cls_def_cnt];
      Class class = objc_lookup_class (category->class_name);
      
      /* If the class for the category exists then append its methods.  */
      if (class)
	{

	  DEBUG_PRINTF ("processing categories from (module,object): %s, %s\n",
			module->name,
			class->name);

	  /* Do instance methods.  */
	  if (category->instance_methods)
	    class_add_method_list (class, category->instance_methods);

	  /* Do class methods.  */
	  if (category->class_methods)
	    class_add_method_list ((Class) class->class_pointer, 
				   category->class_methods);

	  if (category->protocols)
	    {
	      __objc_init_protocols (category->protocols);
	      __objc_class_add_protocols (class, category->protocols);
	    }

          if (_objc_load_callback)
	    _objc_load_callback(class, category);
	}
      else
	{
	  /* The object to which the category methods belong can't be found.
	     Save the information.  */
	  unclaimed_categories = list_cons(category, unclaimed_categories);
	}
    }

  if (statics)
    uninitialized_statics = list_cons (statics, uninitialized_statics);
  if (uninitialized_statics)
    objc_init_statics ();

  /* Scan the unclaimed category hash.  Attempt to attach any unclaimed
     categories to objects.  */
  for (cell = &unclaimed_categories;
       *cell;
       ({ if (*cell) cell = &(*cell)->tail; }))
    {
      Category_t category = (*cell)->head;
      Class class = objc_lookup_class (category->class_name);
      
      if (class)
	{
	  DEBUG_PRINTF ("attaching stored categories to object: %s\n",
			class->name);
	  
	  list_remove_head (cell);
	  
	  if (category->instance_methods)
	    class_add_method_list (class, category->instance_methods);
	  
	  if (category->class_methods)
	    class_add_method_list ((Class) class->class_pointer,
				   category->class_methods);
	  
	  if (category->protocols)
	    {
	      __objc_init_protocols (category->protocols);
	      __objc_class_add_protocols (class, category->protocols);
	    }
	  
          if (_objc_load_callback)
	    _objc_load_callback(class, category);
	}
    }
  
  if (unclaimed_proto_list && objc_lookup_class ("Protocol"))
    {
      list_mapcar (unclaimed_proto_list,(void(*)(void*))__objc_init_protocols);
      list_free (unclaimed_proto_list);
      unclaimed_proto_list = 0;
    }

  objc_send_load ();

  objc_mutex_unlock(__objc_runtime_mutex);
}

void objc_send_load (void)
{
  if (!__objc_module_list)
    return;
 
  /* Try to find out if all the classes loaded so far also have their
     superclasses known to the runtime. We suppose that the objects that are
     allocated in the +load method are in general of a class declared in the
     same module. */
  if (unresolved_classes)
    {
      Class class = unresolved_classes->head;

      while (objc_lookup_class ((char*)class->super_class))
	{
	  list_remove_head (&unresolved_classes);
	  if (unresolved_classes)
	    class = unresolved_classes->head;
	  else
	    break;
	}

      /*
       * If we still have classes for which we don't have yet their super
       * classes known to the runtime we don't send the +load messages.
       */
      if (unresolved_classes)
	return;
    }

  /* Special check to allow sending messages to constant strings in +load
     methods. If the class is not yet known, even if all the classes are known,
     delay sending of +load. */
  if (!objc_lookup_class ("NXConstantString"))
    return;

  /* Iterate over all modules in the __objc_module_list and call on them the
     __objc_send_load function that sends the +load message. */
  list_mapcar (__objc_module_list, (void(*)(void*))__objc_send_load);
  list_free (__objc_module_list);
  __objc_module_list = NULL;
}

static void
__objc_send_message_in_list (MethodList_t method_list, id object, SEL op)
{
  while (method_list)
    {
      int i;

      /* Search the method list. */
      for (i = 0; i < method_list->method_count; i++)
	{
	  Method_t mth = &method_list->method_list[i];

	  if (mth->method_name && sel_eq (mth->method_name, op))
	    {
	      /* The method was found. */
	      (*mth->method_imp) (object, mth->method_name);
	      break;
	    }
	}
      method_list = method_list->method_next;
    }
}

static void
__objc_send_load(Module_t module)
{
  /* The runtime mutex is locked in this point */

  Symtab_t symtab = module->symtab;
  static SEL load_sel = 0;
  int i;

  if (!load_sel)
    load_sel = sel_register_name ("load");

  /* Iterate thru classes defined in this module and send them the +load
     message if they implement it. At this point all methods defined in
     categories were added to the corresponding class, so all the +load
     methods of categories are in their corresponding classes. */
  for (i = 0; i < symtab->cls_def_cnt; i++)
    {
      Class class = (Class) symtab->defs[i];
      MethodList_t method_list = class->class_pointer->methods;

      __objc_send_message_in_list (method_list, (id)class, load_sel);

      /* Call the _objc_load_callback for this class. */
      if (_objc_load_callback)
	_objc_load_callback(class, 0);
    }

  /* Call the _objc_load_callback for categories. Don't register the instance
     methods as class methods for categories to root classes since they were
     already added in the class. */
  for (i = 0; i < symtab->cat_def_cnt; i++)
    {
      Category_t category = symtab->defs[i + symtab->cls_def_cnt];
      Class class = objc_lookup_class (category->class_name);
      
      if (_objc_load_callback)
	_objc_load_callback(class, category);
    }
}

/* Sanity check the version of gcc used to compile `module'*/
static void init_check_module_version(Module_t module)
{
  if ((module->version != OBJC_VERSION) || (module->size != sizeof (Module)))
    {
      int code;

      if(module->version > OBJC_VERSION)
	code = OBJC_ERR_OBJC_VERSION;
      else if (module->version < OBJC_VERSION)
	code = OBJC_ERR_GCC_VERSION;
      else
	code = OBJC_ERR_MODULE_SIZE;

      objc_error(nil, code, "Module %s version %d doesn't match runtime %d\n",
	       module->name, (int)module->version, OBJC_VERSION);
    }
}

static void
__objc_init_protocols (struct objc_protocol_list* protos)
{
  int i;
  static Class proto_class = 0;

  if (! protos)
    return;

  objc_mutex_lock(__objc_runtime_mutex);

  if (!proto_class)
    proto_class = objc_lookup_class("Protocol");

  if (!proto_class)
    {
      unclaimed_proto_list = list_cons (protos, unclaimed_proto_list);
      objc_mutex_unlock(__objc_runtime_mutex);
      return;
    }

#if 0
  assert (protos->next == 0);	/* only single ones allowed */
#endif

  for(i = 0; i < protos->count; i++)
    {
      struct objc_protocol* aProto = protos->list[i];
      if (((size_t)aProto->class_pointer) == PROTOCOL_VERSION)
	{
	  /* assign class pointer */
	  aProto->class_pointer = proto_class;

	  /* init super protocols */
	  __objc_init_protocols (aProto->protocol_list);
	}
      else if (protos->list[i]->class_pointer != proto_class)
	{
	  objc_error(nil, OBJC_ERR_PROTOCOL_VERSION,
		     "Version %d doesn't match runtime protocol version %d\n",
		     (int)((char*)protos->list[i]->class_pointer-(char*)0),
		     PROTOCOL_VERSION);
	}
    }

  objc_mutex_unlock(__objc_runtime_mutex);
}

static void __objc_class_add_protocols (Class class,
					struct objc_protocol_list* protos)
{
  /* Well... */
  if (! protos)
    return;

  /* Add it... */
  protos->next = class->protocols;
  class->protocols = protos;
}
