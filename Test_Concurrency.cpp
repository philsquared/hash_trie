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

    hash_trie<int> h = trans.get();

    h.insert( 1 );
    h.insert( 2 );
    h.insert( 10 );

    REQUIRE( sh.get().size() == 0 );

    trans.try_commit(h);

    hash_trie<int> h2 = sh.get();
    REQUIRE( h2.size() == 3 );
}

TEST_CASE( "concurrent commit" ) {

    using namespace hamt;

    shared_hash_trie<int> sh;

    // Start first transaction
    auto trans1 = sh.start_transaction();
    hash_trie<int> h1 = trans1.get();

    // Start second transaction - they have the same base
    auto trans2 = sh.start_transaction();
    hash_trie<int> h2 = trans2.get();

    // Update first copy
    h1.insert( 1 );
    h1.insert( 2 );
    h1.insert( 10 );

    // Update second copy
    h2.insert( 3 );
    h2.insert( 4 );
    h2.insert( 10 );

    REQUIRE( sh.get().size() == 0 );

    // Commit first transaction - this should succeed
    REQUIRE(trans1.try_commit(h1) == true );

    // Attempt to commit second transaction - this should fail
    REQUIRE(trans2.try_commit(h2) == false );

    h2 = trans2.get(); // Rebase

    // Update second copy again
    h2.insert( 3 );
    h2.insert( 4 );
    h2.insert( 10 );

    // committing second transaction should now work
    REQUIRE(trans2.try_commit(h2) == true );

    hash_trie<int> h = sh.get();
    REQUIRE( h.size() == 5 );
}

TEST_CASE( "transaction task" ) {

    using namespace hamt;

    shared_hash_trie<int> sh;

    sh.update_with([](hash_trie<int> &h) {
        h.insert(1);
        h.insert(2);
        h.insert(10);
    });

    hash_trie<int> h2 = sh.get();
    REQUIRE( h2.size() == 3 );
}