
#ifndef THREAD_SAFE_HPP
#define THREAD_SAFE_HPP

#include <mutex>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <algorithm>

namespace picl {
    namespace thread_safe {
        // Not all members of this class are thread-safe. See each
        // function for details.
        // This class expects that the last node of the list will
        // never be removed.
        template<typename T>
        class boundary_slist {
        public:
            struct node {
                node(bool b, std::shared_ptr<node> n)
                    : is_boundary(b), next(std::move(n)) { }

                bool is_boundary;
                std::shared_ptr<node> next;

                T& get_datum();
            
                virtual ~node() { }
            };

            using ptr_type = std::shared_ptr<node>;

            struct typed_node : node {
                template<typename TT>
                typed_node(ptr_type n, TT&& d)
                    : node(false, std::move(n)), datum(std::forward<TT>(d)) { }

                T datum;
            };
            
            boundary_slist() {
                tail = head = std::make_shared<node>(true, nullptr);
            }

            // thread-safe
            ptr_type push_back_boundary() {
                auto ptr = std::make_shared<node>(true, nullptr);
                push_back_impl(ptr);
                return ptr;
            }

            // thread-safe
            template<typename TT>
            ptr_type push_back(TT&& item) {
                auto ptr = std::make_shared<typed_node>(nullptr, std::forward<TT>(item));
                push_back_impl(ptr);
                return ptr;
            }

            // thread-safe
            ptr_type get_head() const {
                return head;
            }

            // thread-safe
            ptr_type get_tail() const {
                return tail;
            }

            // thread-safe
            bool empty() const {
                return tail == nullptr;
            }

            // NOT thread-safe
            // Returns true if the node was removed, else false.
            // A pointer to a node not in the list should NOT be used with
            // this function.
            bool remove_successor(ptr_type& p) {
                if (p == nullptr)
                    return false;

                ptr_type next_node = p->next;

                if (next_node == nullptr)
                    return false;
                
                p->next = next_node->next;
                return true;
            }

            // NOT thread-safe
            bool remove_head() {
                if (head == tail)
                    return false;

                head = head->next;
            }
        
        private:
            // thread-safe
            void push_back_impl(ptr_type node) {
                // repeat until the atomic exchange works
                while (true) {
                    ptr_type tmp_tail = std::atomic_load_explicit(&tail, std::memory_order_relaxed);

                    ptr_type null = nullptr;
                    if (std::atomic_compare_exchange_weak(&tmp_tail->next, &null, node)) {
                        // this next line can fail; if it does, it
                        // just means that somebody changed this->tail
                        // while we weren't looking but that's OK
                        // because our node is already in the list.
                        std::atomic_compare_exchange_strong(&tail, &tmp_tail, node);
                        return;
                    }
                }
            }

            ptr_type head, tail;
        };

        template<typename T>
        T& boundary_slist<T>::node::get_datum() {
            if (is_boundary)
                throw std::runtime_error("Tried to get datum of boundary in boundary_slist");
                    
            return static_cast<typed_node*>(this)->datum;
        }
    }
}

#endif
