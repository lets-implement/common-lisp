
# `int main()`

We're about to embark on a journey that will take us through
[Gehennom][] and hopefully back too, but as they say, a
program of a thousand k-locs starts with an `int main()`:

    int main() {
        // code here
    }
    
There we go, that wasn't too bad. I think we have a bright road ahead
of us.

## Build system

With all that code down, I think we're going to need a nice build
system. Let's use CMake, because it's pretty cross-platform and it's
much cleaner than Make. I just googled a tutorial and copied from
[this guy](cmake-intro) to get this and I put it in `CMakeLists.txt`:

    cmake_minimum_required(VERSION 2.6)
    project(picl)
    
    file(GLOB SOURCES "core/*.c++")
     
    add_definitions(-std=c++11)
    include_directories(core/include)
    
    add_executable(picl ${SOURCES})

Let's try it out:

    ~/code/lets-implement/common-lisp/code $ ls
    CMakeLists.txt  CMakeLists.txt~  core
    ~/code/lets-implement/common-lisp/code $ mkdir build
    ~/code/lets-implement/common-lisp/code $ cd build
    ~/code/lets-implement/common-lisp/code/build $ cmake ..
    -- The C compiler identification is GNU 5.2.1
    -- The CXX compiler identification is GNU 5.2.1
    -- Check for working C compiler: /usr/bin/cc
    -- Check for working C compiler: /usr/bin/cc -- works
    -- Detecting C compiler ABI info
    -- Detecting C compiler ABI info - done
    -- Detecting C compile features
    -- Detecting C compile features - done
    -- Check for working CXX compiler: /usr/bin/c++
    -- Check for working CXX compiler: /usr/bin/c++ -- works
    -- Detecting CXX compiler ABI info
    -- Detecting CXX compiler ABI info - done
    -- Detecting CXX compile features
    -- Detecting CXX compile features - done
    -- Configuring done
    -- Generating done
    -- Build files have been written to: ~/code/lets-implement/common-lisp/code/build
    ~/code/lets-implement/common-lisp/code/build $ ls
    CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake
    ~/code/lets-implement/common-lisp/code/build $ make
    Scanning dependencies of target picl
    [ 50%] Building CXX object CMakeFiles/picl.dir/core/main.c++.o
    [100%] Linking CXX executable picl
    [100%] Built target picl
    ~/code/lets-implement/common-lisp/code/build $ ls
    CMakeCache.txt  CMakeFiles  Makefile  cmake_install.cmake  picl
    ~/code/lets-implement/common-lisp/code/build $ ./picl
    ~/code/lets-implement/common-lisp/code/build $ echo $?
    0
    ~/code/lets-implement/common-lisp/code/build $ 
    
Noice!

You'll notice that I've started to put the C++ code in `code/core/`. I
think we'll call the *core* the part written in C++, and then some of
the implementation will be written in Common Lisp itself.

Also, I sneakily named it: "PICL." This stands for "Pretty Intense
Common Lisp" or alternatively "Portable Interpreted Common Lisp,"
which I think you'll agree are both pretty meccha-kucha names, and are
in no way backronyms. MKCL was already taken.

## Objects

Getting back to our source code, I guess we're going to need
some... *things*. Objects. Yeah. So why don't we start by building our
object hierarchy. We'll make a new header called `picl/object.h++` in
the `include` directory and put this in it:

    #ifndef OBJECT_HPP
    #define OBJECT_HPP
    
    namespace picl {
        enum lisp_type {
            NIL_T,
            CONS_T,
            FIXNUM_T,
            SYMBOL_T
        };
    
        struct gc_info {
            // stuff
        };
    
        class lisp_object {
        public:
            lisp_object(lisp_type t) : type(t) { }
            
            const lisp_type type;
            gc_info gc_use;
        };
    }
    
    #endif

We've got our standard include guards, namespace, and the class
`lisp_object` which will be the base class for everything that happens
in our lisp. We need to be able to identify the type of these objects
at runtime, so I've got the `lisp_type` enum in there, and `gc_info`
for when we work on the GC.

