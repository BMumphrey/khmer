//
// This file is part of khmer, https://github.com/dib-lab/khmer/, and is
// Copyright (C) Michigan State University, 2009-2015. It is licensed under
// the three-clause BSD license; see LICENSE.
// Contact: khmer-project@idyll.org
//

#ifndef HASHBITS_HH
#define HASHBITS_HH

#include <stddef.h>
#include <string.h>
#include <string>
#include <vector>

#include "hashtable.hh"
#include "khmer.hh"
#include "kmer_hash.hh"

namespace khmer
{
class CountingHash;
class LabelHash;

class Hashbits : public khmer::Hashtable
{
protected:
    std::vector<HashIntoType> _tablesizes;
    size_t _n_tables;
    HashIntoType _occupied_bins;
    HashIntoType _n_unique_kmers;
    Byte ** _counts;

    virtual void _allocate_counters()
    {
        _n_tables = _tablesizes.size();

        _counts = new Byte*[_n_tables];

        for (size_t i = 0; i < _n_tables; i++) {
            HashIntoType tablesize = _tablesizes[i];
            HashIntoType tablebytes = tablesize / 8 + 1;

            _counts[i] = new Byte[tablebytes];
            memset(_counts[i], 0, tablebytes);
        }
    }

public:
    Hashbits(WordLength ksize, std::vector<HashIntoType>& tablesizes)
        : khmer::Hashtable(ksize),
          _tablesizes(tablesizes)
    {
        _occupied_bins = 0;
        _n_unique_kmers = 0;

        _allocate_counters();
    }

    ~Hashbits()
    {
        if (_counts) {
            for (size_t i = 0; i < _n_tables; i++) {
                delete[] _counts[i];
                _counts[i] = NULL;
            }
            delete[] _counts;
            _counts = NULL;

            _n_tables = 0;
        }

    }

    // Accessors for protected/private table info members
    std::vector<HashIntoType> get_tablesizes() const
    {
        return _tablesizes;
    }

    const size_t n_tables() const
    {
        return _n_tables;
    }

    virtual void save(std::string);
    virtual void load(std::string);

    // count number of occupied bins
    virtual const HashIntoType n_occupied() const
    {
        return _occupied_bins;
    }

    virtual const HashIntoType n_unique_kmers() const
    {
        return _n_unique_kmers;
    }

    // Get and set the hashbits for the given kmer.
    inline
    virtual
    BoundedCounterType
    test_and_set_bits(const char * kmer)
    {
        HashIntoType hash = _hash(kmer, _ksize);
        return test_and_set_bits(hash);
    }

    // Get and set the hashbits for the given kmer hash.
    // Generally, it is better to keep tests and mutations separate,
    // but, in the interests of efficiency and thread safety,
    // tests and mutations are being blended here against conventional
    // software engineering wisdom.
    inline
    virtual
    BoundedCounterType
    test_and_set_bits( HashIntoType khash )
    {
        bool is_new_kmer = false;

        for (size_t i = 0; i < _n_tables; i++) {
            HashIntoType bin = khash % _tablesizes[i];
            HashIntoType byte = bin / 8;
            unsigned char bit = (unsigned char)(1 << (bin % 8));

            unsigned char bits_orig = __sync_fetch_and_or( *(_counts + i) + byte, bit );
            if (!(bits_orig & bit)) {
                if (i == 0) {
                  __sync_add_and_fetch( &_occupied_bins, 1 );
                }
                is_new_kmer = true;
            }
        } // iteration over hashtables

        if (is_new_kmer) {
            __sync_add_and_fetch( &_n_unique_kmers, 1 );
            return 1; // kmer not seen before
        }

        return 0; // kmer already seen
    } // test_and_set_bits

    virtual void count(const char * kmer)
    {
        HashIntoType hash = _hash(kmer, _ksize);
        count(hash);
    }

    virtual void count(HashIntoType khash)
    {
        test_and_set_bits(khash);
    }

    // get the count for the given k-mer.
    virtual const BoundedCounterType get_count(const char * kmer) const
    {
        HashIntoType hash = _hash(kmer, _ksize);
        return get_count(hash);
    }

    // get the count for the given k-mer hash.
    virtual const BoundedCounterType get_count(HashIntoType khash) const
    {
        for (size_t i = 0; i < _n_tables; i++) {
            HashIntoType bin = khash % _tablesizes[i];
            HashIntoType byte = bin / 8;
            unsigned char bit = bin % 8;

            if (!(_counts[i][byte] & (1 << bit))) {
                return 0;
            }
        }
        return 1;
    }

    void update_from(const Hashbits &other);
};
}

#include "counting.hh"
#include "labelhash.hh"
#endif // HASHBITS_HH

// vim: set sts=2 sw=2:
