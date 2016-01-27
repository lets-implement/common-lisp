
#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <array>
#include <string>
#include <atomic>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <unordered_map>

namespace picl {
    enum lisp_type {
        NIL_T,
        CONS_T,
        FIXNUM_T,
        SYMBOL_T,
        STRING_T,
        PACKAGE_T,
        READTABLE_T
    };

    struct gc_info {
        // The initialization of crefcount isn't atomic, but I don't
        // think it needs to be.
        gc_info() : crefcount(0) { }

        std::atomic_uint_fast32_t crefcount;
    };

    class lisp_object {
    public:
        lisp_object(lisp_type t) : type(t) { }
        
        const lisp_type type;
        gc_info gc_use;
    };

    using atomic_lisp_ptr = std::atomic<lisp_object*>;

    class NIL : public lisp_object {
    private:
        NIL() : lisp_object(NIL_T) { }

    public:
        static NIL* get_nil() {
            static NIL nil_instance;
            &nil_instance;
        }
    } * const nil(NIL::get_nil());

    class CONS : public lisp_object {
    public:
        CONS() : lisp_object(CONS_T), car(nullptr), cdr(nullptr) { }

        atomic_lisp_ptr car, cdr;
    };

    class FIXNUM : public lisp_object {
    public:
        FIXNUM() : lisp_object(FIXNUM_T), number() { }

        long number;
    };

    class STRING : public lisp_object {
    public:
        STRING() : lisp_object(STRING_T) { }

        std::string data;
    };

    class PACKAGE;

    class SYMBOL : public lisp_object {
    public:
        SYMBOL() : lisp_object(SYMBOL_T), home_package(0) { }

        const std::string repr;
        std::atomic<PACKAGE*> home_package;
    };

    class PACKAGE : public lisp_object {
    public:
        PACKAGE() : lisp_object(PACKAGE_T) { }

        std::unordered_map<std::string, std::atomic<SYMBOL*>> symbols;
    };

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
}

#endif
