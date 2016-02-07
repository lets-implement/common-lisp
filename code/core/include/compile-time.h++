
#ifndef COMPILE_TIME
#define COMPILE_TIME

namespace picl {
    namespace compiletime_hash {
        using hash_t = std::uint32_t;

        constexpr hash_t combine_hash(const hash_t hash, const unsigned int i) {
            return ((hash + i) +
                    (((hash + i) << 10)))
                ^ (((hash + i)
                    + (((hash + i)
                        << 10))) >> 6);
        }

        namespace internal {
            static constexpr hash_t calculate_intermediate_hash(const char* str, const hash_t hash, const unsigned int i) {
                return (str[i] == 0)
                    ? hash
                    : calculate_intermediate_hash_(str, combine_hash(hash, str[i]), i + 1);
            }

            static constexpr hash_t calculate_final_hash(const hash_t hash) {
                return
                    ((hash + (hash << 3))
                     ^ ((hash + (hash << 3)) >> 11))
                    + (((hash + (hash << 3))
                        ^ ((hash + (hash << 3)) >> 11))
                       << 15);
            }
        }

        // calculates Jenkins's one-at-a-time hash at
        // compile-time. Read https://en.wikipedia.org/wiki/Jenkins_hash_function
        // for info on the hash.
        static constexpr hash_t string_hash(const char* str) {
            return internal::calculate_final_hash(internal::calculate_intermediate_hash(str, 0, 0));
        }
    }
}

#endif
