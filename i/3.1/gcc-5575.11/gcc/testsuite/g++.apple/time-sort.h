
/*  synopsis

class universal_time
{
public:
universal_time();  // current time
universal_time& operator += (const elapsed_time& dt);
universal_time& operator -= (const elapsed_time& dt);

std::time_t  sec_;
std::int32_t nsec_;
};

class elapsed_time
{
 public:
  explicit elapsed_time(std::time_t sec = 0, std::int32_t nsec = 0);

  elapsed_time& operator += (const elapsed_time& dt);
  elapsed_time& operator -= (const elapsed_time& dt);
  elapsed_time operator - () const;
  elapsed_time operator + () const;

  std::time_t  sec_;
  std::int32_t nsec_;
};

elapsed_time   operator-(const universal_time& t1, const universal_time& t2);
universal_time operator-(const universal_time& t1, const elapsed_time& dt);
elapsed_time   operator-(const elapsed_time& dt1, const elapsed_time& dt2);

universal_time operator+(const universal_time& t1, const elapsed_time& dt);
universal_time operator+(const elapsed_time& dt, const universal_time& t1);
elapsed_time   operator+(const elapsed_time& dt1, const elapsed_time& dt2);

bool operator == (const universal_time& x, const universal_time& y);
bool operator != (const universal_time& x, const universal_time& y);
bool operator <  (const universal_time& x, const universal_time& y);
bool operator <= (const universal_time& x, const universal_time& y);
bool operator >  (const universal_time& x, const universal_time& y);
bool operator >= (const universal_time& x, const universal_time& y);

bool operator == (const elapsed_time& x, const elapsed_time& y);
bool operator != (const elapsed_time& x, const elapsed_time& y);
bool operator <  (const elapsed_time& x, const elapsed_time& y);
bool operator <= (const elapsed_time& x, const elapsed_time& y);
bool operator >  (const elapsed_time& x, const elapsed_time& y);
bool operator >= (const elapsed_time& x, const elapsed_time& y);

*/

#ifndef HTIMER
#define HTIMER

#include <time.h>
#include <stdint.h>

class elapsed_time;

class universal_time
{
 public:
  universal_time();  // current time
  universal_time& operator += (const elapsed_time& dt);
  universal_time& operator -= (const elapsed_time& dt);

  time_t  sec_;
  int32_t nsec_;
};

class elapsed_time
{
 public:
  explicit elapsed_time(time_t sec = 0, int32_t nsec = 0)
    : sec_(sec), nsec_(nsec) {}
  elapsed_time& operator += (const elapsed_time& dt);
  elapsed_time& operator -= (const elapsed_time& dt);
  elapsed_time operator - () const {return elapsed_time(-sec_, -nsec_);}
  elapsed_time operator + () const {return *this;}

  time_t  sec_;
  int32_t nsec_;
};

inline
elapsed_time
operator-(const universal_time& t1, const universal_time& t2)
{
  elapsed_time t(t1.sec_, t1.nsec_);
  t -= (const elapsed_time&)t2;
  return t;
}

inline
universal_time
operator-(const universal_time& t1, const elapsed_time& dt)
{
  universal_time t(t1);
  t -= dt;
  return t;

}

inline
elapsed_time
operator-(const elapsed_time& dt1, const elapsed_time& dt2)
{
  elapsed_time t(dt1);
  t -= dt2;
  return t;
}

inline
universal_time
operator+(const universal_time& t1, const elapsed_time& dt)
{
  universal_time t(t1);
  t += dt;
  return t;
}

inline
universal_time
operator+(const elapsed_time& dt, const universal_time& t1)
{
  universal_time t(t1);
  t += dt;
  return t;
}

inline
elapsed_time
operator+(const elapsed_time& dt1, const elapsed_time& dt2)
{
  elapsed_time t(dt1);
  t += dt2;
  return t;
}

inline
bool
operator == (const universal_time& x, const universal_time& y)
{
  return x.sec_ == y.sec_ && x.nsec_ == y.nsec_;
}

inline
bool
operator != (const universal_time& x, const universal_time& y)
{
  return !(x == y);
}

inline
bool
operator <  (const universal_time& x, const universal_time& y)
{
  return x.sec_ < y.sec_ || (x.sec_ == y.sec_ && x.nsec_ < y.nsec_);
}

inline
bool
operator <= (const universal_time& x, const universal_time& y)
{
  return !(y < x);
}

inline
bool
operator >  (const universal_time& x, const universal_time& y)
{
  return y < x;
}

inline
bool
operator >= (const universal_time& x, const universal_time& y)
{
  return !(x < y);
}

inline
bool
operator == (const elapsed_time& x, const elapsed_time& y)
{
  return x.sec_ == y.sec_ && x.nsec_ == y.nsec_;
}

inline
bool
operator != (const elapsed_time& x, const elapsed_time& y)
{
  return !(x == y);
}

inline
bool
operator <  (const elapsed_time& x, const elapsed_time& y)
{
  return x.sec_ < y.sec_ || (x.sec_ == y.sec_ && x.nsec_ < y.nsec_);
}

inline
bool
operator <= (const elapsed_time& x, const elapsed_time& y)
{
  return !(y < x);
}

inline
bool
operator >  (const elapsed_time& x, const elapsed_time& y)
{
  return y < x;
}

inline
bool
operator >= (const elapsed_time& x, const elapsed_time& y)
{
  return !(x < y);
}

#endif  // HTIMER
