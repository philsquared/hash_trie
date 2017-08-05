#include "hash_trie.hpp"

#include "catch.hpp"

#include <unordered_set>
#include <iostream>


std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-";

struct Containers {
    std::vector<std::string> vector_strings;
    std::vector<int> vector_ints;
    std::vector<size_t> vector_hashes;
    hamt::hash_trie<int> hamt_ints;
    hamt::hash_trie<std::string> hamt_strings;
    std::set<std::string> set_strings;
    std::set<int> set_ints;
    std::unordered_set<std::string> unordered_set_strings;
    std::unordered_set<int> unordered_set_ints;

    Containers( size_t size ) {
        vector_strings.reserve( size );
        vector_ints.reserve( size );
        vector_hashes.reserve( size );

        auto len = static_cast<size_t>( std::ceil(std::log( size )/std::log(64)) );
        std::vector<size_t> indices(size, 0);
        std::string str(len, '-');
        for( size_t i =0; i < size; ++i ) {
            if( i % (size/10) == 0 ) {
                std::cout << ".";
                std::cout.flush();
            }

            bool incDigit = true;
            for( size_t pos = 0; pos < len; ++pos ) {
                str[pos] = chars[indices[pos]];
                if( incDigit ) {
                    if( ++indices[pos] == 64 )
                        indices[pos] = 0;
                    else
                        incDigit = false;
                }
            }
            vector_ints.push_back(i);
            hamt_ints.insert(i);
            set_ints.insert(i);
            unordered_set_ints.insert(i);

            vector_strings.push_back( str );
            hamt_strings.insert( str );
            set_strings.insert( str );
            unordered_set_strings.insert( str );

            vector_hashes.push_back( std::hash<std::string>()( str ) );
        }
    }
};

size_t num = 1000000;

template<typename Container, typename Iterator>
bool isEnd( Iterator const& it, Container const& container ) {
    return it == container.end();
}
template<typename T, typename Iterator>
bool isEnd( Iterator const& it, hamt::hash_trie<T> const& ) {
    return !it.leaf();
}

template<typename Src, typename Dest>
int testFind( Src const& src, Dest& dest ) {
    int count = 0;
    for (auto const &item : src)
        count += isEnd(dest.find(item), dest) ? 0 : 1;

    return count;
}

TEST_CASE( "benchmarks", "[benchmarks]" ) {

    static Containers containers( num );

    int count = 0;

    SECTION ( "insert ints" ) {
        std::set<int> set_ints;
        BENCHMARK( "set<int>::insert" ) {
            for( int i =0; i < num; ++i )
                set_ints.insert( i );
        }
        CHECK( set_ints.size() == num );

        std::unordered_set<int> unordered_set_ints;
        BENCHMARK("unordered_set<int>::insert" ) {
            for( int i =0; i < num; ++i )
                unordered_set_ints.insert( i );
        }
        CHECK( unordered_set_ints.size() == num );

        hamt::hash_trie<int> hamt_ints;
        BENCHMARK("hash_trie<int>::insert" ) {
            for( int i =0; i < num; ++i )
                hamt_ints.insert( i );
        }
        CHECK( hamt_ints.size() == num );
    }

    SECTION ( "find ints" ) {
        BENCHMARK("hash_trie<int>::find" ) {
            count = testFind( containers.vector_ints, containers.hamt_ints );
        }
        CHECK( count > 0 );

        BENCHMARK("set<int>::find" ) {
            count = testFind( containers.vector_ints, containers.set_ints );
        }
        CHECK( count > 0 );

        BENCHMARK("unordered_set<int>::find" ) {
            count = testFind( containers.vector_ints, containers.unordered_set_ints );
        }
        CHECK( count > 0 );
    }


    SECTION ( "find strings" ) {
        BENCHMARK("hash_trie<string>::find" ) {
            count = testFind( containers.vector_strings, containers.hamt_strings );
        }
        CHECK( count > 0 );

        BENCHMARK("set<string>::find" ) {
            count = testFind( containers.vector_strings, containers.set_strings );
        }
        CHECK( count > 0 );

        BENCHMARK("unordered_set<string>::find" ){
            count = testFind( containers.vector_strings, containers.unordered_set_strings );
        }
        CHECK( count > 0 );
    }

    SECTION ( "find hashes" ) {
        BENCHMARK("hash_trie<hash>::find" ) {
            count = testFind( containers.vector_hashes, containers.hamt_ints );
        }
        CHECK( count > 0 );

        BENCHMARK("set<hash>::find" ) {
            count = testFind( containers.vector_hashes, containers.set_ints );
        }
        CHECK( count > 0 );

        BENCHMARK("unordered_set<hash>::find" ) {
            count = testFind( containers.vector_hashes, containers.unordered_set_ints );
        }
        CHECK( count > 0 );
    }

    SECTION( "count_set_bits" ) {
        int totals = 0;
        BENCHMARK("count_set_bits" ) {
            for( auto hash : containers.vector_hashes )
                totals += hamt::detail::count_set_bits( hash );
        }
        CHECK( totals > 0 );

        totals = 0;
        BENCHMARK("count_set_bits_popcount" ) {
            for( auto hash : containers.vector_hashes ) {
                totals += hamt::detail::count_set_bits_popcount( hash );
            }
        }
        CHECK( totals > 0 );
    }
}
