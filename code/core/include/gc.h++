
#ifndef GCC_HPP
#define GCC_HPP

#include <vector>
#include <type_traits>

#include "object.h++"
#include "thread_safe.h++"

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

            // the above assignment involves an atomic operation, so
            // it is not possible for it to be reordered after the
            // following push_back() call. That's important because we
            // don't want the garbage collector to examine an object
            // before other objects have references to it and also
            // before we've added a cref to it.

            allocated_objects.push_back(&new_object);

            return retval;
        }

    private:
        thread_safe::boundary_slist<object*> allocated_objects;
    };
}

#endif
