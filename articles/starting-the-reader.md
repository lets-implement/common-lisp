
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
> "sub-character" to emphasize its subordinate role in the
> dispatching) that is looked up in the dispatch table associated with
> the dispatching macro character C1. The reader macro function
> associated with the sub-character C2 is invoked with three
> arguments: the stream, the sub-character C2, and the infix parameter
> P. For more information about dispatch characters, see the function
> set-dispatch-macro-character.

The last paragraph in the quote above is important because it lets us
know that we need to set up the facilities (or at least the interface
to the facilities) for accessing dynamic variables from native code
before we write the reader. To do that, we're going to have to write
our `lisp_state` class, which encapsulates the state of an instance of
our interpreter, and we'll get on that after we finish researching
what other things we need for the reader.

## Readtable class

So it turns out we weren't quite ready to write `READ` after all; we
still have a few more things to set up. Taking into account all the
information we discussed in the previous section, we can code up the
first version of `READTABLE` (back in `object.h++` again):

    class READTABLE : public lisp_object {
    public:
        static const auto READTABLE_T = get_new_object_id("READTABLE");

        READTABLE() : lisp_object(READTABLE_T) { }

        static constexpr int num_characters = 128;
        
        struct character_info {
            enum syntax_type {
                syntax_type_invalid,
                syntax_type_constituent,
                syntax_type_whitespace,
                syntax_type_terminating_macro,
                syntax_type_nonterminating_macro,
                syntax_type_terminating_dispatch_macro,
                syntax_type_nonterminating_dispatch_macro,
                syntax_type_multiple_escape,
                syntax_type_single_escape
            } type = syntax_type_invalid;

            struct dispatch_table {
                std::array<FUNCTION*, num_characters> char_funcs = { };
            };

            // null if type != [terminating/nonterminating]_dispatch
            std::unique_ptr<dispatch_table> dispatch_char_dispatch_table;
        };

    private:
        std::array<character_info, num_characters> char_to_info;
    };
    
Each standard character will have a `character_info` entry associated
with it, and you can access a character's `character_info` instance by
using the character's numerical code as an index into the readtable's
`char_to_info` member. The `character_info` struct stores the
character's `syntax_type` and dispatch table if it's a dispatch macro
character. If it is a dispatch macro character, you can get the macro
function by using the dispatch character as an index in the
`char_funcs` member of the `dispatch_table` pointed to by
`dispatch_char_dispatch_table`. We'll define the standard readtable
when we start on the reader.

I decided to make the four combinations of
"terminating/nonterminating" and "dispatch/nondispatch" into their own
top-level types `syntax_type`s so that we don't have to deal with
types and subtypes.

