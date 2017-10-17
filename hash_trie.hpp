////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// hash_trie - a persistent hash array-mapped trie for c++
//
// https://github.com/philsquared/hash_trie
//
// Copyright (c) 2017, Phil Nash
// All rights reserved.
//
// Distributed under the BSD 2-Clause License. (See accompanying
// file LICENSE)
//

#ifndef HASH_TRIE_HPP_INCLUDED
#define HASH_TRIE_HPP_INCLUDED

#include <cassert>
#include <memory>
#include <functional>
#include <atomic>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// For debugging purposes #define one of the following before #including this header
// -  HAMT_DEBUG_RC
//      This tracks total ref counts and gives each object a unique id which is easier to track
//      in a debugger than a pointer value, which may change between executions
//
// -  HAMT_DEBUG_VERBOSE
//      Automatically gives you the above, plus prints to stdout on each ctor/ dtor/ addref/ release
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(HAMT_DEBUG_VERBOSE) && !defined(HAMT_DEBUG_RC)
#define HAMT_DEBUG_RC
#endif

#ifdef HAMT_DEBUG_RC
#include <iostream>
#include <iomanip>
#endif


// Forward refs
namespace hamt {
    template<typename T> class branch_node;
    template<typename T> class leaf_node;
}

// Default deleters (impls later)
namespace std
{
    template<typename T>
    class default_delete<hamt::branch_node<T>> {
    public:
        void operator()(hamt::branch_node<T> *p);
    };

    template<typename T>
    class default_delete<hamt::leaf_node<T>> {
    public:
        void operator()(hamt::leaf_node<T> *p);
    };
}

namespace hamt {

    namespace detail {

        constexpr int bitsPerChunk = 5;

        constexpr size_t maxDepth = (sizeof(size_t)*8)/bitsPerChunk;
        constexpr size_t chunkMask = (1<<bitsPerChunk)-1;


        // adapted from `http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel`
        // - could be substituted for an assembler instruction if available?
        // See also: `http://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer#109025`
        inline auto count_set_bits( uint32_t i ) {
            i = i - ((i >> 1) & 0x55555555);
            i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
            return static_cast<uint8_t>( ((i + (i >> 4) & 0xF0F0F0F) * 0x1010101) >> 24 );
        }

        inline auto count_set_bits_popcount( unsigned int i ) {
            return __builtin_popcount( i );
        }

        // From http://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key#12996028
        inline auto rehash( uint64_t x ) {
            x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
            x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
            x = x ^ (x >> 31);
            return x;
        }

        struct chunked_hash {
            size_t hash;
            size_t shiftedHash;
            size_t chunk;

            explicit chunked_hash( size_t hash )
            :   hash( hash ),
                shiftedHash( hash ),
                chunk( shiftedHash & chunkMask )
            {}

            chunked_hash& operator ++() {
                shiftedHash >>= bitsPerChunk;
                chunk = shiftedHash & chunkMask;
                return *this;
            }
            chunked_hash& operator += ( int chunks ) {
                shiftedHash >>= (bitsPerChunk*chunks);
                chunk = shiftedHash & chunkMask;
                return *this;
            }
            chunked_hash operator + ( int chunks ) {
                chunked_hash newChunkedHash( *this );
                newChunkedHash += chunks;
                return newChunkedHash;
            }
        };

    } // namespace detail


    // "Strong typedef" for unsigned int that represents a compact index into the physical
    // backing array of a sparse array
    class compact_index {
        size_t m_value;
    public:
        explicit compact_index( size_t value ) : m_value( value ) {}

        auto value() const { return m_value; }
    };

    // "Strong typedef" for unsigned int that represents an index into a sparse array,
    // along with a method for obtaining the compact index, given a bitmap
    class sparse_index {
        size_t m_value;
    public:
        explicit sparse_index( size_t value ) : m_value( value )  {}

        auto value() const { return m_value; }
        auto bit_position() const { return 1 << m_value; }

