#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "ApplicabilityChecker.h"
#include "WaveLoadCalculator.h"

#include <cmath>

using namespace rmrs;

TEST_CASE("Волновой коэффициент cw (п. 1.3.1.4)") {
	// Линейный участок L <= 90 м
	CHECK(WaveLoadCalculator::waveCoefficient(50.0) == doctest::Approx(0.0856 * 50.0));
	CHECK(WaveLoadCalculator::waveCoefficient(90.0) == doctest::Approx(7.704));
	// Плато при 300...350 м
	CHECK(WaveLoadCalculator::waveCoefficient(300.0) == doctest::Approx(10.75));
	CHECK(WaveLoadCalculator::waveCoefficient(350.0) == doctest::Approx(10.75));
	// Средний участок
	CHECK(WaveLoadCalculator::waveCoefficient(150.0)
		== doctest::Approx(10.75 - std::pow(1.5, 1.5)));
	// Практическая непрерывность в точке L = 90 м
	CHECK(std::abs(WaveLoadCalculator::waveCoefficient(90.0)
		- WaveLoadCalculator::waveCoefficient(90.001)) < 0.01);
}

TEST_CASE("Коэффициент распределения изгибающего момента alpha (табл. 1.4.4.1)") {
	CHECK(WaveLoadCalculator::alphaDistribution(0.0)  == doctest::Approx(0.0));
	CHECK(WaveLoadCalculator::alphaDistribution(0.2)  == doctest::Approx(0.5));
	CHECK(WaveLoadCalculator::alphaDistribution(0.4)  == doctest::Approx(1.0));
	CHECK(WaveLoadCalculator::alphaDistribution(0.5)  == doctest::Approx(1.0));
	CHECK(WaveLoadCalculator::alphaDistribution(0.65) == doctest::Approx(1.0));
	CHECK(WaveLoadCalculator::alphaDistribution(1.0)  == doctest::Approx(0.0));
	// Симметрия середины: максимум на 0,4...0,65
	CHECK(WaveLoadCalculator::alphaDistribution(0.825) == doctest::Approx(0.5));
}

TEST_CASE("Вспомогательный коэффициент f0 и распределения перерезывающей силы") {
	const double f0 = WaveLoadCalculator::f0(0.7);
	CHECK(f0 == doctest::Approx(190.0 * 0.7 / (110.0 * 1.4)));

	// Непрерывность f1 на стыках участков
	CHECK(WaveLoadCalculator::positiveShearDistribution(0.199, f0)
		== doctest::Approx(WaveLoadCalculator::positiveShearDistribution(0.201, f0)).epsilon(0.02));
	CHECK(WaveLoadCalculator::positiveShearDistribution(0.7, f0) == doctest::Approx(1.0));
	CHECK(WaveLoadCalculator::positiveShearDistribution(1.0, f0) == doctest::Approx(0.0));

	// Непрерывность f2 на стыках участков
	CHECK(WaveLoadCalculator::negativeShearDistribution(0.2, f0) == doctest::Approx(0.92));
	CHECK(WaveLoadCalculator::negativeShearDistribution(0.4, f0) == doctest::Approx(0.70));
	CHECK(WaveLoadCalculator::negativeShearDistribution(1.0, f0) == doctest::Approx(0.0));
}

TEST_CASE("Знаки волновых нагрузок в средней части судна") {
	// Судно в середине диапазона: перегиб положителен, прогиб отрицателен и т.д.
	WaveLoadCalculator calc(120.0, 18.0, 0.75, NavigationArea::Unlimited);
	const SectionLoads s = calc.loadsAt(60.0); // мидель, x/L = 0,5

	CHECK(s.momentHog > 0.0);
	CHECK(s.momentSag < 0.0);
	CHECK(calc.reductionFactor() == doctest::Approx(1.0)); // неограниченный район
	// В средней части коэффициент alpha = 1, поэтому момент максимален по величине
	CHECK(s.momentHog == doctest::Approx(
		190.0 * calc.waveCoefficient() * 18.0 * 120.0 * 120.0 * 0.75 * 1e-3));
}