Actually, forget that. We don't want to have to maintain a central
list of object IDs, we want to be able to create one when we need it
without modifying `object.h++`. We *could* implement a compile-time
string hash and base the object ID on a combination of the filename
and line number (obtainable with the `__FILE__` and `__LINE__`
preprocessor macros) at which the object ID declaration appears, and
that would be cool because then all the object IDs would be
compile-time constants. Even though the code for that is quite simple
with `constexpr` and I *really* want to do it (*note from the future:
I did in fact do it, but I reverted the change*), the implementation
would be distracting and the performance gained wouldn't be sufficient
to justify preempting our mandate of simplicity. Therefore we'll just
make a static function on `lisp_object` that keeps a counter. (This
also absolves us of having to think about the unlikely event of a hash
collision.)

Let's add that to `lisp_object`:

        static lisp_type_t get_new_object_id(std::string classname) {
            object_id_to_name.push_back(std::move(classname));
            return object_id_to_name.size() - 1;
        }

    private:
        static std::vector<std::string> object_id_to_name;

As a bonus, we'll keep a record of object IDs to class names in case
that comes in handy later.

This necessitates creating our first new C++ file, `object.c++`, which
will be the home of `lisp_object::object_id_to_name`:

    #include <picl/object.h++>
    
    namespace picl {
        std::vector<std::string> lisp_object::object_id_to_name;
    }

And thanks to the way we wrote our `CMakeLists.txt`, we don't even
have to modify it to get this source file to compile.

In summary, instead of having that global `enum`, we can create the
object IDs inside the classes themselves, as we'll demonstrate
later. We'll add a quick typedef for `lisp_type_t` to `lisp_object` to
be used whenever we declare a variable to hold one of these object
IDs.

Onward!

## Garbage collection: first thoughts

We actually need to start thinking about the garbage collector
immediately, because GCs can be pretty invasive and it will affect
what decisions we make when coding in the near future.

[The Boehm GC][boehm-gc] is a well-regarded conservative GC library
for C, but using it would violate the requirement about having no
external libs, so I'm going to make my own. We want to go for an easy
implementation and we don't want problems with cyclical references or
a moving collector, so I think we'll use a naïve mark-and-sweep
tracing collector. I don't want to implement my own memory allocator
either though, so we'll just use the system's `new` and `delete`
implementations. This is probably going to lead to cringe-worthy
performance, but we need to tightly focus on our stated goals of
simplicity and short development time.

Since we're going to be manipulating these pointers from C, the GC
should deallocate an object if and only if the object is not reachable
from the root set **and** the object isn't reachable from native
code. I think we should use a smart-pointer class to help us out;
it'll just increment a reference count in a `lisp_object` when we
refer to it from C, and decrement it when it goes out of scope. I'll
name it `lisp_object_handle`, and the code will be boiler-platey
enough not to be listed here. The only notable C++11 features we use
are `auto` for compile-time variable type deduction and
[move-constructors][] which should be straightforward.

We updated `gc_info` to look like this now:

    struct gc_info {
        gc_info() : crefcount(0) { }

        unsigned int crefcount;
    };
    
Let's start `gc.h++` now too. Besides housing `lisp_object_handle`,
it'll hold the `gc` class which will manage garbage collection.

    class gc {
    public:
        template<typename T>
        lisp_object_handle allocate() {
            auto new_object = new T();
            lisp_object_handle retval = &new_object;
            allocated_objects.push_back(&new_object);

            return retval;
        }

    private:
        std::list<object*> allocated_objects;
    };

A simple `allocate` function is templated on a subtype of
`lisp_object`. It merely allocates a `T` calling the default
constructor, saves a handle and then adds it to the object list. The
object list will hold a pointer to every object allocated with that
GC; then later when we implement the mark-and-sweep part, we'll use
that list and some other things (like the stack, dynamic environment,
etc.) to mark every object that's reachable, and then we'll traverse
this `allocated_objects` and `delete` everything that's not marked
reachable and remove those deleted pointers from the list. And there's
our GC.

Obviously we haven't yet put the "C" in our "GC" yet, but that's ok
for the time being, because we want to focus on the meat of our
interpreter before making sure the whole thing isn't just one giant
memory leak.

## Lisp type classes

