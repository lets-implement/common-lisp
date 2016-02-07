
#ifndef INTERPRETER_HPP
#define INTERPRETER_HPP

#include <stack>
#include <unordered_map>

#include "gc.h++"
#include "object.h++"

namespace picl {
    class stack_frame : public lisp_object {
    public:
        static const auto STACKFRAME_T = get_new_object_id("STACKFRAME");

        stack_frame(stack_frame* above) : lisp_object(STACKFRAME_T),
                                          above_frame(above) { }

        std::unordered_map<SYMBOL*, lisp_object*> lexical_bindings;

        stack_frame* const above_frame;
    };

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
}

#endif