If you've read [Section 2.1.4.2](http://clhs.lisp.se/Body/02_adb.htm),
you know that there's another table of information that we need to
worry about, namely the "constituent traits". This table tells us how
to treat characters when they appear as "constituents" (i.e. their
syntax type is `constituent` or they are escaped by a `single_escape`
or `multiple_escape`). However, this table is not modifiable by the
user ("no mechanism is provided for changing the constituent trait of
a character" *--2.1.4.2*, and of
[`SET-SYNTAX-FROM-CHAR`](http://clhs.lisp.se/Body/f_set_sy.htm#set-syntax-from-char),
"`set-syntax-from-char` makes the syntax of *to-char* in
*to-readtable* be the same as the syntax of *from-char* in
*from-readtable* ... The constituent traits of *from-char* are not
copied"), and I think it'll actually turn out to be easier to just
hardcode this info in the reader algorithm, coming up next, instead of
making it an actual table.

## Lisp state

We mentioned earlier that the reader is going to (among other things)
require access to dynamic variables to do its job. We should get that
out of the way and design the class that will encapsulate the state of
an instance of our interpreter; it'll contain things like the stack,
the dynamic environment, and the garbage collector.

Let's put this in its own header and name it `interpreter.h++` (I'll
skip the header guards, namespace declaration, etc.):

    class interpreter {
    public:
        gc allocator;

    private:
        // Associates package names -> packages
        std::unordered_map<std::string, PACKAGE*> packages;

        // special variables -> values
        std::unordered_map<SYMBOL*, lisp_object*> dynamic_bindings;

        stack_frame* stack = nullptr;
    };

Our stack frame class will be a `lisp_object` itself, because we want
it to be garbage-collected so that lambdas can capture them for
closures and all that crunchiness.

    class stack_frame : public lisp_object {
    public:
        static const auto STACKFRAME_T = get_new_object_id("STACKFRAME");

        stack_frame(stack_frame* above) : lisp_object(STACKFRAME_T),
                                          above_frame(above) { }

        std::unordered_map<SYMBOL*, lisp_object*> lexical_bindings;

        stack_frame* const above_frame;
    };

We used a `SYMBOL*` as a key in that `unordered_map` there. In
general, you don't want to use pointers to objects as keys in a hash
map because pointer equality isn't the same as object equality; in
this special case, however, two symbols are equal if and only if their
addresses are equal, as long as we code our `INTERN` function
correctly. This is actually great, because we get a fast hash function
and we don't have to make the choice between either creating our own
memoized hash function for `SYMBOL*`s or recalculating the hash every
time we use the table like we would if we hash the string
representation of a symbol.

## The reader algorithm

We've worked our way through the pre-preliminaries for writing the
reader, and the next thing in section 2 is
[2.2 Reader Algorithm](http://clhs.lisp.se/Body/02_b.htm). This page
lists the ten steps in the reader algorithm in sufficient detail to
implement it. Let's read through this section and keep a list of what
else we need to set up before we write the reader. (For the sake of
the readability of the plain-text versions of these documents, I'm not
going to preserve the many links in these quotations; if you can, I
recommend reading along in the hyperspec itself rather than here, so
that you can chase any links that take your fancy).

> This section describes the algorithm used by the Lisp reader to
> parse objects from an input character stream, including how the Lisp
> reader processes macro characters.

Well this immediately lets us know that we have to set up our
interfaces for character streams and the functions thereof.

> When dealing with tokens, the reader's basic function is to
> distinguish representations of symbols from those of numbers. When a
> token is accumulated, it is assumed to represent a number if it
> satisfies the syntax for numbers listed in Figure 2-9. If it does
> not represent a number, it is then assumed to be a potential number
> if it satisfies the rules governing the syntax for a potential
> number. If a valid token is neither a representation of a number nor
> a potential number, it represents a symbol.

Common Lisp supports several types of numbers including
arbitrary-precision integers, so when we write our algorithm for
`READ`, we'll have to take into account the fact that we haven't
written those classes yet. But we have `FIXNUM`, so we'll settle for
that at the moment and we can implement handling for things like
`BIGNUM` and `RATIO` later.

A "potential number" is just an implementation-defined syntax for
numbers, but our implementation isn't going to support any extra
numerical syntaxes, so for all intensive porpoises "potential number"
and "number" are synonymous. (Plain-text readers, sorry for the inline
HTML, but Markdown stupidly doesn't allow beginning a list from a
number other than 1.)

> The algorithm performed by the Lisp reader is as follows:
> 
> <ol start="1">
> <li>If at end of file, end-of-file processing is performed as
>     specified in read. Otherwise, one character, <i>x</i>, is read from the
>     input stream, and dispatched according to the syntax type of <i>x</i> to
>     one of steps 2 to 7.</li>
> </ol>

We'll have to look at the
[documentation for `READ` and `READ-PRESERVING-WHITESPACE`](http://clhs.lisp.se/Body/f_rd_rd.htm#read)
to learn the "end-of-file processing as specified in read":

> `read` &optional *input-stream eof-error-p eof-value recursive-p* => *object*
>
> `read-preserving-whitespace` &optional *input-stream eof-error-p eof-value recursive-p* => *object*
>
> If a file ends in a symbol or a number immediately followed by an
> end of file[1], `read` reads the symbol or number successfully; when
> called again, it sees the end of file[1] and only then acts
> according to *eof-error-p*. If a file contains ignorable text at the
> end, such as blank lines and comments, `read` does not consider it to
> end in the middle of an object.
>
> If *recursive-p* is true, the call to `read` is expected to be made
> from within some function that itself has been called from `read` or
> from a similar input function, rather than from the top level.
>
> Both functions return the object read from
> *input-stream*. *Eof-value* is returned if *eof-error-p* is false
> and end of file is reached before the beginning of an object.

And, in the **Exceptional Situations** notes at the bottom,

> `read` signals an error of type `end-of-file`, regardless of
> *eof-error-p*, if the file ends in the middle of an object
> representation. For example, if a file does not contain enough right
> parentheses to balance the left parentheses in it, `read` signals an
> error. This is detected when `read` or `read-preserving-whitespace`
> is called with *recursive-p* and *eof-error-p* non-nil, and
> end-of-file is reached before the beginning of an object.
>
> If *eof-error-p* is true, an error of type `end-of-file` is signaled at
> the end of file.

So that means that we have to set up the interface for errors.

> <ol start="2"><li>If <i>x</i> is an invalid character, an error of type
> reader-error is signaled.</li></ol>

There's errors again.

> <ol start="3"><li>If <i>x</i> is a whitespace[2] character, then it is
> discarded and step 1 is re-entered.</li></ol>

Nothing to see here.

> <ol start="4"><li>
>    <p>If <i>x</i> is a terminating or non-terminating macro character then its
>    associated reader macro function is called with two arguments,
>    the input stream and x.</p>
>
>    <p>The reader macro function may read characters from the input stream;
>    if it does, it will see those characters following the macro
>    character. The Lisp reader may be invoked recursively from the
>    reader macro function.</p>
> 
>    <p>The reader macro function must not have any side effects other than
>    on the input stream; because of backtracking and restarting of the
>    read operation, front ends to the Lisp reader (e.g., "editors" and
>    "rubout handlers") may cause the reader macro function to be
>    called repeatedly during the reading of a single expression in which
>    <i>x</i> only appears once.</p>
> 
>    <p>The reader macro function may return zero values or one value. If
>    one value is returned, then that value is returned as the result of
>    the read operation; the algorithm is done. If zero values are
>    returned, then step 1 is re-entered.</p>
> </ol>

Therefore we need the ability to invoke function calls when a macro
character is encountered.

> <ol start="5"><li>
>    If <i>x</i> is a single escape character then the next character, <i>y</i>, is
>    read, or an error of type end-of-file is signaled if at the end of
>    file. <i>y</i> is treated as if it is a constituent whose only constituent
>    trait is alphabetic. <i>y</i> is used to begin a token, and step 8 is
>    entered.
> </li></ol>

Whenever we encounter an escaped character, we don't check if it's a
macro character or whatever. We just begin a token.

> <ol start="6"><li>
> If <i>x</i> is a multiple escape character then a token
> (initially containing no characters) is begun and step 9 is entered.
> </li></ol>

Ok.

> <ol start="7"><li>
>    If <i>x</i> is a constituent character, then it begins a
>    token. After the token is read in, it will be interpreted either
>    as a Lisp object or as being of invalid syntax. If the token
>    represents an object, that object is returned as the result of
>    the read operation. If the token is of invalid syntax, an error
>    is signaled. If <i>x</i> is a character with case, it might be
>    replaced with the corresponding character of the opposite case,
>    depending on the readtable case of the current readtable, as
>    outlined in <a href="http://clhs.lisp.se/Body/23_ab.htm">Section
>    23.1.2 (Effect of Readtable Case on the Lisp
>    Reader)</a>. <i>X</i> is
>    used to begin a token, and step 8 is entered.
> </li></ol>

Recall that a "token" is for us either a number or a symbol since we
won't support potential numbers.

> <ol start="8"><li>
>     At this point a token is being accumulated, and an even number
>     of multiple escape characters have been encountered. If at end
>     of file, step 10 is entered. Otherwise, a character, <i>y</i>, is read,
>     and one of the following actions is performed according to its
>     syntax type:
>      <ul><li>If <i>y</i> is a constituent or non-terminating macro character:
>              <ul><li>If <i>y</i> is a character with case, it might be replaced with the
>                      corresponding character of the opposite case, depending on the
>                      readtable case of the current readtable, as outlined in Section
>                      23.1.2 (Effect of Readtable Case on the Lisp Reader).</li>
>                  <li><i>Y</i> is appended to the token being built.</li>
>                  <li>Step 8 is repeated.</li>
>              </ul>
>         <li>If <i>y</i> is a single escape character, then the next character, <i>z</i>,
>             is read, or an error of type end-of-file is signaled if at end of
>             file. <I>Z</I> is treated as if it is a constituent whose only
>             constituent trait is alphabetic[2]. <I>Z</I> is appended to the token
>             being built, and step 8 is repeated.</li>
>         <li>If <i>y</i> is a multiple escape character, then step 9 is entered.</li>
>         <li>If <i>y</i> is an invalid character, an error of type reader-error is signaled.</li>
>         <li>If <i>y</i> is a terminating macro character, then it terminates the
>             token. First the character <i>y</i> is unread (see unread-char), and then
>             step 10 is entered.</li>
>         <li>If <i>y</i> is a whitespace[2] character, then it terminates the
>             token. First the character <i>y</i> is unread if appropriate (see
>             read-preserving-whitespace), and then step 10 is entered.</li>
>      </ul>
> </li></ol>

You can see the components of the code we'll write in this description
of the algorithm; a while-loop, a switch or a sequence of
if-statements.

> <ol start="9"><li>
> At this point a token is being accumulated, and an odd number of
> multiple escape characters have been encountered. If at end of file,
> an error of type end-of-file is signaled. Otherwise, a character, <i>y</i>,
> is read, and one of the following actions is performed according to
> its syntax type:
>   <ul>
>     <li>If <i>y</i> is a constituent, macro, or whitespace[2]
>         character, <i>y</i> is treated as a constituent whose only
>         constituent trait is alphabetic[2]. <I>Y</I> is appended to
>         the token being built, and step 9 is repeated.</li>
>     <li> If <i>y</i> is a single escape character, then the next
>          character, <i>z</i>, is read, or an error of type end-of-file is
>          signaled if at end of file. <I>Z</I> is treated as a constituent
>          whose only constituent trait is alphabetic[2]. <I>Z</I> is appended
>          to the token being built, and step 9 is repeated.</li>
>     <li>If <i>y</i> is a multiple escape character, then step 8 is
>         entered.</li>
>     <li>If <i>y</i> is an invalid character, an error of type
>         reader-error is signaled.</li>
>   </ul>
> </li></ol>

Pretty straightforward. It's really nice how the writers say "At this
point, *blah* has happened," don't you think?

> <ol start="10"><li>
> An entire token has been accumulated. The object represented by the
> token is returned as the result of the read operation, or an error of
> type reader-error is signaled if the token is not of valid syntax.
> </li></ol>

### Summary of required interfaces

Here's a summary of the stuff we need to create interfaces for (we can
implement them later) before we write the reader algorithm:

1. Error/signals interface
2. Function calls
3. The stream class and some of its functions

Not too bad, eh? Let's start with function calls. We actually already
used them in our declaration of the `READTABLE` class above without
providing a declaration.

### Function calls

I think we'll make a class with a virtual `call` member function that
will execute the body of the function, and it'll take its arguments
and return its values on the stack. We can then make subclasses of
this class later so that we can easily call interpreted lisp code or
native C++ code with a uniform interface.

In our old friend `object.h`:

    class interpreter;

    class FUNCTION : public lisp_object {
    public:
        static const auto FUNCTION_T = get_new_object_id("FUNCTION");

        FUNCTION() : lisp_object(FUNCTION_T) { }

        virtual std::uint32_t call(interpreter*, std::uint32_t arg_count) = 0;
    };

Next, let's get the interface for errors and signals prototyped out.

### Errors

Common Lisp's "signal" system is a bit exotic compared to most other
languages. It not only allows for non-local control-flow like C++ and
family's exceptions do, but it also allows control to be *resumed*
from where it left off 

[meta-circular]: https://en.wikipedia.org/wiki/Meta-circular_evaluator
[previous-article]: int-main.md
[undefined]: http://clhs.lisp.se/Body/01_db.htm
