//=======================================================================
// Copyright 2015-2017
// Author: Alex Hagen-Zanker
// University of Surrey
//
// Distributed under the MIT Licence (http://opensource.org/licenses/MIT)
//=======================================================================
//

#pragma once

#include <pronto/raster/access_type.h>
#include <pronto/raster/gdal_block.h>
#include <pronto/raster/gdal_includes.h>
#include <pronto/raster/reference_proxy.h>

#include <algorithm>
#include <cassert>
#include <iterator>
#include <utility>

namespace pronto
{
  namespace raster
  {
    class forward_only_iteration {};
    class random_access_iteration {};

    template<class, class> class gdal_raster_view; // forward declaration

    template<class T, class AccessType, class IterationType = random_access_iteration>
    class gdal_raster_iterator
    {
      using block_type = block<AccessType>;
      using block_iterator_type = typename block_type::iterator;
      using view_type = gdal_raster_view<T, IterationType>;

      // For strictly forward iteration we could use the following and be more efficient
      static const bool is_forward_only = std::is_same<forward_only_iteration, IterationType>::value;
      using proxy_ref = typename std::conditional<is_forward_only
        , reference_proxy<const gdal_raster_iterator&>
        , reference_proxy<gdal_raster_iterator> >::type;


    public:
      using is_mutable = std::integral_constant<bool, AccessType::value == read_write>;

      using reference = typename std::conditional<is_mutable::value,
        proxy_ref, T>::type;
      using value_type = T;
      using difference_type = std::ptrdiff_t;
      using pointer = void;
      using iterator_category = std::input_iterator_tag;

      gdal_raster_iterator()
        : m_block()
        , m_end_of_stretch(m_block.get_null_iterator()) // not so elegant
        , m_pos(m_block.get_null_iterator())            // not so elegant
      {}

      gdal_raster_iterator(const gdal_raster_iterator& other) = default;
      gdal_raster_iterator(gdal_raster_iterator&& other) = default;
      gdal_raster_iterator& operator=(const gdal_raster_iterator& other)
        = default;
      gdal_raster_iterator& operator=(gdal_raster_iterator&& other) = default;
      ~gdal_raster_iterator() = default;

      friend bool operator==(const gdal_raster_iterator& a
        , const gdal_raster_iterator& b)
      {
        return a.m_pos == b.m_pos;
      }

      friend bool operator!=(const gdal_raster_iterator& a
        , const gdal_raster_iterator& b)
      {
        return a.m_pos != b.m_pos;
      }

      gdal_raster_iterator& operator+=(std::ptrdiff_t distance)
      {
        goto_index(get_index() + static_cast<int>(distance));
        return *this;
      }

      gdal_raster_iterator& operator-=(std::ptrdiff_t distance)
      {
        goto_index(get_index() - static_cast<int>(distance));
        return *this;
      }

      gdal_raster_iterator& operator--()
      {
        auto d = std::distance(m_block.get_iterator(0, 0, m_view->m_stride), m_pos);
        if ( d % m_block.block_cols() > 0) {
          m_pos -= m_view->m_stride;
          return *this;
        }
        else
        {
          return goto_index(get_index() - 1);
        }
      }

      gdal_raster_iterator& operator--(int)
      {
        gdal_raster_iterator temp(*this);
        --(*this);
        return temp;
      }

      gdal_raster_iterator operator+(std::ptrdiff_t distance) const
      {
        gdal_raster_iterator temp(*this);
        temp += distance;
        return temp;
      }

      gdal_raster_iterator operator-(std::ptrdiff_t distance) const
      {
        gdal_raster_iterator temp(*this);
        temp -= distance;
        return temp;
      }

      reference operator[](std::ptrdiff_t distance) const
      {
         return *(operator+(distance));
      }

      bool operator<(const gdal_raster_iterator& that) const
      {
        return get_index() < that.get_index();
      }

      bool operator>(const gdal_raster_iterator& that) const
      {
        return get_index() > that.get_index();
      }

      bool operator<=(const gdal_raster_iterator& that) const
      {
        return get_index() <= that.get_index();
      }

      bool operator>=(const gdal_raster_iterator& that) const
      {
        return get_index() >= that.get_index();
      }

      gdal_raster_iterator& operator++()
      {
        m_pos += m_stride;

        if (m_pos == m_end_of_stretch) {
          m_pos -= m_stride;
          return goto_index(get_index() + 1);
        }
        return *this;
      }

