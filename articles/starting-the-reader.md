
# Starting the reader

It's time that we started writing some real algorithms. The reader is
part of the [meta-circular evaluator system](meta-circular) that has
been making lisp so fascinating since 1958, and it is there that we
will dive in with something more than just object definitions.

[Last time](previous-article), we set up a skeletal bunch of class
definitions for the lisp objects we'll be manipulating. The reader
will most certainly make use of them. We'll be making heavy use of
[Section 2.1](http://clhs.lisp.se/Body/02_a.htm) from the spec and its
subsections. That page gives a pretty good definition of the reader:

> The Lisp reader takes characters from a stream, interprets them as a
> printed representation of an object, constructs that object, and
> returns it.

Conceptually very simple, but in practice it's a lot to do because the
reader knows how to construct quite a few types of objects in quite a
few different ways. In fact, with the sharp-dot operator `#.`,
arbitrary code can be evaluated at read-time. Our reader will
necessarily be incomplete until a ways down the road, but we'll at
least have something capable of reading basic forms by the end of this
article.

Let's move on to the next subsection,
[2.1.1, Readtables](http://clhs.lisp.se/Body/02_aa.htm). Here are a
few relevant passages for us to keep in mind when designing our
readtable class (all emphasis mine):

> Syntax information for use by the Lisp reader is embodied in an
> object called a readtable. Among other things, the readtable
> contains the association between characters and syntax
> types. *--Section 2.1.1*

> Several readtables describing different syntaxes can exist, but at
> any given time only one, called the current readtable, affects the
> way in which expressions[2] are parsed into objects by the Lisp
> reader. The current readtable in a given dynamic environment is the
> value of *readtable* in that environment. To make a different
> readtable become the current readtable, *readtable* can be assigned
> or bound. *--2.1.1.1*

> [The consequences are undefined][undefined] if an attempt is made to
> modify the standard readtable. *--2.1.1.2*

> The readtable case of the standard readtable is :upcase.

> The initial readtable is the readtable that is the current readtable
> at the time when the Lisp image starts. At that time, it conforms to
> standard syntax. **The initial readtable is distinct from the standard
> readtable. It is permissible for a conforming program to modify the
> initial readtable.** *--2.1.1.3*

> The Lisp reader is influenced not only by the current readtable, but
> also by various dynamic variables. The next figure lists the
> variables that influence the behavior of the Lisp reader. *--2.1.2*
> 
>     *package*    *read-default-float-format*  *readtable*
>     *read-base*  *read-suppress* 

You can also view the list of standard characters in
[Section 2.1.3](http://clhs.lisp.se/Body/02_ac.htm). 

The first quote above says that the readtable contains the
"association between characters and syntax types." The list of syntax
types is given in
[Figure 2-7 in Section 2.1.4](http://clhs.lisp.se/Body/02_ad.htm#charsyntaxtypesinstdsyntax),
reproduced here:

    character  syntax type                 character  syntax type             
    Backspace  constituent                 0--9       constituent             
    Tab        whitespace[2]               :          constituent             
    Newline    whitespace[2]               ;          terminating macro char  
    Linefeed   whitespace[2]               <          constituent             
    Page       whitespace[2]               =          constituent             
    Return     whitespace[2]               >          constituent             
    Space      whitespace[2]               ?          constituent*            
    !          constituent*                @          constituent             
    "          terminating macro char      A--Z       constituent             
    #          non-terminating macro char  [          constituent*            
    $          constituent                 \          single escape           
    %          constituent                 ]          constituent*            
    &          constituent                 ^          constituent             
    '          terminating macro char      _          constituent             
    (          terminating macro char      `          terminating macro char  
    )          terminating macro char      a--z       constituent             
    *          constituent                 {          constituent*            
    +          constituent                 |          multiple escape         
    ,          terminating macro char      }          constituent*            
    -          constituent                 ~          constituent             
    .          constituent                 Rubout     constituent             
    /          constituent                 

(Go ahead and read all the subsections of 2.1.4 if you haven't
already, they're pretty short and provide concise summaries of how
each of the character types are to be treated.)

Thus we know that we need a map from characters to syntax
information. Reading
[Section 2.1.4.4 (Macro Characters)](http://clhs.lisp.se/Body/02_add.htm)
tells us that we also need a "dispatch-table" for dispatch macro
characters, and then a map for each "sub-character" (character after a
dispatch macro character) to the corresponding macro function, as
installed by
[`SET-DISPATCH-MACRO-CHARACTER`](http://clhs.lisp.se/Body/f_set__1.htm#set-dispatch-macro-character):

> If a character is a dispatching macro character C1, its reader macro
> function is a function supplied by the implementation. This function
> reads decimal digit characters until a non-digit C2 is read. If any
> digits were read, they are converted into a corresponding integer
> infix parameter P; otherwise, the infix parameter P is nil. The
> terminating non-digit C2 is a character (sometimes called a
> ``sub-character'' to emphasize its subordinate role in the
> dispatching) that is looked up in the dispatch table associated with
> the dispatching macro character C1. The reader macro function
> associated with the sub-character C2 is invoked with three
> arguments: the stream, the sub-character C2, and the infix parameter
> P. For more information about dispatch characters, see the function
> set-dispatch-macro-character.

Taking all that information into account, we can code up the first
version of `READTABLE` (back in `object.h++` again):

    class READTABLE : public lisp_object {
    public:
        READTABLE() : lisp_object(READTABLE_T) { }

        static constexpr int num_characters = 128;
        
        struct character_info {
            character_info() : type(invalid) { }

            enum syntax_type {
                invalid,
                constituent,
                whitespace,
                terminating_macro,
                nonterminating_macro,
                terminating_dispatch_macro,
                nonterminating_dispatch_macro,
                multiple_escape,
                single_escape
            } type;

            struct dispatch_table {
                dispatch_table() {
                    // Initialize all the functions to nullptr
                    for (auto& i : char_funcs)
                        i.store(nullptr, std::memory_order_relaxed);
                }

                std::array<std::atomic<FUNCTION*>, num_characters> char_funcs;
            };

            // null if type != [terminating/nonterminating]_dispatch
            std::unique_ptr<dispatch_table> dispatch_char_dispatch_table;
        };

    private:
        std::array<character_info, num_characters> char_to_info;
    };
    
Each standard character will have a `character_info` entry associated
with it. The `character_info` struct stores the character's
`syntax_type` and dispatch table if it's a dispatch macro character. I
decided to make the four combinations of "terminating/nonterminating"
and "dispatch/nondispatch" into their own top-level types
`syntax_type`s so that we don't have to deal with types and subtypes.

If you've read [Section 2.1.4.2](http://clhs.lisp.se/Body/02_adb.htm),
you know that there's another table of information that we need to
worry about, namely the "constituent traits". This table tells us how
to treat characters when they appear as "constituents" (i.e. their
syntax type is `constituent` or they are escaped by a `single_escape`
or `multiple_escape`). However, this table is not modifiable by the
user, so instead of making a another array, I think we'll just use
this information when we write the code for the reader-macro function
for the `constituent` type and the escapes.

[meta-circular]: https://en.wikipedia.org/wiki/Meta-circular_evaluator
[previous-article]: int-main.md
[undefined]: http://clhs.lisp.se/Body/01_db.htm
