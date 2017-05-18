#include "hash_trie.hpp"

#include "catch.hpp"

// These tests are only valid in a debug build
#ifdef HAMT_DEBUG_RC

TEST_CASE("ref counts") {
    using namespace hamt;

    node::dbg_get_total_refs() = 0;
    {
        hash_trie<int> h;

        CHECK( h.size() == 0 );
        CHECK(node::dbg_get_total_refs() == 1 );

        h.insert(42);

        CHECK( h.size() == 1 );
        CHECK(node::dbg_get_total_refs() == 2 );

        h.insert(42);

        CHECK( h.size() == 1 );
        CHECK(node::dbg_get_total_refs() == 2 );

        h.insert(7);

        CHECK( h.size() == 2 );
        CHECK(node::dbg_get_total_refs() == 3 );
    }
    CHECK(node::dbg_get_total_refs() == 0 );
}

TEST_CASE( "hash patterns" ) {
    using namespace hamt;

    node::dbg_get_total_refs() = 0;
    {
        hash_trie<int> h;

        h.insert(0b01000'00010'00001);
        h.insert(0b00100'00010'00001); // differs only in third hash chunk
    }
    CHECK(node::dbg_get_total_refs() == 0 );
}

#endif