      gdal_raster_iterator operator++(int)
      {
        gdal_raster_iterator temp(*this);
        ++(*this);
        return temp;
      }

       reference operator*() const
      {
        return get_reference(is_mutable{});
      }

    private:
      friend class reference_proxy<const gdal_raster_iterator&>;
      friend class reference_proxy<gdal_raster_iterator>;

      T get() const
      {
        return m_view->get(static_cast<void*>(m_pos));
      }

      void put(const T& value) const
      {
        static_assert(AccessType::value == read_write
          , "only allow writing in mutable iterators");
         m_view->put(value, static_cast<void*>(m_pos));
      }

    private:
      friend class gdal_raster_view<T, IterationType>;

      void find_begin(const view_type* view)
      {
        m_view = view;
        m_stride = view->m_stride;
        goto_index(0);
      }

      void find_end(const view_type* view)
      {
        m_view = view;
        m_stride = view->m_stride;
        goto_index(static_cast<long long>(m_view->rows()) * static_cast<long long>(m_view->cols()));
      }

    private:
      reference get_reference(std::true_type) const // mutable
      {
        return reference(*this);
      }

      reference get_reference(std::false_type) const // not mutable
      {
        return m_view->get(static_cast<void*>(m_pos));
      }

      long long get_index() const
      {
        // it might seem more efficient to just add an index member to the
        // iterator, however the hot-path is operator++ and operator*(),
        // keep those as simple as possible

        int block_rows = m_block.block_rows();
        int block_cols = m_block.block_cols();

        int major_row = m_block.major_row();
        int major_col = m_block.major_col();

        block_iterator_type start = m_block.get_iterator(0, 0, m_view->m_stride);

        int index_in_block = static_cast<int>(std::distance(start, m_pos))
          / m_view->m_stride; //  not -1 because m_pos has not been incremented
        assert(index_in_block >= 0);

        int minor_row = index_in_block / block_cols;
        int minor_col = index_in_block % block_cols;

        int gdaldata_row = major_row * block_rows + minor_row;
        int gdaldata_col = major_col * block_cols + minor_col;
        int row = gdaldata_row - m_view->m_first_row;
        int col = gdaldata_col - m_view->m_first_col;

        long long index;
        // in last block? one past the last element?
        if (row == m_view->rows() || col == m_view->cols()) {

          index =  static_cast<long long>(m_view->rows() )* static_cast<long long>(m_view->cols() );
        } else {
          index =  static_cast<long long>(row) * static_cast<long long> (m_view->cols()) + static_cast<long long>(col);
        }
        assert(index <= static_cast<long long>(m_view->rows()) * static_cast<long long>(m_view->cols()));
        return index;
      }

      gdal_raster_iterator& goto_index(long long index)
      {
        if (index == static_cast<long long>(m_view->cols()) * static_cast<long long>(m_view->rows())) {
          if (index == 0) { // empty raster, no place to go
            m_pos = m_block.get_null_iterator();
            return *this;
          }
          // Go to last block, one past the last element.
          goto_index(index - 1);
          m_pos += m_view->m_stride;
          return *this;
        }

        int row = static_cast<int>(index / m_view->cols());
        int col = static_cast<int>(index % m_view->cols());

        int gdaldata_row = row + m_view->m_first_row;
        int gdaldata_col = col + m_view->m_first_col;

        int block_rows = 0;
        int block_cols = 0;

        m_view->m_band->GetBlockSize(&block_cols, &block_rows);

        int block_row = gdaldata_row / block_rows;
        int block_col = gdaldata_col / block_cols;

        int row_in_block = gdaldata_row % block_rows;
        int col_in_block = gdaldata_col % block_cols;

        //int index_in_block = row_in_block * block_cols + col_in_block;

        m_block.reset(m_view->m_band.get(), block_row, block_col);
        if (is_mutable::value && m_view->m_band->GetAccess() == GA_Update)
        {
          m_block.mark_dirty();
        }

        m_pos = m_block.get_iterator(row_in_block, col_in_block, m_view->m_stride);

        int end_col = std::min<int>((block_col + 1) * block_cols
          , m_view->m_first_col + m_view->m_cols);

        int minor_end_col = 1 + (end_col - 1) % block_cols;

        m_end_of_stretch = m_block.get_iterator(row_in_block, minor_end_col,
          m_view->m_stride);

        return *this;
      }

      const view_type* m_view;
      unsigned char m_stride;
      block_type m_block;
      block_iterator_type m_end_of_stretch;
      block_iterator_type m_pos;
    };
  }
}