        auto toCompact( size_t bitmap ) const {
            auto lowMask = bit_position()-1;
            return compact_index( detail::count_set_bits_popcount( static_cast<unsigned int>( bitmap & lowMask ) ) );
        }
    };

    enum class node_type { branch, leaf };

    class node {
    public:
        mutable std::atomic<size_t> m_refCount { 1 };
        node_type m_type;

        node() = delete;
        node( node const& ) = delete;
        node( node&& ) = delete;
        node& operator=( node const& ) = delete;
        node& operator=( node&& ) = delete;

    public:
#ifdef HAMT_DEBUG_RC
        static auto dbg_get_total_refs() -> size_t& {
            static size_t s_dbgTotalRefs = 0;
            return s_dbgTotalRefs;
        }
#endif

#ifdef HAMT_DEBUG_VERBOSE
        void dbg_addref(char const *type, size_t refCount) const {
            dbg_get_total_refs()++;
            dbgPrintRefs(type, refCount);
        }

        void dbg_release(size_t refCount) const {
            dbg_get_total_refs()--;
            dbgPrintRefs("--", refCount - 1);
            assert(refCount > 0);
        }
#elif defined(HAMT_DEBUG_RC)
        void dbg_addref(char const *type, size_t refCount) const {
            dbg_get_total_refs()++;
        }

        void dbg_release(size_t refCount) const {
            dbg_get_total_refs()--;
            assert(refCount > 0);
        }
#else
        void dbg_addref(char const *, size_t) const {}
        void dbg_release(size_t) const {}
#endif

    protected:

#ifdef HAMT_DEBUG_VERBOSE
        int m_id;
        static auto dbg_get_next_id() -> int {
            static int s_nextId = 0;
            return s_nextId++;
        }

        auto typeName() const -> char const* {
            switch( m_type ) {
                case node_type::branch: return "branch";
                case node_type::leaf: return "leaf";
            }
        }

        explicit node( node_type type ): m_type( type ), m_id(dbg_get_next_id()) {
            std::cout << m_id << " @ 0x" << std::hex << (unsigned long)this << " " << typeName() << "()" << std::endl;
            dbg_addref("==", 1);
        }
        ~node() {
            std::cout << m_id << " @ 0x" << std::hex << (unsigned long)this << " ~" << typeName() << "()" << std::endl;
        }

        void dbgPrintRefs(char const *type, size_t refCount) const {
            std::cout << m_id << " @ 0x" << std::hex << (unsigned long)this << " " << type << " " << refCount << "/ " << dbg_get_total_refs() << std::endl;
        }

#elif defined(HAMT_DEBUG_RC)
        explicit node( node_type type ): m_type( type ) {
            dbg_addref("==", 1);
        }
        ~node() = default;
#else
        explicit node( node_type type ) : m_type( type ) {}
        ~node() = default;
#endif
    };


    inline void addref(node const *p) {

        std::atomic_fetch_add_explicit (&p->m_refCount, size_t(1), std::memory_order_relaxed);
        p->dbg_addref("++", p->m_refCount.load( std::memory_order_relaxed ) );
    }

    template<typename NodeT>
    inline void release( NodeT const* p ) {
        p->dbg_release( p->m_refCount.load( std::memory_order_relaxed ) );

        if( std::atomic_fetch_sub_explicit (&p->m_refCount, size_t(1), std::memory_order_release) == 1 ) {
            std::atomic_thread_fence( std::memory_order_acquire );
            std::default_delete<NodeT>()( const_cast<NodeT *>( p ) );
        }
    }

    template<typename T>
    class leaf_node : public node { // NOLINT
        friend class std::default_delete<leaf_node>;

        size_t m_size;
        size_t m_hash;
        union { ;
            T m_values[1];
        };

        leaf_node( size_t size, size_t hash )
        :   node( node_type::leaf ),
            m_size( size ),
            m_hash( hash )
        {}

        ~leaf_node() {
            for( size_t i=0; i < m_size; ++i )
                m_values[i].~T();
        }