TEST_CASE("Редукционный коэффициент phi (табл. 1.4.4.3)") {
	CHECK(WaveLoadCalculator::reductionFactor(NavigationArea::Unlimited, 120.0)
		== doctest::Approx(1.0));
	// R2 при L = 120 м: phi = 1,0 - 0,25*1,2 = 0,70
	CHECK(WaveLoadCalculator::reductionFactor(NavigationArea::R2, 120.0)
		== doctest::Approx(0.70));
	// Коэффициент всегда в диапазоне (0; 1] внутри области таблицы
	const double phi = WaveLoadCalculator::reductionFactor(NavigationArea::R3, 60.0);
	CHECK(phi > 0.0);
	CHECK(phi <= 1.0);
	// Табл. 1.4.4.3 задана для 60...150 м: вне диапазона редукция не применяется
	CHECK(WaveLoadCalculator::reductionFactor(NavigationArea::R2, 180.0)
		== doctest::Approx(1.0));
	CHECK(WaveLoadCalculator::reductionFactor(NavigationArea::R3, 50.0)
		== doctest::Approx(1.0));
	// R0: phi = 0,9 при 60...150 м (табл. 1.4.4.3 изд. 174)
	CHECK(WaveLoadCalculator::reductionFactor(NavigationArea::R0, 120.0)
		== doctest::Approx(0.9));
}

TEST_CASE("Применимость: обычное судно в области распространения") {
	ShipParameters p;
	p.length = 120.0; p.breadth = 18.0; p.depth = 10.0;
	p.blockCoeff = 0.75; p.speed = 15.0;
	p.area = NavigationArea::Unlimited; p.type = ShipType::General;

	const ApplicabilityResult r = ApplicabilityChecker::evaluate(p);
	CHECK(r.verdict == Verdict::FormulaApplicable);
	CHECK(r.outOfScopeReasons.empty());
	CHECK(r.directCalcReasons.empty());
}

TEST_CASE("Применимость: короткое судно вне области распространения (п. 1.4.1.1)") {
	ShipParameters p;
	p.length = 40.0; p.breadth = 9.0; p.depth = 4.0; p.blockCoeff = 0.7;
	p.area = NavigationArea::Unlimited;

	const ApplicabilityResult r = ApplicabilityChecker::evaluate(p);
	CHECK(r.verdict == Verdict::NotInScope);
	CHECK_FALSE(r.outOfScopeReasons.empty());
}

TEST_CASE("Применимость: малый Cb требует прямого расчёта (п. 1.4.1.2.2)") {
	ShipParameters p;
	p.length = 120.0; p.breadth = 18.0; p.depth = 10.0;
	p.blockCoeff = 0.55; // < 0,6
	p.speed = 15.0; p.area = NavigationArea::Unlimited;

	const ApplicabilityResult r = ApplicabilityChecker::evaluate(p);
	CHECK(r.verdict == Verdict::DirectCalculationRequired);
	CHECK_FALSE(r.directCalcReasons.empty());
}

TEST_CASE("Пороговая скорость v = k*sqrt(L) (формула 1.4.1.2.3)") {
	// При L <= 100 м k = 2,2
	CHECK(ApplicabilityChecker::speedThreshold(100.0)
		== doctest::Approx(2.2 * std::sqrt(100.0)));
	// При L = 200 м k = 2,2 - 0,25*(100)/100 = 1,95
	CHECK(ApplicabilityChecker::speedThreshold(200.0)
		== doctest::Approx(1.95 * std::sqrt(200.0)));
}

