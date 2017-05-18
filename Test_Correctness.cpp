#include "hash_trie.hpp"

#include "catch.hpp"

#include <iostream>

TEST_CASE( "iterate" ) {

    using namespace hamt;

    const int num = 1000;

    std::set<int> expected;
    hash_trie<int> h;
    for(int i=0; i < num; ++i ) {
        h.insert(i);
        expected.insert( i );
    }


    auto it = h.begin();
    auto itEnd = h.end();

    std::set<int> actual;

    int count = 0;
    for(; it != itEnd; ++it ) {
        //std::cout << *it << std::endl;
        actual.insert( *it );
        count++;
    }

    CHECK( count == num );

    std::set<int> diff;
    std::set_difference( expected.begin(), expected.end(), actual.begin(), actual.end(), std::inserter( diff, diff.end() ) );

    if( !diff.empty() ) {
        std::cout << "Missing:\n";
        for (auto x : diff) {
            std::cout << x << std::endl;
        }
    }
}
