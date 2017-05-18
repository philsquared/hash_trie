#include "hash_trie.hpp"

#include "catch.hpp"

TEST_CASE( "chunked hash" ) {
    using namespace hamt::detail;

    SECTION( "1") {
        chunked_hash ch( 1 );
        CHECK( ch.shiftedHash == 1 );
        CHECK( ch.chunk == 1 );
        ++ch;
        CHECK( ch.shiftedHash == 0 );
        CHECK( ch.chunk == 0);
    }
    SECTION( "00001'00001") {
        chunked_hash ch( 0b00001'00001 );
        CHECK( ch.shiftedHash == 0b00001'00001 );
        CHECK( ch.chunk == 1 );
        ++ch;
        CHECK( ch.shiftedHash == 1 );
        CHECK( ch.chunk == 1);
        ++ch;
        CHECK( ch.shiftedHash == 0 );
        CHECK( ch.chunk == 0 );
        CHECK( ch.hash == 0b00001'00001 );
    }
}

TEST_CASE("explicit nodes") {
    using namespace hamt;

    auto leaf = leaf_node<int>::create(42, std::hash<int>()(42));
    auto v = branch_node<int>::create_single(sparse_index(1), leaf.get());
    leaf.release();
    CHECK( v->size() == 1 );

    auto n1 = branch_node<int>::create_single(sparse_index(5), v.release());
    CHECK( n1->size() == 1 );

    auto leaf7 = leaf_node<int>::create(7, std::hash<int>()(7));
    auto n2 = n1->branch_node<int>::with_inserted(sparse_index(3), leaf7.get());
    leaf7.release();

    CHECK( n2->size() == 2 );

    auto result3 = n2->get_at(sparse_index(3));
    CHECK( result3->m_type == node_type::leaf );
    CHECK(static_cast<leaf_node<int> const *>( result3 )->get_at(0) == 7 );

    auto result5 = n2->get_at(sparse_index(5));
    CHECK( result5->m_type == node_type::branch );

    auto b = static_cast<branch_node<int> const*>( result5 );
    auto result1 = b->get_at(sparse_index(1));
    CHECK( result1->m_type == node_type::leaf );
    CHECK(static_cast<leaf_node<int> const *>( result1 )->get_at(0) == 42 );
}