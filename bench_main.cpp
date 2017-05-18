#include "hash_trie.hpp"
#include <vector>
#include <set>
#include <unordered_set>

#include <nonius/nonius_single.h++>

size_t num = 1000000;
std::vector<std::string> vector_strings;
std::vector<int> vector_ints;
std::vector<size_t> vector_hashes;
hamt::hash_trie<int> hamt_ints;
hamt::hash_trie<std::string> hamt_strings;
std::set<std::string> set_strings;
std::set<int> set_ints;
std::unordered_set<std::string> unordered_set_strings;
std::unordered_set<int> unordered_set_ints;

template<typename Container, typename Iterator>
bool isEnd( Iterator const& it, Container const& container ) {
    return it == container.end();
}
template<typename T, typename Iterator>
bool isEnd( Iterator const& it, hamt::hash_trie<T> const& ) {
    return !it.leaf();
}


int main(int argc, char * argv[]) {

    std::cout << "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;

    if( argc > 2 && std::string(argv[argc-2]) == "-i" ) {
        std::stringstream ss;
        ss << argv[argc-1];
        ss >> num;
        if( ss.fail() ) {
            std::cerr << "[" << argv[argc-1] << "] is not a number" << std::endl;
            return -1;
        }
        std::cout << "Using custom size of: " << num << std::endl;
        argc -= 2;
    }
    else
        std::cout << "Using default size of: " << num << std::endl;

    vector_strings.reserve(num);
    vector_ints.reserve(num);
    vector_hashes.reserve(num);

    std::cout << "Setting up ";
    std::cout.flush();

    std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-";
    auto len = static_cast<size_t>( std::ceil(std::log(num)/std::log(64)) );
    std::vector<size_t> indices(num, 0);
    std::string str(len, '-');
    for( size_t i =0; i < num; ++i ) {
        if( i % (num/10) == 0 ) {
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
    std::cout << " completed" << std::endl;
    std::cout << ". . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . ." << std::endl;

    nonius::main(argc, argv);
}

template<typename Src, typename Dest>
void testFind( Src const& src, Dest& dest ) {
    int count = 0;
    const int iterations = 10;

    for( int j = 0; j < iterations; ++j ) {
        for (auto const &item : src)
            count += isEnd(dest.find(item), dest) ? 0 : 1;
    }

    if( count < 0 )
        throw std::domain_error("wrong count: " + std::to_string( count ) );
}


NONIUS_BENCHMARK("hash_trie<int>::insert", []{
    hamt::hash_trie<int> ints;
    for( int i =0; i < num; ++i )
        ints.insert( i );
});

NONIUS_BENCHMARK("set<int>::insert", []{
    std::set<int> ints;
    for( int i =0; i < num; ++i )
        ints.insert( i );
});
NONIUS_BENCHMARK("unordered_set<int>::insert", []{
    std::unordered_set<int> ints;
    for( int i =0; i < num; ++i )
        unordered_set_ints.insert( i );
});


NONIUS_BENCHMARK("hash_trie<int>::find", []{
    testFind( vector_ints, hamt_ints );
});

NONIUS_BENCHMARK("set<int>::find", []{
    testFind( vector_ints, set_ints );
});

NONIUS_BENCHMARK("unordered_set<int>::find", []{
    testFind( vector_ints, unordered_set_ints );
});


NONIUS_BENCHMARK("hash_trie<hash>::find", []{
    testFind( vector_hashes, hamt_ints );
});

NONIUS_BENCHMARK("set<hash>::find", []{
    testFind( vector_hashes, set_ints );
});

NONIUS_BENCHMARK("unordered_set<hash>::find", []{
    testFind( vector_hashes, unordered_set_ints );
});




NONIUS_BENCHMARK("hash_trie<string>::find", []{
    testFind( vector_strings, hamt_strings );
});

NONIUS_BENCHMARK("set<string>::find", []{
    testFind( vector_strings, set_strings );
});

NONIUS_BENCHMARK("unordered_set<string>::find", []{
    testFind( vector_strings, unordered_set_strings );
});

NONIUS_BENCHMARK("count_set_bits", []{
    int totals = 0;
    for( auto hash : vector_hashes ) {
        totals += hamt::detail::count_set_bits( hash );
    }
    if( totals == 0 )
        throw std::logic_error( "zero" );
});

NONIUS_BENCHMARK("count_set_bits_popcount", []{
    int totals = 0;
    for( auto hash : vector_hashes ) {
        totals += hamt::detail::count_set_bits_popcount( hash );
    }
    if( totals == 0 )
        throw std::logic_error( "zero" );
});