        // Calculates the raw storage size for a leaf type that
        // contains size elements in the array
        static constexpr auto storage_size(size_t size) {
            return sizeof(leaf_node) + sizeof(T*[size-1]);
        }

        // Creates a new leaf_node type with enough additional storage for
        // size items - but does not populate the array
        static auto create_unpopulated( size_t size, size_t hash ) {
            assert( size >=1 );
            auto temp = std::make_unique<unsigned char[]>(storage_size(size) );
            auto leaf_ptr = new(temp.get()) leaf_node( size, hash );
            temp.release();
            return std::unique_ptr<leaf_node>( leaf_ptr );
        }

    public:

        auto hash() const { return m_hash; }
        auto size() const { return m_size; }

        template<typename U>
        static auto create( U &&value, size_t hash ) -> std::unique_ptr<leaf_node> {
            auto leaf = create_unpopulated(1, hash);
            new (&leaf->m_values[0]) T( std::forward<U>( value ) );
            return leaf;
        }

        template<typename U>
        auto with_appended_value(U &&newValue) const {
            auto newLeaf = create_unpopulated(m_size + 1, m_hash);
            for( size_t i=0; i < m_size; ++i )
                new (&newLeaf->m_values[i]) T( m_values[i] );
            new (&newLeaf->m_values[m_size]) T( std::forward<U>( newValue ) );
            return newLeaf;
        }

        auto find( T const& value ) const -> T const* {
            for( size_t i=0; i < m_size; ++i )
                if( m_values[i] == value )
                    return &m_values[i];
            return nullptr;
        }

        auto get_at(size_t index) const -> T const& {
            assert( index < m_size );
            return m_values[index];
        }
    };


    template<typename T>
    class branch_node : public node { // NOLINT
        friend class std::default_delete<branch_node>;

        size_t m_size;
        size_t m_bitmap; // set bits indicate the indexed element is a branch or leaf value

        union {
            node const *m_children[1];
        };


        explicit branch_node( size_t size, size_t bitmap ) // NOLINT
        :   node( node_type::branch ),
            m_size( size ),
            m_bitmap( bitmap )
        {}

        ~branch_node() {
            auto len = size();
            for( size_t i = 0; i < len; ++i ) {
                auto node = m_children[i];
                if( node->m_type == node_type::branch )
                    release( static_cast<branch_node<T> const*>( node ) );
                else
                    release( static_cast<leaf_node<T> const*>( node ) );
            }
        }

        // Calculates the raw storage size for a node type that
        // contains size elements in the array
        static constexpr auto storage_size(size_t size) {
            return sizeof(branch_node) + sizeof(node*)*(size-1);
        }

        // Creates a new branch_node type with enough additional storage for
        // size items - but does not populate the array
        static auto create_unpopulated( size_t size, size_t bitmap ) {
            assert( size <= 32 );
            auto temp = std::make_unique<unsigned char[]>(storage_size(size) );

            auto node_ptr = new(temp.get()) branch_node( size, bitmap );
            temp.release();
            return std::unique_ptr<branch_node>( node_ptr );
        }

    public:
        static auto create_empty() -> std::unique_ptr<branch_node> {
            auto node = create_unpopulated( 1, 0 );
            node->m_size = 0;
            return node;
        }

        static auto create_single(sparse_index index, node const *child) -> std::unique_ptr<branch_node> {
            auto node = create_unpopulated( 1, static_cast<size_t>( index.bit_position() ) );
            node->m_children[0] = child;
            return node;
        }

        static auto create_pair(sparse_index index1, leaf_node<T> const *leaf1, sparse_index index2,
                                leaf_node<T> const *leaf2) -> std::unique_ptr<branch_node> {
            auto bitmap = static_cast<size_t>( index1.bit_position() | index2.bit_position() );
            auto node = create_unpopulated( 2, bitmap );
            auto children = &node->m_children[0];
            if( index1.value() >  index2.value() ) {
                children[0] = leaf2;
                children[1] = leaf1;
            }
            else {
                children[0] = leaf1;
                children[1] = leaf2;
            }
            return node;
        }

