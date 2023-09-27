#pragma once

#include <QtGlobal>
#include <qrect.h>
#include <qregion.h>

#include "VipHybridVector.h"


namespace detail
{
	template<int NDim>
	struct Contains
	{
		template<class Rect, class Coord>
		static bool apply(const Rect & r, const Coord & pos)
		{
			for (int i = 0; i < pos.size(); ++i)
				if (pos[i] < r.start(i) || pos[i] >= r.end(i)) return false;
			return true;
		}
	};
	template<>
	struct Contains<1>
	{
		template<class Rect, class Coord>
		static bool apply(const Rect & r, const Coord & pos)
		{
			return pos[0] >= r.start(0) && pos[0] < r.end(0);
		}
	};
	template<>
	struct Contains<2>
	{
		template<class Rect, class Coord>
		static bool apply(const Rect & r, const Coord & pos)
		{
			return pos[0] >= r.start(0) && pos[0] < r.end(0) &&
				pos[1] >= r.start(1) && pos[1] < r.end(1);
		}
	};
	template<>
	struct Contains<3>
	{
		template< class Rect, class Coord>
		static bool apply(const Rect & r, const Coord & pos)
		{
			return pos[0] >= r.start(0) && pos[0] < r.end(0) &&
				pos[1] >= r.start(1) && pos[1] < r.end(1) &&
				pos[2] >= r.start(2) && pos[2] < r.end(2);
		}
	};
}


/// VipNDRect represents a N-dimension rectangle.
/// It is represented as a start and end (excluded) position.
template<int NDim = Vip::None>
class VipNDRect
{
public:
	typedef VipHybridVector<int, NDim> shape_type;

	//! static size if known at compile time, or -1
	static const int static_size = NDim;

	///Default constructor, create an invalid rectangle (isEmpty() == true)
	VipNDRect() {
		m_start.fill(0);
		m_end.fill(0);
	};
	///Copy constructor
	VipNDRect(const VipNDRect & other) :m_start(other.m_start), m_end(other.m_end) {}
	///Construct from a start and end position
	VipNDRect(const shape_type & st, const shape_type & en) : m_start(st), m_end(en) {}

	///Returns true if at least one of the dimensions is empty
	bool isEmpty() const {
		if (m_start.size() == 0 || m_end.size() == 0)
			return true;
		for (int i = 0; i < m_start.size(); ++i)
			if (shape(i) <= 0) return true;
		return false;
	}
	bool isValid() const {
		return (m_start.size() == m_end.size()) && !isEmpty();
	}
	///Returns the number of dimensions
	int size() const { return m_start.size(); }
	///Returns the number of dimensions
	int dimCount() const { return size(); }
	///Returns the full shape size (cumulative multiplication of all shapes)
	int shapeSize() const {
		if (size() == 0) return 0;
		int s = shape(0);
		for (int i = 1; i < size(); ++i)
			s *= shape(i);
		return s;
	}
	///Returns the start position
	const shape_type & start() const { return m_start; }
	///Returns the end of dimensions
	const shape_type & end() const { return m_end; }
	///Returns the shape (end -start)
	shape_type shape() const {
		shape_type res = m_end;
		for (int i = 0; i < res.size(); ++i)
			res[i] -= m_start[i];
		return res;
	}
	///Returns the shape (end -start) for given dimension
	int shape(int index) const { return m_end[index] - m_start[index]; }
	///Returns the start position for given dimension
	int start(int index) const { return m_start[index]; }
	///Returns the end position for given dimension
	int end(int index) const { return m_end[index]; }

	///Moves the start position, leaving the shape unchanged (this might change the end position).
	void moveStart(const shape_type & start) {
		for (int i = 0; i < start.size(); ++i) {
			int w = m_end[i] - m_start[i];
			m_start[i] = start[i];
			m_end[i] = start[i] + w;
		}
	}
	///Moves the start position for given dimension, leaving the shape unchanged (this might change the end position).
	void moveStart(int index, int new_pos) {
		int w = m_end[index] - m_start[index];
		m_start[index] = new_pos;
		m_end[index] = new_pos + w;
	}
	///Set the start position. This might change the shape, but never the end position.
	void setStart(const shape_type & start) {
		m_start = start;
	}
	///Set the start position for given index. This might change the shape, but never the end position.
	void setStart(int index, int new_pos) {
		m_start[index] = new_pos;
	}

