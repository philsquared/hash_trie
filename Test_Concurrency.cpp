#include "hash_trie.hpp"

#include "catch.hpp"

TEST_CASE( "is_lock_free", "[!mayfail]" ) {

    // Depending on th platform this may or may not be a lock-free implementation
    REQUIRE( hamt::shared_hash_trie<int>().is_lock_free() );
}

TEST_CASE( "transaction commit" ) {

    using namespace hamt;

    shared_hash_trie<int> sh;

    auto trans = sh.start_transaction();

    hash_trie<int> h = trans;

    h.insert( 1 );
    h.insert( 2 );
    h.insert( 10 );

    REQUIRE( hash_trie<int>( sh ).size() == 0 );

    trans.commit( h );

    hash_trie<int> h2 = sh;
    REQUIRE( h2.size() == 3 );
}

TEST_CASE( "transaction task" ) {

    using namespace hamt;

    shared_hash_trie<int> sh;

    sh.update_with([](hash_trie<int> &h) {
        h.insert(1);
        h.insert(2);
        h.insert(10);
    });

    hash_trie<int> h2 = sh;
    REQUIRE( h2.size() == 3 );
}