        auto with_inserted(sparse_index sparseIndex, node const *child) const -> std::unique_ptr<branch_node> {
            auto originalSize = size();
            auto bitmap = m_bitmap | sparseIndex.bit_position();

            // If adding new we need to offset later nodes
            assert( ( m_bitmap & sparseIndex.bit_position() ) == 0 );

            auto node = create_unpopulated( originalSize + 1, bitmap );

            auto splitPoint = sparseIndex.toCompact( m_bitmap ).value();

            for( size_t i = 0; i < splitPoint; ++i ) {
                auto sharedNode = node->m_children[i] = m_children[i];
                addref(sharedNode);
            }

            node->m_children[splitPoint] = child;

            for( size_t i = splitPoint; i < originalSize; ++i ) {
                auto sharedNode = node->m_children[i+1] = m_children[i];
                addref(sharedNode);
            }
            return node;
        }

        auto with_replaced(sparse_index sparseIndex, node const *child) const -> std::unique_ptr<branch_node> {
            auto originalSize = size();
            auto bitmap = m_bitmap | sparseIndex.bit_position();

            // If replacing a node we overwrite existing in place
            assert( ( m_bitmap & sparseIndex.bit_position() ) != 0 );

            auto node = create_unpopulated( originalSize, bitmap );

            auto splitPoint = sparseIndex.toCompact( m_bitmap ).value();

            for( size_t i = 0; i < splitPoint; ++i ) {
                auto sharedNode = node->m_children[i] = m_children[i];
                addref(sharedNode);
            }

            node->m_children[splitPoint] = child;

            for( size_t i = splitPoint+1; i < originalSize; ++i ) {
                auto sharedNode = node->m_children[i] = m_children[i];
                addref(sharedNode);
            }
            return node;
        }

        auto size() const {
            assert( m_size == detail::count_set_bits( static_cast<uint32_t>( m_bitmap ) ) );
            return m_size;
        }

        auto get_at(compact_index compactIndex) const {
            return m_children[compactIndex.value()];
        }
        auto get_at(sparse_index sparseIndex) const -> node const* {
            if( ( m_bitmap & sparseIndex.bit_position() ) == 0 )
                return nullptr;
            return get_at(sparseIndex.toCompact(m_bitmap));
        }
    };


    // TBD: This is just a start - not std compliant yet
    template<typename T>
    class iterator {
        struct Level {
            branch_node<T> const *branch;
            size_t compactIndex;
            size_t width;
        };
        Level m_levels[detail::maxDepth];

        leaf_node<T> const* m_leaf;

        size_t m_depth;

        void descend_from(branch_node<T> const *branch, size_t depth) {
            leaf_node<T> const* leaf = nullptr;
            while( !leaf ) {
                assert( branch->size() > 0 );
                m_levels[depth++] = { branch, 0, branch->size() };
                auto nextNode = branch->get_at(compact_index(0));
                assert( nextNode );
                if( nextNode->m_type == node_type::leaf )
                    leaf = static_cast<leaf_node<T> const*>( nextNode );
                else {
                    assert( branch->size() > 1 ); // If this is a branch with a non-leaf child it must have at least two children in total
                    branch = static_cast<branch_node<T> const*>( nextNode );
                }
            };
            m_leaf = leaf;
            m_depth = depth;
            assert( m_leaf->size() == 1 );
        }
    public:
        explicit iterator( branch_node<T> const* root ) {
            if( root ) {
                descend_from(root, 0);
            }
            else {
                m_depth = 0;
                m_leaf = nullptr;
            }
        }