	///Moves the end position, leaving the shape unchanged (this might change the start position).
	void moveEnd(const shape_type & end) {
		for (int i = 0; i < end.size(); ++i) {
			int w = m_end[i] - m_start[i];
			m_end[i] = end[i];
			m_start[i] = end[i] - w;
		}
	}
	///Moves the end position for given dimension, leaving the shape unchanged (this might change the start position).
	void moveEnd(int index, int new_pos) {
		int w = m_end[index] - m_start[index];
		m_end[index] = new_pos;
		m_start[index] = new_pos - w;
	}
	///Set the end position. This might change the shape, but never the start position.
	void setEnd(const shape_type & end) {
		m_end = end;
	}
	///Set the end position for given index. This might change the shape, but never the start position.
	void setEnd(int index, int new_pos) {
		m_end[index] = new_pos;
	}

	///Returns a normalized rectangle; i.e., a rectangle that has a non-negative shapes.
	/// If a shape is negative, this function swaps start and end position for given dimension.
	VipNDRect normalized() const {
		VipNDRect res;
		res.resize(size());
		for (int i = 0; i < size(); ++i)
			if (end(i) < start(i)) {
				res.setStart(i,end(i));
				res.setEnd(i, start(i));
			}
			else {
				res.setStart(i, start(i));
				res.setEnd(i, end(i));
			}
		return res;
	}

	///Returns true if the rectangle contains the point \a pos
	template <class Coord>
	bool contains(const Coord & pos) const {
		return detail::Contains<(NDim > 0 ? NDim : Coord::static_size)>::apply(*this, pos);
	}

	///Translate rectangle by a given offset
	void translate(const shape_type & offset) {
		for (int i = 0; i < offset.size(); ++i) {
			m_start += offset[i];
			m_end += offset[i];
		}
	}
	///Returns a translated version of this rectangle
	VipNDRect translated(const shape_type & offset) const {
		VipNDRect res(*this);
		res.translate(offset);
		return res;
	}

	///Returns the intersection of this rectangle with \a rect. Returns an empty rectangle if the rectangles do not intersect.
	VipNDRect intersected(const VipNDRect rect) const {
		VipNDRect res;
		res.resize(size());
		for (int i = 0; i < size(); ++i) {
			if (end(i) <= rect.start(i) || start(i) >= rect.end(i))
				return VipNDRect();
			if (end(i) > rect.start(i)) {
				res.setStart(i, rect.start(i));
				res.setEnd(i, end(i));
			}
			else {
				res.setStart(i, end(i));
				res.setEnd(i, rect.start(i));
			}
		}
		return res;
	}
	///Returns true if \a rect intersects this rectangle
	bool intersects(const VipNDRect rect) const {
		for (int i = 0; i < size(); ++i) {
			if (end(i) <= rect.start(i) || start(i) >= rect.end(i))
				return false;
		}
		return true;
	}

	void resize(int size)
	{
		m_start.resize(size);
		m_end.resize(size);
	}

	///Returns the union of \a rect and this rectangle
	VipNDRect united(const VipNDRect rect) const {
		VipNDRect res;
		res.resize(size());
		for (int i = 0; i < size(); ++i) {
			res.setStart(i, qMin(start(i),rect.start(i)));
			res.setEnd(i, qMax(end(i), rect.end(i)));
		}
		return res;
	}

