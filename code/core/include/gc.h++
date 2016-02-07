
#ifndef GCC_HPP
#define GCC_HPP

#include <list>
#include <type_traits>

#include "object.h++"

namespace picl {
    /* Pointers to lisp_objects that are not stored in other
     * lisp_objects should be wrapped in a lisp_object_handle so that
     * the GC knows about those references. */
    class lisp_object_handle {
    public:
        lisp_object_handle(lisp_object* const obj) : object(obj) {
            ++object->gc.crefcount;
        }

        lisp_object_handle(lisp_object&& dying) : object(dying.object) {
            dying.object = nullptr;
        }

        lisp_object_handle(const lisp_object_handle& rhs) : object(rhs.object) {
            ++object->gc.crefcount;
        }

        lisp_object_handle& operator=(const lisp_object_handle& rhs) {
            ++rhs.object->gc.crefcount;
            --object->gc.crefcount;
            object = rhs.object;
        }

        ~lisp_object_handle() {
            if (obj)
                --obj->gc.crefcount;
        }

        lisp_object* operator->() {
            return object;
        }

        lisp_object& operator*() {
            return *object;
        }

        const lisp_object* operator->() const {
            return object;
        }

        const lisp_object& operator*() const {
            return *object;
        }

    private:
        lisp_object* object;
    };

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
}

#endif