Let's go ahead and make the classes whose types we had formerly
declared in the `enum`:

    class NIL : public lisp_object {
    private:
        NIL() : lisp_object(NIL_T) { }

    public:
        static const auto NIL_T = get_new_object_id("NIL");

        static NIL* get_nil() {
            static NIL nil_instance;
            &nil_instance;
        }
    } * const nil(NIL::get_nil());

    class CONS : public lisp_object {
    public:
        static const auto CONS_T = get_new_object_id("CONS");

        CONS() : lisp_object(CONS_T) { }

        OBJECT* car = nullptr, * cdr = nullptr;
    };

    class FIXNUM : public lisp_object {
    public:
        static const auto FIXNUM_T = get_new_object_id("FIXNUM");

        FIXNUM(long n) : lisp_object(FIXNUM_T), number(n) { }

        const long number;
    };

    class STRING : public lisp_object {
    public:
        static const auto STRING_T = get_new_object_id("STRING");

        STRING() : lisp_object(STRING_T) { }

        std::string data;
    };
    
There you can see we used `GENERATE_LISP_OBJECT_ID` in each of these
classes to create the object IDs.

`NIL` is going to be a singleton instance, and we'll provide a handy
access to it via `picl::nil` (and since this object isn't in the gc's
list of allocated objects, it won't get accidentally deallocated with
`delete` and cause
[undefined behavior][undefined-behavior-cpp]). `CONS` and `FIXNUM` are
pretty much as expected. I'm leaving all these member functions inline
because they're so short.

Something that deserves attention is the fact that I used
`std::string` for the representation of strings. `std::string` is
a typedef for `std::basic_string<char>`, and `char` is usually (not
always) 8 bits wide; we've implicitly chosen a character encoding
here.

But what about Unicode? UTF-8, 16, 32? [UTF-9? UTF-18?][utf-9-18]

Text is an enormously complex topic, but I'm putting my foot down:
we're supporting ASCII. That's right, we're using that ancient
Anglo-centric hillbilly encoding that uses 7 bits for each
character. Graphemes? Combining characters? Normalization? What are
snails even *trying* to do? We're just shoving those questions aside
so that we can get on with our lives.