        auto operator==( iterator const& other ) const -> bool {
            return m_leaf == other.m_leaf;
        }
        auto operator!=( iterator const& other ) const -> bool {
            return m_leaf != other.m_leaf;
        }
        auto operator++() -> iterator& {
            auto& level = m_levels[m_depth-1];

            // !TBD: If multiple values, iterate those first

            if( ++level.compactIndex == level.width ) {
                if( --m_depth > 0 ) {
                    return operator++();
                }
                else { // NOLINT
                    m_leaf = nullptr;
                    return *this;
                }
            }
            auto nextNode = level.branch->get_at(compact_index(level.compactIndex));
            assert( nextNode );
            if( nextNode->m_type == node_type::leaf )
                m_leaf = static_cast<leaf_node<T> const*>( nextNode );
            else
                descend_from(static_cast<branch_node<T> const *>( nextNode ), m_depth);

            return *this;
        }

        auto operator *() const {
            // !TBD: current value
            return m_leaf->get_at(0);
        }
    };


    // Traces a path of branch_nodes and hash chunks down to either a leaf_node
    // that matches the given hash, or the last matching branch_node.
    template<typename T>
    class path {

        branch_node<T> const *m_branches[detail::maxDepth];
        size_t m_chunks[detail::maxDepth];

        branch_node<T> const *m_lastBranch;
        detail::chunked_hash m_chunkedHash;
        leaf_node<T> const* m_leaf;
        size_t m_size = 0;

    public:
        path( T const& value, branch_node<T> const* root ) : m_chunkedHash( std::hash<T>()( value ) ) { // NOLINT
            size_t size = 0;
            assert( root != nullptr );
            auto lastBranch = root;
            auto chunk = m_chunkedHash.chunk;
            auto nextNode = lastBranch->get_at( sparse_index( chunk ) );

            while( nextNode && nextNode->m_type == node_type::branch ) {
                m_branches[size] = lastBranch;
                m_chunks[size] = chunk;

                lastBranch = static_cast<branch_node<T> const*>( nextNode );

                ++size;
                ++m_chunkedHash;

                chunk = m_chunkedHash.chunk;
                nextNode = lastBranch->get_at( sparse_index( chunk ) );
            };

            assert( size <= detail::maxDepth );

            m_leaf = static_cast<leaf_node<T> const*>( nextNode );
            m_lastBranch = lastBranch;
            m_size = size;
        }

        auto size() const -> size_t { return m_size; }
        auto last_branch() const { return m_lastBranch; }
        auto leaf() const { return m_leaf; }
        auto whole_hash() const { return m_chunkedHash.hash; }
        auto hash_chunk() const { return m_chunkedHash.chunk; }
        auto chunked_hash() const { return m_chunkedHash; }

        auto rewrite( branch_node<T> const* newBranch ) const -> branch_node<T> const* {
            auto currentBranch = newBranch;

            for( auto i = m_size; i > 0; --i ) {
                auto parent = m_branches[i - 1]->with_replaced(sparse_index(m_chunks[i - 1]), currentBranch);
                currentBranch = parent.release();
            }
            return currentBranch;
        }
    };

    template<typename T>
    struct hash_trie_data {
        branch_node<T> const* m_root;
        size_t m_size;
    };

    template<typename T>
    auto add_value_at_currently_unset_position(path<T> const &path, std::unique_ptr<leaf_node<T>> &&leaf) {
        auto newBranch = path.last_branch()->with_inserted(sparse_index(path.hash_chunk()), leaf.get());
        leaf.release();
        auto newRoot = path.rewrite( newBranch.get() );
        newBranch.release();
        return newRoot;
    }

    template<typename T>
    auto extend
            (   detail::chunked_hash existingHash,
                leaf_node<T> const *existingLeaf,
                detail::chunked_hash newHash,
                std::unique_ptr<leaf_node<T>> &&newLeaf ) -> std::unique_ptr<branch_node<T>> {
        if( existingHash.chunk == newHash.chunk ) {
            auto newChildBranch = extend(existingHash + 1, existingLeaf, newHash + 1, std::move(newLeaf));
            auto newParentBranch = branch_node<T>::create_single(sparse_index(newHash.chunk), newChildBranch.get());
            newChildBranch.release();
            return newParentBranch;
        }
        else { // NOLINT
            auto newBranch = branch_node<T>::create_pair(sparse_index(existingHash.chunk), existingLeaf,
                                                         sparse_index(newHash.chunk), newLeaf.get());
            newLeaf.release();
            addref(existingLeaf);
            return newBranch;
        }
    }

