#pragma once

#include "NavigationArea.h"

#include <vector>

namespace rmrs {

// Значения волновых нагрузок в одном поперечном сечении корпуса
struct SectionLoads {
	double x            = 0.0; // отстояние сечения от кормового перпендикуляра, м
	double relative     = 0.0; // безразмерная координата x/L
	double momentHog    = 0.0; // Mw при перегибе (hogging), кН*м  (>= 0)
	double momentSag    = 0.0; // Mw при прогибе  (sagging), кН*м  (<= 0)
	double shearPos     = 0.0; // Nw положительная, кН  (>= 0)
	double shearNeg     = 0.0; // Nw отрицательная, кН  (<= 0)
};

// Расчёт волновых изгибающих моментов Mw и перерезывающих сил Nw
// по п. 1.4.4 Правил РМРС, часть II "Корпус"
//
// Основные формулы:
//   cw = 0,0856*L                       при L <= 90 м            (п. 1.3.1.4)
//   cw = 10,75 - ((300 - L)/100)^1,5     при 90 < L < 300 м
//   cw = 10,75                           при 300 <= L <= 350 м
//
//   Mw(hog) = +190*cw*B*L^2*Cb*alpha*1e-3        (1.4.4.1-1)
//   Mw(sag) = -110*cw*B*L^2*(Cb+0,7)*alpha*1e-3  (1.4.4.1-2)
//   Nw(+)   = +30*cw*B*L*(Cb+0,7)*f1*1e-2        (1.4.4.2-1)
//   Nw(-)   = -30*cw*B*L*(Cb+0,7)*f2*1e-2        (1.4.4.2-2)
//
// где alpha, f1, f2 - коэффициенты распределения нагрузок по длине судна
// (табл. 1.4.4.1 и 1.4.4.2); для ограниченных районов при 60...150 м
// результат домножается на phi (табл. 1.4.4.3; для R0 phi = 0,9)
class WaveLoadCalculator {
public:
	// length (L, м), breadth (B, м), blockCoeff (Cb), район плавания
	WaveLoadCalculator(double length, double breadth, double blockCoeff, NavigationArea area);

	// --- Статические зависимости (не зависят от конкретного судна) ----------

	// Волновой коэффициент cw (п. 1.3.1.4)
	static double waveCoefficient(double length);

	// Коэффициент распределения изгибающего момента alpha (табл. 1.4.4.1)
	static double alphaDistribution(double relative);

	// Вспомогательный коэффициент f0 = 190*Cb / (110*(Cb + 0,7))
	static double f0(double blockCoeff);

	// Коэффициенты распределения перерезывающей силы (табл. 1.4.4.2)
	static double positiveShearDistribution(double relative, double f0value); // f1
	static double negativeShearDistribution(double relative, double f0value); // f2

	// Редукционный коэффициент phi (табл. 1.4.4.3);
	// неограниченный район и L вне 60...150 м: 1,0; R0: 0,9
	static double reductionFactor(NavigationArea area, double length);

	// --- Параметры конкретного судна ----------------------------------------

	double waveCoefficient()  const { return cw_; }
	double effectiveBlock()   const { return cbEff_; } // Cb, но не менее 0,6 (п. 1.4.4.1)
	double reductionFactor()  const { return phi_; }

	// Нагрузки в сечении на расстоянии x (м) от кормового перпендикуляра
	SectionLoads loadsAt(double x) const;

	// Эпюры по всей длине: sections+1 равномерно расположенных сечений (x/L = 0..1)
	std::vector<SectionLoads> distribution(int sections = 20) const;

private:
	double L_;
	double B_;
	double cb_;    // исходный Cb
	double cbEff_; // Cb, ограниченный снизу значением 0,6
	double cw_;
	double phi_;
};

} // namespace rmrs