![OBJECTION](http://imgur.com/QiBpAu2.png)

Ah, but you see, I can do that! Because according to
[Section 1.5.2.2](http://clhs.lisp.se/Body/01_ebb.htm):

> Portable code is written using only [standard characters](std-chars).

And there are only 97 "standard characters," and they all occur in
ASCII!

Now, just because portable code is *written* with standard characters
doesn't mean that somebody won't expect to be able to create a UTF-8
string programmatically. But we're going to let them worry about that
themselves and maybe write a library or something, because Common Lisp
doesn't make any guarantees about the range of character objects,
except that it supports the standard characters.

How about symbols, how will we know what we need to store in one? The
hyperspec has [a page](http://clhs.lisp.se/Body/t_symbol.htm) about
the `COMMON-LISP:SYMBOL` class and it details what a symbol is made
of, so let's pull an excerpt from that here:

> Symbols have the following attributes. For historical reasons, these
> are sometimes referred to as cells, although the actual internal
> representation of symbols and their attributes is
> implementation-dependent.
>
> ##### Name
>
> The name of a symbol is a string used to identify the
> symbol. Every symbol has a name, and the consequences are undefined
> if that name is altered. The name is used as part of the external,
> printed representation of the symbol; see Section 2.1 (Character
> Syntax). The function symbol-name returns the name of a given
> symbol. A symbol may have any character in its name.
> 
> ##### Package
>
> The object in this cell is called the home package of the symbol. If
> the home package is nil, the symbol is sometimes said to have no
> home package.  When a symbol is first created, it has no home
> package. When it is first interned, the package in which it is
> initially interned becomes its home package. The home package of a
> symbol can be accessed by using the function symbol-package.
>
> If a symbol is uninterned from the package which is its home
> package, its home package is set to nil. Depending on whether there
> is another package in which the symbol is interned, the symbol might
> or might not really be an uninterned symbol. A symbol with no home
> package is therefore called apparently uninterned.
>
> The consequences are undefined if an attempt is made to alter the
> home package of a symbol external in the COMMON-LISP package or the
> KEYWORD package.
>
> ##### Property list
>
> The property list of a symbol provides a mechanism for associating
> named attributes with that symbol. The operations for adding and
> removing entries are destructive to the property list. Common Lisp
> provides operators both for direct manipulation of property list
> objects (e.g., see getf, remf, and symbol-plist) and for implicit
> manipulation of a symbol's property list by reference to the symbol
> (e.g., see get and remprop). The property list associated with a
> fresh symbol is initially null.
> 
> ##### Value
>
> If a symbol has a value attribute, it is said to be bound, and that
> fact can be detected by the function boundp. The object contained in
> the value cell of a bound symbol is the value of the global variable
> named by that symbol, and can be accessed by the function
> symbol-value. A symbol can be made to be unbound by the function
> makunbound.  The consequences are undefined if an attempt is made to
> change the value of a symbol that names a constant variable, or to
> make such a symbol be unbound.
> 
> ##### Function
>
> If a symbol has a function attribute, it is said to be fbound, and
> that fact can be detected by the function fboundp. If the symbol is
> the name of a function in the global environment, the function cell
> contains the function, and can be accessed by the function
> symbol-function. If the symbol is the name of either a macro in the
> global environment (see macro-function) or a special operator (see
> special-operator-p), the symbol is fbound, and can be accessed by
> the function symbol-function, but the object which the function cell
> contains is of implementation-dependent type and purpose. A symbol
> can be made to be funbound by the function fmakunbound.  The
> consequences are undefined if an attempt is made to change the
> functional value of a symbol that names a special form.

In that first paragraph, the word "cell" might or might not have some
special meaning in lisp (that is, a "cell" might be something settable
by a lisp function or whatever), but thankfully the "internal
representation of symbols and their attributes is
implementation-dependent" so we can just add some member variables to
our `SYMBOL` class corresponding to (some of) the "cells" the spec
mentions.

We obviously have to have a name and home package (but that means we
need to create a `PACKAGE` class too). As for the property list,
value, and function cells, we'll come back to them later when we start
implementing `EVAL`. 

So here's our `SYMBOL` class.

    class SYMBOL : public lisp_object {
    public:
        static const auto SYMBOL_T = get_new_object_id("SYMBOL");

        SYMBOL(std::string r) : lisp_object(SYMBOL_T), repr(std::move(r)) { }

        const std::string repr;
        PACKAGE* home_package = nullptr;
    };

What about `PACKAGE`? A package is simply "a namespace that maps
symbol names to symbols". Well, that basically tells us exactly what
we need to put in our class:

    class PACKAGE : public lisp_object {
    public:
        static const auto PACKAGE_T = get_new_object_id("PACKAGE");

        PACKAGE() : lisp_object(PACKAGE_T) { }

        std::unordered_map<std::string, SYMBOL*> symbols;
    };

We'll have to come back to this and refine it later when we start
writing the code for `INTERN` and other package functions because of
things like internal symbols, inherited symbols, etc. But for now
it'll do.

You've probably noticed that our object hierarchy is very simple. Too
simple. It's missing a lot. Among many other things, it's completely
missing Common Lisp's powerful dynamic type system. But we have to
start somewhere, and this code will give us a good diving board off of
which to jump into the rest of the program.

Now that we have a few object definitions, we're ready to start on our
first bit of real code: the reader. But that's going to be pretty
involved because the reader is sophisticated (we won't finish it in
one sitting, it'll be iteratively developed over the lifetime of the
project because it ties in with all corners of lisp), so we'll get
started next time.

[Gehennom]: http://nethack.wikia.com/wiki/Gehennom
[cmake-intro]: http://derekmolloy.ie/hello-world-introductions-to-cmake/
[boehm-gc]: http://www.hboehm.info/gc/
[move-constructors]: http://blog.smartbear.com/c-plus-plus/c11-tutorial-introducing-the-move-constructor-and-the-move-assignment-operator/
[undefined-behavior-cpp]: http://blog.regehr.org/archives/213
[utf-9-18]: https://www.ietf.org/rfc/rfc4042.txt
[std-chars]: http://clhs.lisp.se/Body/26_glo_s.htm#standard_character
[jenkinss_hash]: https://en.wikipedia.org/wiki/Jenkins_hash_function