	///Comparison operator
	template<int Dim>
	bool operator==(const VipNDRect<Dim> & other) const {
		if (size() != other.size()) return false;
		for (int i = 0; i < size(); ++i)
			if (start(i) != other.start(i) || end(i) != other.end(i)) return false;
		return true;
	}
	///Comparison operator
	template<int Dim>
	bool operator!=(const VipNDRect<Dim> & other) const {
		if (size() != other.size()) return true;
		for (int i = 0; i < size(); ++i)
			if (start(i) != other.start(i) || end(i) != other.end(i)) return true;
		return false;
	}
	///Intersection operator
	VipNDRect	operator&(const VipNDRect & rect) const { return this->intersected(rect); }
	///Inplace intersection operator
	VipNDRect &	operator&=(const VipNDRect & rect) {
		*this = this->intersected(rect);
		return *this;
	}
	///Union operator
	VipNDRect	operator|(const VipNDRect & rect) const { return this->united(rect); }
	///Inplace union operator
	VipNDRect &	operator|=(const VipNDRect & rect) {
		*this = this->united(rect);
		return *this;
	}

private:
	shape_type m_start, m_end;

};


/// Specialization for di2, uses a QRect internally
template<>
class VipNDRect<2>
{
public:
	typedef VipHybridVector<int, 2> shape_type;

	//! static size if known at compile time, or -1
	static const int static_size = 2;

	///Default constructor, create an invalid rectangle (isEmpty() == true)
	VipNDRect() {};
	///Copy constructor
	VipNDRect(const VipNDRect & other) :m_rect(other.m_rect) {}
	///Construct from a start and end position
	VipNDRect(const shape_type & st, const shape_type & en) : m_rect(QPoint(st[1], st[0]), QPoint(en[1]-1, en[0]-1)) {}
	VipNDRect(const QRect & r) : m_rect(r) {}

	VipNDRect & operator=(const VipNDRect & other) {
		m_rect = other.m_rect;
		return *this;
	}

	///Returns true if at least one of the dimensions is empty
	bool isEmpty() const {return m_rect.isEmpty();}
	bool isValid() const { return m_rect.isValid(); }
	///Returns the number of dimensions
	int size() const { return 2; }
	///Returns the number of dimensions
	int dimCount() const { return 2; }
	///Returns the full shape size (cumulative multiplication of all shapes)
	int shapeSize() const {return m_rect.width()*m_rect.height();}
	///Returns the start position
	shape_type start() const { return vipVector(m_rect.top(),m_rect.left()); }
	///Returns the end of dimensions
	shape_type end() const { return vipVector(m_rect.bottom()+1, m_rect.right()+1); }
	///Returns the shape (end -start)
	shape_type shape() const { return vipVector(m_rect.height(), m_rect.width()); }
	///Returns the shape (end -start) for given dimension
	int shape(int index) const { return end(index) - start(index); }
	///Returns the start position for given dimension
	int start(int index) const { return ((int*)&m_rect)[(1 - index)] ; }
	///Returns the end position for given dimension
	int end(int index) const { return ((int*)&m_rect)[(1 - index) + 2] + 1; }

	///Moves the start position, leaving the shape unchanged (this might change the end position).
	void moveStart(const shape_type & start) {
		m_rect.moveTopLeft(QPoint(start[1], start[0]));
	}
	///Moves the start position for given dimension, leaving the shape unchanged (this might change the end position).
	void moveStart(int index, int new_pos) {
		if (index == 0) m_rect.moveTop(new_pos);
		else m_rect.moveLeft(new_pos);
	}
	///Set the start position. This might change the shape, but never the end position.
	void setStart(const shape_type & start) {
		m_rect.setTopLeft(QPoint(start[1], start[0]));
	}
	///Set the start position for given index. This might change the shape, but never the end position.
	void setStart(int index, int new_pos) {
		if (index == 0) m_rect.setTop(new_pos);
		else m_rect.setLeft(new_pos);
	}