    template<typename U, typename T>
    static auto add_value_at_leaf(path<T> const &path, U &&value) -> branch_node<T> const* {
        auto existingLeaf = path.leaf();
        // If the value already exists we're done
        if( existingLeaf->find( value )) {
            return nullptr;
        }

        detail::chunked_hash existingHash( existingLeaf->hash() );

        // If hash matches then add an extra value to the existing leaf node
        if( existingHash.hash == path.whole_hash() ) {
            auto newLeaf = existingLeaf->with_appended_value(value);
            auto newBranch = path.last_branch()->with_replaced
                    (sparse_index(path.hash_chunk()),
                     newLeaf.get());

            newLeaf.release();
            auto newRoot = path.rewrite( newBranch.get());
            newBranch.release();
            return newRoot;
        }

        // Different hash, so add branches to the point they diverge
        else { // NOLINT
            existingHash += path.size();
            auto newChildBranch = extend
                    ( existingHash + 1,
                      existingLeaf,
                      path.chunked_hash() + 1,
                      leaf_node<T>::create(std::forward<U>(value), path.whole_hash()));
            auto newBranch = path.last_branch()->with_replaced
                    (sparse_index(path.hash_chunk()),
                     newChildBranch.get());
            auto newRoot = path.rewrite(newBranch.get());
            newChildBranch.release();
            newBranch.release();
            return newRoot;
        }
    }

    template<typename U, typename T>
    static auto inserted( branch_node<T> const* root, U &&value) -> branch_node<T> const* {
        static_assert( std::is_constructible<T, decltype(std::forward<U>(value))>::value, "value must be convertible to T" );

        path<T> path( value, root );

        return path.leaf()
               ? add_value_at_leaf( path, std::forward<U>(value) )
               : add_value_at_currently_unset_position
                       ( path, leaf_node<T>::create(std::forward<U>(value), path.whole_hash() ) );
    }

    template<typename T>
    class shared_hash_trie;

    template<typename T>
    class hash_trie {

        hash_trie_data<T> m_data;
        friend shared_hash_trie<T>;

        static auto makeEmptyData() -> hash_trie_data<T> {
            return { branch_node<T>::create_empty().release(), 0 };
        }

    public:
        hash_trie() : m_data( makeEmptyData() ) {}

        explicit hash_trie( hash_trie_data<T> const& data ) : m_data( data ) {
            addref( m_data.m_root );
        }
        hash_trie( hash_trie<T> const& other ) : hash_trie( other.m_data ) {}
        hash_trie( hash_trie<T>&& other ) noexcept(false) : hash_trie() {
            swap( other );
        }

        ~hash_trie() {
            release( m_data.m_root );
        }

        hash_trie& operator = ( hash_trie const& other ) {
            hash_trie temp( other );
            swap( temp );
            return *this;
        }
        hash_trie& operator = ( hash_trie&& other ) noexcept(false) {
            if( !empty() ) {
                hash_trie emptyTemp;
                swap( emptyTemp );
            }
            swap( other );
            return *this;
        }

        void swap( hash_trie& other ) noexcept {
            std::swap( m_data.m_root, other.m_data.m_root );
            std::swap( m_data.m_size, other.m_data.m_size );
        }

        auto size() const -> size_t { return m_data.m_size; }
        auto empty() const -> bool { return size() == 0; }

        auto find( T const& value ) const {
            return path<T>( value, m_data.m_root );
        }

        template<typename U>
        auto insert( U &&value ) {
            if( auto newRoot = inserted( m_data.m_root, std::forward<U>(value) ) ) {
                release( m_data.m_root );
                m_data = { newRoot, size()+1 };
            }
        }

        auto begin() -> iterator<T> {
            return iterator<T>( m_data.m_root );
        }
        auto end() -> iterator<T> {
            return iterator<T>( nullptr );
        }