TEST_CASE("Применимость: превышение скорости требует прямого расчёта") {
	ShipParameters p;
	p.length = 100.0; p.breadth = 18.0; p.depth = 10.0; p.blockCoeff = 0.7;
	p.speed = 30.0; // заведомо больше порога 22 уз
	p.area = NavigationArea::Unlimited;

	const ApplicabilityResult r = ApplicabilityChecker::evaluate(p);
	CHECK(r.verdict == Verdict::DirectCalculationRequired);
}

TEST_CASE("Применимость: контейнеровоз неограниченного района L >= 90 м (п. 1.4.1.1)") {
	ShipParameters p;
	p.length = 120.0; p.breadth = 20.0; p.depth = 12.0;
	p.blockCoeff = 0.70; p.speed = 18.0;
	p.area = NavigationArea::Unlimited;
	p.type = ShipType::ContainerShip;

	const ApplicabilityResult r = ApplicabilityChecker::evaluate(p);
	CHECK(r.verdict == Verdict::NotInScope);
}

TEST_CASE("Применимость: широкое раскрытие палубы - доп. требования 3.1") {
	ShipParameters p;
	p.length = 120.0; p.breadth = 18.0; p.depth = 10.0;
	p.blockCoeff = 0.75; p.speed = 15.0;
	p.area = NavigationArea::Unlimited;
	p.type = ShipType::WideDeckOpening;

	const ApplicabilityResult r = ApplicabilityChecker::evaluate(p);
	CHECK(r.verdict == Verdict::FormulaApplicable);
	CHECK_FALSE(r.additionalNotes.empty());
}

TEST_CASE("Применимость: большое L/D не выводит из главы 1.4 (п. 1.4.1.1 изд. 174)") {
	ShipParameters p;
	p.length = 200.0; p.breadth = 20.0; p.depth = 10.0; // L/D = 20; B/D = 2,0
	p.blockCoeff = 0.70; p.speed = 15.0;
	p.area = NavigationArea::Unlimited;

	const ApplicabilityResult r = ApplicabilityChecker::evaluate(p);
	CHECK(r.verdict == Verdict::FormulaApplicable);
	CHECK(r.outOfScopeReasons.empty());
}

TEST_CASE("Применимость: порог B/D для прямого расчёта зависит от района (п. 1.4.1.2.1)") {
	CHECK(ApplicabilityChecker::directCalcBreadthToDepthLimit(NavigationArea::Unlimited)
		== doctest::Approx(2.5));
	CHECK(ApplicabilityChecker::directCalcBreadthToDepthLimit(NavigationArea::R0)
		== doctest::Approx(2.5));
	CHECK(ApplicabilityChecker::directCalcBreadthToDepthLimit(NavigationArea::R2)
		== doctest::Approx(4.0));
	CHECK(ApplicabilityChecker::minLength(NavigationArea::R0) == doctest::Approx(65.0));
	CHECK(ApplicabilityChecker::minLength(NavigationArea::R3) == doctest::Approx(60.0));

	ShipParameters p;
	p.length = 120.0; p.breadth = 21.0; p.depth = 7.0; // B/D = 3,0; L/B > 5
	p.blockCoeff = 0.75; p.speed = 15.0;
	p.area = NavigationArea::R2;
	p.type = ShipType::General;

	ApplicabilityResult r = ApplicabilityChecker::evaluate(p);
	CHECK(r.verdict == Verdict::FormulaApplicable);

	p.breadth = 20.0; p.depth = 5.0; // B/D = 4,0; L/B = 6 > 5
	r = ApplicabilityChecker::evaluate(p);
	CHECK(r.verdict == Verdict::DirectCalculationRequired);
}

TEST_CASE("Знак прогиба Mw в средней части (формула 1.4.4.1-2)") {
	WaveLoadCalculator calc(120.0, 18.0, 0.75, NavigationArea::Unlimited);
	const SectionLoads s = calc.loadsAt(60.0);
	CHECK(s.momentSag == doctest::Approx(
		-110.0 * calc.waveCoefficient() * 18.0 * 120.0 * 120.0 * (0.75 + 0.7) * 1e-3));
}
