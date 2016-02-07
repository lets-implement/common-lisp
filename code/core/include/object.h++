
#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <array>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <unordered_map>

namespace picl {
    struct gc_info {
        gc_info() : crefcount(0) { }

        unsigned int crefcount;
    };

    class lisp_object {
    public:
        using lisp_type_t = unsigned int;

        lisp_object(lisp_type t) : type(t) { }

        const lisp_type_t type;

        gc_info gc_use;

        static lisp_type_t get_new_object_id(std::string classname) {
            object_id_to_name.push_back(std::move(classname));
            return object_id_to_name.size() - 1;
        }

    private:
        static std::vector<std::string> object_id_to_name;
    };

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

        FIXNUM(const long n) : lisp_object(FIXNUM_T), number(n) { }

        const long number;
    };

    class STRING : public lisp_object {
    public:
        static const auto STRING_T = get_new_object_id("STRING");

        STRING() : lisp_object(STRING_T) { }

        std::string data;
    };

    class PACKAGE;

    class SYMBOL : public lisp_object {
    public:
        static const auto SYMBOL_T = get_new_object_id("SYMBOL");

        SYMBOL(std::string r) : lisp_object(SYMBOL_T), repr(std::move(r)) { }

        const std::string repr;
        PACKAGE* home_package = nullptr;
    };

    

    class PACKAGE : public lisp_object {
    public:
        static const auto PACKAGE_T = get_new_object_id("PACKAGE");

        PACKAGE() : lisp_object(PACKAGE_T) { }

        std::unordered_map<std::string, SYMBOL*> symbols;
    };

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
}

#endif
