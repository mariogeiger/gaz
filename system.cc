#include "system.hh"
#include "random.hh"
#include <QTime>
#include <QDebug>
#include <QMultiMap>
#include <map>

System::System()
	: _contract(0.0)
{
}

void System::setSizes(double x, double y, double z)
{
	_dim[0] = x;
	_dim[1] = y;
	_dim[2] = z;
}

void System::initRandomParticles(int n, double speed)
{
	_ps.clear();
	_ps.reserve(n);
	for (int i = 0; i < n; ++i) {
		Particle p;
		p.q[0] = rdm::uniformd(-_dim[0], _dim[0]);
		p.q[1] = rdm::uniformd(-_dim[1], _dim[1]);
		p.q[2] = rdm::uniformd(-_dim[2], _dim[2]);

		double a = rdm::uniformd(1.0, 3.0);
		p.r *= a;
		p.m *= pow(a, 3.0);
		p.p = Vec3::random(rdm::uniformd(0.0, speed)) * p.m;


		// test 2D
//		p.q[2] = 0.0;
//		p.p[2] = 0.0;


		//		p.color[0] = p.color[1] = double(i) / double(n);
		p.color[0] = p.color[1] = (p.q[0] + _dim[0]) / (2.0 * _dim[0]);
		//		p.q[1] = i * 3.0;
		//		p.p[1] = 10.0 * p.m;

		_ps.append(p);
	}
	computeMaxR();
}

int System::evolve(double dt)
{
	for (int k = 0; k < 3; ++k) {
		_dim[k] -= dt * _contract;
	}

	int count = 0;

	for (Particle& z : _ps)
		z.q += z.p / z.m * dt;

	// collision avec les murs
	for (Particle& z : _ps) {
		for (int k = 0; k < 3; ++k) {
			if (z.q[k] < -_dim[k] + z.r && z.p[k] < 0.0) {
				z.p[k] = -z.p[k];
				break;
			}
			if (z.q[k] > _dim[k] - z.r && z.p[k] > 0.0) {
				z.p[k] = -z.p[k];
				break;
			}
		}
	}

	// collisions entre les particules
#ifdef NAIVE
	for (int i = 0; i < _ps.size(); ++i) {
		for (int j = 0; j < _ps.size(); ++j) {
			if (i == j) continue;
			count += Particle::collision(_ps[i], _ps[j]);
		}
	}
#endif

#ifdef XSORT
	qSort(_ps.begin(), _ps.end(), [] (const Particle& a, const Particle& b) {return a.q[0] < b.q[0];});

	for (int i = 0; i < _ps.size(); ++i) {
		Particle &pi = _ps[i];
		for (int j = i-1; j >= 0
			 && pi.q[0] - _ps[j].q[0] <= _maxd; --j)
			count += Particle::collision(pi, _ps[j]);
		for (int j = i+1; j < _ps.size()
			 && _ps[j].q[0] - pi.q[0] <= _maxd; ++j)
			count += Particle::collision(pi, _ps[j]);
	}
#endif

//#define BOX
#ifdef BOX
	QMultiMap<quint64, Particle*> map;
	for (int i = 0; i < _ps.size(); ++i) {
		quint64 x = std::max(std::round((_ps[i].q[0]+_dim[0]) / _maxd), 0.0);
		quint64 y = std::max(std::round((_ps[i].q[1]+_dim[1]) / _maxd), 0.0);
		quint64 z = std::max(std::round((_ps[i].q[2]+_dim[2]) / _maxd), 0.0);
		map.insert((z<<40) + (y<<20) + x, &_ps[i]);
	}

	for (auto ic = map.constBegin(); ic != map.constEnd(); ++ic) {
		for (int z = 0; z <= 1; ++z) {
			for (int y = (z==0 ? 0 : -1); y <= 1; ++y) {
				quint64 key = ic.key();
				quint64 keyBegin = key + z*(1ul<<40) + y*(1ul<<20) + (z==0 && y==0 ? 0 : -1);
				quint64 keyEnd   = key + z*(1ul<<40) + y*(1ul<<20) + 1;
				auto i = map.lowerBound(keyBegin);
				while (i != map.constEnd() && i.key() <= keyEnd) {
					count += Particle::collision(*ic.value(), *i.value());
					++i;
				}
			}
		}
	}
#endif

#define BOXSTL
#ifdef BOXSTL
	std::multimap<u_int64_t, Particle*> map;
	for (int i = 0; i < _ps.size(); ++i) {
		u_int64_t x = std::max(std::round((_ps[i].q[0]+_dim[0]) / _maxd), 0.0);
		u_int64_t y = std::max(std::round((_ps[i].q[1]+_dim[1]) / _maxd), 0.0);
		u_int64_t z = std::max(std::round((_ps[i].q[2]+_dim[2]) / _maxd), 0.0);
		map.insert(std::make_pair((z<<40) + (y<<20) + x, &_ps[i]));
	}

	for (auto ic = map.cbegin(); ic != map.cend(); ++ic) {
		for (int z = 0; z <= 1; ++z) {
			for (int y = (z==0 ? 0 : -1); y <= 1; ++y) {
				u_int64_t key = ic->first;
				u_int64_t keyBegin = key + z*(1ul<<40) + y*(1ul<<20) + (z==0 && y==0 ? 0 : -1);
				u_int64_t keyEnd   = key + z*(1ul<<40) + y*(1ul<<20) + 1;
				auto i = map.lower_bound(keyBegin);
				while (i != map.cend() && i->first <= keyEnd) {
					count += Particle::collision(*ic->second, *i->second);
					++i;
				}
			}
		}
	}
#endif

	return count;
}

void System::computeMaxR()
{
	_maxd = 0.0;
	for (Particle& z : _ps)
		_maxd = qMax(_maxd, z.r);
	_maxd *= 2.0;
}

// pour 5000
// XSORT 100ms
// NAIVE 1885ms
// BOX 12ms
