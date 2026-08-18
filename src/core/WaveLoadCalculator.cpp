#include "WaveLoadCalculator.h"

#include <algorithm>
#include <cmath>

namespace rmrs {

WaveLoadCalculator::WaveLoadCalculator(double length, double breadth,
                                       double blockCoeff, NavigationArea area)
	: L_(length)
	, B_(breadth)
	, cb_(blockCoeff)
	, cbEff_(std::max(blockCoeff, 0.6)) // в формулах 1.4.4 Cb принимается не менее 0,6
	, cw_(waveCoefficient(length))
	, phi_(reductionFactor(area, length)) {
}

double WaveLoadCalculator::waveCoefficient(double length) {
	// Пункт 1.3.1.4
	if (length <= 90.0) {
		return 0.0856 * length;
	}
	if (length < 300.0) {
		return 10.75 - std::pow((300.0 - length) / 100.0, 1.5);
	}
	return 10.75; // 300 <= L <= 350
}

double WaveLoadCalculator::alphaDistribution(double relative) {
	// Табл. 1.4.4.1: коэффициент alpha распределения изгибающего момента по длине
	if (relative < 0.4) {
		return 2.5 * relative;
	}
	if (relative <= 0.65) {
		return 1.0;
	}
	return (1.0 - relative) / 0.35;
}

double WaveLoadCalculator::f0(double blockCoeff) {
	const double cb = std::max(blockCoeff, 0.6);
	return 190.0 * cb / (110.0 * (cb + 0.7));
}

double WaveLoadCalculator::positiveShearDistribution(double r, double f0value) {
	// Табл. 1.4.4.2, коэффициент f1 (положительная перерезывающая сила)
	if (r < 0.2) {
		return 4.6 * f0value * r;
	}
	if (r < 0.3) {
		return 0.92 * f0value;
	}
	if (r < 0.4) {
		return 0.70 + (9.2 * f0value - 7.0) * (0.4 - r);
	}
	if (r <= 0.6) {
		return 0.70;
	}
	if (r <= 0.7) {
		return 0.70 + 3.0 * (r - 0.6);
	}
	if (r <= 0.85) {
		return 1.0;
	}
	return 6.67 * (1.0 - r);
}

double WaveLoadCalculator::negativeShearDistribution(double r, double f0value) {
	// Табл. 1.4.4.2, коэффициент f2 (отрицательная перерезывающая сила)
	if (r < 0.2) {
		return 4.6 * r;
	}
	if (r < 0.3) {
		return 0.92;
	}
	if (r < 0.4) {
		return 1.58 - 2.2 * r;
	}
	if (r <= 0.6) {
		return 0.70;
	}
	if (r <= 0.7) {
		return 0.70 + (10.0 * f0value - 7.0) * (r - 0.6);
	}
	if (r <= 0.85) {
		return f0value;
	}
	return 6.67 * (1.0 - r) * f0value;
}

double WaveLoadCalculator::reductionFactor(NavigationArea area, double length) {
	// П. 1.4.4.3 / табл. 1.4.4.3: phi задан для ограниченных районов
	// при 60 м <= L <= 150 м; вне этого диапазона редукция не применяется
	// Коэффициенты psi и v приняты равными 1 (нужна геометрия носа)
	if (area == NavigationArea::Unlimited || length < 60.0 || length > 150.0) {
		return 1.0;
	}

	const double lc = length * 1.0e-2; // L * 10^-2 из формул таблицы
	double phi = 1.0;
	switch (area) {
		case NavigationArea::Unlimited:  return 1.0;
		case NavigationArea::R0:         return 0.9;
		case NavigationArea::R1:         phi = 1.10 - 0.23 * lc; break;
		case NavigationArea::R2:         phi = 1.00 - 0.25 * lc; break;
		case NavigationArea::R2_RSN:     phi = 0.94 - 0.26 * lc; break;
		case NavigationArea::R2_RSN_4_5: phi = 0.92 - 0.29 * lc; break;
		case NavigationArea::R3_RSN:     phi = 0.71 - 0.22 * lc; break;
		case NavigationArea::R3:         phi = 0.60 - 0.20 * lc; break;
	}
	// Для R1 таблица явно требует phi < 1; коэффициент не бывает отрицательным
	return std::clamp(phi, 0.0, 1.0);
}

SectionLoads WaveLoadCalculator::loadsAt(double x) const {
	const double r = (L_ > 0.0) ? x / L_ : 0.0;
	const double f0value = f0(cbEff_);

	const double alpha = alphaDistribution(r);
	const double f1    = positiveShearDistribution(r, f0value);
	const double f2    = negativeShearDistribution(r, f0value);

	SectionLoads s;
	s.x        = x;
	s.relative = r;
	// Изгибающие моменты, кН*м (формулы 1.4.4.1-1 и 1.4.4.1-2)
	s.momentHog =  190.0 * cw_ * B_ * L_ * L_ * cbEff_          * alpha * 1.0e-3 * phi_;
	s.momentSag = -110.0 * cw_ * B_ * L_ * L_ * (cbEff_ + 0.7)  * alpha * 1.0e-3 * phi_;
	// Перерезывающие силы, кН (формулы 1.4.4.2-1 и 1.4.4.2-2)
	s.shearPos  =   30.0 * cw_ * B_ * L_ * (cbEff_ + 0.7) * f1 * 1.0e-2 * phi_;
	s.shearNeg  =  -30.0 * cw_ * B_ * L_ * (cbEff_ + 0.7) * f2 * 1.0e-2 * phi_;
	return s;
}

std::vector<SectionLoads> WaveLoadCalculator::distribution(int sections) const {
	std::vector<SectionLoads> result;
	if (sections < 1) {
		sections = 1;
	}
	result.reserve(static_cast<size_t>(sections) + 1);
	for (int i = 0; i <= sections; ++i) {
		const double x = L_ * static_cast<double>(i) / static_cast<double>(sections);
		result.push_back(loadsAt(x));
	}
	return result;
}

} // namespace rmrs