        auto data() const -> hash_trie_data<T> const& { return m_data; }
        auto data() -> hash_trie_data<T>& { return m_data; }
    };

    template<typename T>
    class hash_trie_transaction;

    template<typename T>
    class shared_hash_trie { // NOLINT
        static_assert( std::is_trivially_copyable<hash_trie_data<T>>::value,
                       "hash_trie_data must be trivially copyable to be used atomically" );

        std::atomic<hash_trie_data<T>> m_data;

        static auto makeEmptyData() -> hash_trie_data<T> {
            return { branch_node<T>::create_empty().release(), 0 };
        }

    public:
        shared_hash_trie& operator = ( shared_hash_trie const& ) = delete;
        shared_hash_trie& operator = ( shared_hash_trie&& ) = delete;

        shared_hash_trie() : m_data( makeEmptyData() ) {}

        explicit shared_hash_trie( hash_trie<T> const& hash_trie ) {
            m_data.store( hash_trie.data(), std::memory_order_relaxed );
            addref( hash_trie.data().m_root );
        }

        auto data() const -> hash_trie_data<T> {
            return m_data.load( std::memory_order_relaxed );
        }

        auto get() const -> hash_trie<T> {
            return hash_trie<T>( data() );
        }

        auto start_transaction() -> hash_trie_transaction<T>;

        template<typename L>
        void update_with(L const &updateTask);

        // "low level" compare-exchange wrapper - use transaction
        auto reset( hash_trie_data<T>& originalData,
                    hash_trie_data<T>& newData ) -> bool {
            if( !m_data.compare_exchange_strong
                    ( originalData, newData,
                      std::memory_order_release,
                      std::memory_order_relaxed ) )
                return false;

            release( originalData.m_root );
            addref( newData.m_root );
            return true;
        }

        auto is_lock_free() const { return m_data.is_lock_free(); }
    };

    template<typename T>
    class hash_trie_transaction {
        hash_trie_data<T> m_baseData; // For compare-exchange
        shared_hash_trie<T>& m_shared;

    public:
        explicit hash_trie_transaction( shared_hash_trie<T>& shared )
        : m_baseData( shared.data() ),
          m_shared( shared )
        {
            addref( m_baseData.m_root );
        }

        auto get() const -> hash_trie<T> {
            return hash_trie<T>( m_baseData );
        }

        auto try_commit(hash_trie<T> &newHashTrie) -> bool {
            return m_shared.reset( m_baseData, newHashTrie.data() );
        }

        template<typename L>
        void update_with(L const &updateTask) {
            while( true ) {
                hash_trie<T> copy( m_baseData );
                updateTask( copy );

                // If we didn't change, don't do anything
                if( copy.data().m_root == m_baseData.m_root )
                    break;

                // try to commit, and if successful we're done
                if(try_commit(copy) )
                    break;

                // m_baseData has been updated with new base
            };
        }
    };

    template<typename T>
    auto shared_hash_trie<T>::start_transaction() -> hash_trie_transaction<T> {
        return hash_trie_transaction<T>( *this );
    }
    template<typename T>
    template<typename L>
    void shared_hash_trie<T>::update_with(L const &updateTask) {
        auto trans = start_transaction();
        trans.update_with(updateTask);
    }

}

namespace std // NOLINT
{
    template<typename T>
    void default_delete<hamt::branch_node<T>>::operator()( hamt::branch_node<T> *p ) {
        p->~branch_node();
        auto rawStorage = reinterpret_cast<unsigned char*>( p ); // NOLINT

        delete[] rawStorage;
    }

    template<typename T>
    void default_delete<hamt::leaf_node<T>>::operator()( hamt::leaf_node<T> *p ) {
        p->~leaf_node();
        auto rawStorage = reinterpret_cast<unsigned char*>( p ); // NOLINT
        delete[] rawStorage;
    }
}

#endif // HASH_TRIE_HPP_INCLUDED
