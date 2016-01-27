
# Let's Implement: Common Lisp

It is a truth universally acknowledged that Lisp is the third best
idea ever in human history, just behind indoor plumbing. Common Lisp
is probably the most widely used and supported language in the Lisp
family. It happens to be my favorite programming language, and since I
come from a long line of [ideal observer][ideal]s, good opinions run
in my family.

Even though Common Lisp was designed in the eighties, it's still
lightyears ahead of most languages despite the fact that they've been
stealing from it for ages. I could go on and on about macros and
[homoiconicity][], metaprogramming,
[multiple dispatch][multi-dispatch], [anaphoria][], [pandoria][], and
how lisp is the "programmable programming language," but I don't need
to tell you how great it is because you're already here and you
already know. You're a very smart person.

There are four or five high-quality actively-developed free
implementations. They're great and all, but I... I just don't feel
safe. About the future. About all my beautiful code. And uh, your
beautiful code too. You must have felt it: what if the guys who
maintain the lisps have an accident? What if they all get together for
a soirée or even a shindig, and they share a tainted pepperoni pizza?
What if they get drunk and decide to shoot the arm off of a saguaro
cactus under which they happen to be standing? And every copy of all
their source code and compiled binaries gets deleted at the same
time?? Then none of my, er, our code would run, and the world
would... well, something really bad would happen probably. The point
is, the knowledge of lisp isn't safe in so few hands. Everybody needs
to know how to implement Common Lisp.

So that's what we're going to do. We're going to implement it
together, all the way, and maybe even some non-standard extensions
(maybe). We're going to end up with a super-portable uncommonly
excellent Common Lisp and some meccha-kucha knowledge. And after we
implement it, you're going to teach your mom, your dad, your spouse,
children, and their friends how to do the same thing (you can teach
your friends too, when you get some). And then the world will be
whatever who cares, let's just get crackin'.

For this first article, I'm just going to set some goals and define
the scope of the project a little. I have a bad habit of making a
project too broad and then getting mired in details and recursive
subprojects ("oh, I want a website that offers users email? it's gotta
be great, so better roll my own <strike>smtp server</strike>
<strike>unicode library</strike> high-performance
transparently-asynchronous multithreaded IO library first!") and never
finishing anything, but that's not going to happen because this is
Different. Now, it *is* going to take some time, but Rome didn't burn
in a day you know.

## Goals

We want to end up with an interpreter program which is a conforming
implementation of the ANSI Common Lisp Specification (available online
at various places including [clhs.lisp.se][hyperspec]). I will be
optimizing the code for the following properties in this order:

1. Easy comprehensibility
2. Algorithmic simplicity
3. Portability
1. meccha kucha
5. Few/no external dependencies
6. Speed of development
7. Build simplicity

In particular, I do *not* want to optimize for spatial or temporal
performance. People have spent collective centuries optimizing lisp's
execution time, and we're certainly not going to be able to do
anything significant in the short amount of time we have together. But
we're not going to be stupid and use [association-lists][]
where we need hash tables or anything. It just means that we're not
going to write an optimizing JIT compiler (sorry).

### A "conforming implementation"

So, if you've browsed around the hyperspec by now (I encourage you to
read the intro), you've probably realized that Common Lisp is a
rather... *extensive* language, to put it kindly. This means that the
language supports everything you could want, but it also means that it
supports some (possibly a lot of) stuff that you don't want. So are we
going to *improve* the language by omitting some stuff like
[`NREVERSE`][nreverse] because side effects are evil? Are we going to
rename stuff like [`RPLACD`][rplacd] just because an aphasic gnome
named it? No! [Section 1.5.1.1](http://clhs.lisp.se/Body/01_eaa.htm)
tells us exactly what we need to do:

 > A conforming implementation shall accept all features (including
 > deprecated features) of the language specified in this standard,
 > with the meanings defined in this standard.
 >
 > A conforming implementation shall not require the inclusion of
 > substitute or additional language elements in code in order to
 > accomplish a feature of the language that is specified in this
 > standard.
 
Easy as pie.

And it's in this way that our development will be guided along. We'll
jump into the spec at the beginning, reading every page (probably not
sequentially) and writing code as we go. We'll document
[implementation-defined][impl-defined] behavior (but not necessarily
[implementation-*dependent*][impl-dependent] behavior -- big
difference) as the spec
[asks us to](http://clhs.lisp.se/Body/01_eab.htm). I expect it'll
actually be a pretty fun experience in reading standards documents,
too, which is an invaluable skill especially as you get closer to
hardware where precise and complete specifications are more precious.

## Implementation details

So, there are a lot of languages out there to write our interpreter
in. Sure, we could use Common Lisp and it'd be a heck of a lot easier
and more fun, but then how would we run our compiler when all the
other implementations are wiped off the face of the earth?

We have to make a choice: do we use a fancy high-level language that
will make development a breeze, or do we go low-level and tough it
out?

I chose the latter alternative for two reasons: one, because for this
project, we're trying to remove layers of abstraction, and
implementing a high-level language like Lisp in another high-level
language, it feels like cheating. Secondly, because we want this
compiler to work for all-time, right? And the language we choose has
to never go away, no matter how much Linus Torvalds hates it. So we're
going to use C++. It's too big to fail.

But C++ is a big language, bigger than Lisp, and we have a lot of
temptations to resist. You're going to want to use design
patterns. You're going to want to use templates for
[SFINAE][]. You're going to want to use all the fancy features
of C++11/14/17. But we're going to try not to get too caught up in the
design of class hierarchies, metaprogramming, and some other third
thing to be swallowed up by the wiles of the Lotus Eaters. We want to
write something that doesn't depend too much on one language;
something durable, something independent of the whims and caprices of
our fickle age, like [TAOCP][] (but maybe just a little
less... *Knuth*). So C++ it is. We could also use C, but LOL NO.

Of course, diversity is healthy, so if you want to follow along in a
language of your own choosing, that'll be great too. Let me know if
you do and I'll link to your GitHub project.

So with the pleasantries out of the way, let's get started!

[ideal]: https://en.wikipedia.org/wiki/Ideal_observer_theory
[homoiconicity]: http://c2.com/cgi/wiki?HomoiconicLanguages
[multi-dispatch]: https://en.wikipedia.org/wiki/Multiple_dispatch
[anaphoria]: http://letoverlambda.com/index.cl/guest/chap6.html#sec_1
[pandoria]: http://letoverlambda.com/index.cl/guest/chap6.html#sec_7
[hyperspec]: clhs.lisp.se/
[association-lists]: https://en.wikipedia.org/wiki/Association_list
[nreverse]: http://clhs.lisp.se/Body/f_revers.htm
[rplacd]: http://clhs.lisp.se/Body/f_rplaca.htm
[SFINAE]: https://en.wikipedia.org/wiki/Substitution_failure_is_not_an_error
[TAOCP]: http://www-cs-faculty.stanford.edu/~uno/taocp.html
[impl-defined]: http://clhs.lisp.se/Body/26_glo_i.htm#implementation-defined
[impl-dependent]: http://clhs.lisp.se/Body/26_glo_i.htm#implementation-dependent