	///Moves the end position, leaving the shape unchanged (this might change the start position).
	void moveEnd(const shape_type & end) {
		m_rect.moveBottomRight(QPoint(end[1]-1,end[0]-1));
	}
	///Moves the end position for given dimension, leaving the shape unchanged (this might change the start position).
	void moveEnd(int index, int new_pos) {
		if (index == 0) m_rect.moveBottom(new_pos-1);
		else m_rect.moveRight(new_pos-1);
	}
	///Set the end position. This might change the shape, but never the start position.
	void setEnd(const shape_type & end) {
		m_rect.setBottomRight(QPoint(end[1] - 1, end[0] - 1));
	}
	///Set the end position for given index. This might change the shape, but never the start position.
	void setEnd(int index, int new_pos) {
		if (index == 0) m_rect.setBottom(new_pos-1);
		else m_rect.setRight(new_pos-1);
	}

	///Returns a normalized rectangle; i.e., a rectangle that has a non-negative shapes.
	/// If a shape is negative, this function swaps start and end position for given dimension.
	VipNDRect normalized() const { return VipNDRect(m_rect.normalized()); }

	///Returns true if the rectangle contains the point \a pos
	template <class Coord>
	bool contains(const Coord & pos) const { return m_rect.contains(pos[1], pos[0]); }

	///Translate rectangle by a given offset
	void translate(const shape_type & offset) { m_rect.translate(offset[1], offset[0]); }
	///Returns a translated version of this rectangle
	VipNDRect translated(const shape_type & offset) const {
		VipNDRect res(*this);
		res.translate(offset);
		return res;
	}

	///Returns the intersection of this rectangle with \a rect. Returns an empty rectangle if the rectangles do not intersect.
	VipNDRect intersected(const VipNDRect rect) const { return VipNDRect(m_rect.intersected(rect.m_rect));}

	///Returns true if \a rect intersects this rectangle
	bool intersects(const VipNDRect rect) const {return m_rect.intersects(rect.m_rect);}
	void resize(int ) {}

	///Returns the union of \a rect and this rectangle
	VipNDRect united(const VipNDRect rect) const { return VipNDRect(m_rect.united(rect.m_rect));}

	const QRect & rect() const { return m_rect; }

	///Comparison operator
	template<int Dim>
	bool operator==(const VipNDRect<Dim> & other) const {
		if (size() != other.size()) return false;
		for (int i = 0; i < size(); ++i)
			if (start(i) != other.start(i) || end(i) != other.end(i)) return false;
		return true;
	}
	///Comparison operator
	template<int Dim>
	bool operator!=(const VipNDRect<Dim> & other) const {
		if (size() != other.size()) return true;
		for (int i = 0; i < size(); ++i)
			if (start(i) != other.start(i) || end(i) != other.end(i)) return true;
		return false;
	}
	///Intersection operator
	VipNDRect	operator&(const VipNDRect & rect) const { return this->intersected(rect); }
	///Inplace intersection operator
	VipNDRect &	operator&=(const VipNDRect & rect) {
		*this = this->intersected(rect);
		return *this;
	}
	///Union operator
	VipNDRect	operator|(const VipNDRect & rect) const { return this->united(rect); }
	///Inplace union operator
	VipNDRect &	operator|=(const VipNDRect & rect) {
		*this = this->united(rect);
		return *this;
	}

private:
	QRect m_rect;
};





/// @brief Build a VipNDRect from a start and end position
template< int N1, int N2>
VipNDRect < (N1 < 0 ? N2 : N1)> vipRectStartEnd(const VipHybridVector<int, N1> & start, const VipHybridVector<int, N2> & end)
{
	typedef VipNDRect < (N1 < 0 ? N2 : N1)> result;
	return result(start, end);
}

/// @brief Build a VipNDRect from a start position and a shape
template< int N1, int N2>
VipNDRect < (N1 < 0 ? N2 : N1)> vipRectStartShape(const VipHybridVector<int, N1> & start, const VipHybridVector<int, N2> & shape)
{
	typedef VipNDRect < (N1 < 0 ? N2 : N1)> result;
	VipHybridVector<int,(N1 < 0 ? N2 : N1)> end;
	end.resize(start.size());
	for (int i = 0; i < start.size(); ++i)
		end[i] = start[i] + shape[i];
	return result(start, end);
}




