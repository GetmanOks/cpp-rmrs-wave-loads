#include "ApplicabilityChecker.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace rmrs {

namespace {

// Человекочитаемое число для формулировок вердикта (без "120.000000")
std::string fmt(const double value, const int digits = 2) {
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%.*f", digits, value);
	return buf;
}

} // namespace

double ApplicabilityChecker::minLength(NavigationArea area) {
	// Неограниченный, R0, R1, R2 - 65 м; районы RSN/R3 - 60 м (п. 1.4.1.1)
	return isSeaArea(area) ? 65.0 : 60.0;
}

double ApplicabilityChecker::maxLength() {
	// Верхняя граница части II "Корпус" и области определения cw (п. 1.1.1.1, п. 1.3.1.4)
	return 350.0;
}

double ApplicabilityChecker::directCalcBreadthToDepthLimit(NavigationArea area) {
	// П. 1.4.1.2.1: B/D >= 2,5; для R2 и районов RSN/R3 порог 4,0
	switch (area) {
		case NavigationArea::R2:
		case NavigationArea::R2_RSN:
		case NavigationArea::R2_RSN_4_5:
		case NavigationArea::R3_RSN:
		case NavigationArea::R3:
			return 4.0;
		default:
			return 2.5;
	}
}

double ApplicabilityChecker::speedThreshold(double length) {
	// Формула 1.4.1.2.3: v = k*sqrt(L)
	const double k = (length <= 100.0)
		? 2.2
		: 2.2 - 0.25 * (length - 100.0) / 100.0;
	return k * std::sqrt(length);
}

ApplicabilityResult ApplicabilityChecker::evaluate(const ShipParameters& p) {
	ApplicabilityResult result;

	// --- Пункт 1.4.1.1: область распространения -----------------------------
	if (!p.steelWelded) {
		result.outOfScopeReasons.push_back(
			"Требования распространяются на стальные суда сварной конструкции (п. 1.1.1.1)");
	}

	const double minL = minLength(p.area);
	if (p.length < minL) {
		result.outOfScopeReasons.push_back(
			"Длина L = " + fmt(p.length) + " м меньше минимальной " +
			fmt(minL, 0) + " м для района " + toString(p.area) + " (п. 1.4.1.1)");
	}
	if (p.length > maxLength()) {
		result.outOfScopeReasons.push_back(
			"Длина L = " + fmt(p.length) +
			" м превышает 350 м: вне области распространения части II (п. 1.1.1.1)");
	}

	// Контейнеровозы неограниченного района длиной 90 м и более: часть XVIII
	if (p.type == ShipType::ContainerShip
		&& p.area == NavigationArea::Unlimited
		&& p.length >= 90.0) {
		result.outOfScopeReasons.push_back(
			"Контейнеровоз неограниченного района L >= 90 м: прочность определяется "
			"по части XVIII, а не по главе 1.4 (п. 1.4.1.1)");
	}

	// П. 1.4.1.1: эти типы остаются в главе 1.4, но есть доп. требования гл. 3
	if (p.type == ShipType::WideDeckOpening) {
		result.additionalNotes.push_back(
			"Судно с широким раскрытием палубы должно дополнительно отвечать "
			"требованиям 3.1 (п. 1.4.1.1); формулы п. 1.4.4 при этом сохраняются");
	}
	if (p.type == ShipType::TechnicalFleet) {
		result.additionalNotes.push_back(
			"Судно технического флота должно дополнительно отвечать требованиям 3.6 "
			"(п. 1.4.1.1); формулы п. 1.4.4 при этом сохраняются");
	}

	// --- Пункт 1.4.1.2: случаи, требующие прямого расчёта -------------------
	if (p.lengthToBreadth() <= 5.0) {
		result.directCalcReasons.push_back(
			"L/B = " + fmt(p.lengthToBreadth()) + " <= 5 (п. 1.4.1.2.1)");
	}
	const double bdLimit = directCalcBreadthToDepthLimit(p.area);
	if (p.breadthToDepth() >= bdLimit) {
		result.directCalcReasons.push_back(
			"B/D = " + fmt(p.breadthToDepth()) + " >= " + fmt(bdLimit, 1) +
			" (п. 1.4.1.2.1)");
	}
	if (p.blockCoeff < 0.6) {
		result.directCalcReasons.push_back(
			"Cb = " + fmt(p.blockCoeff, 3) + " < 0,6 (п. 1.4.1.2.2)");
	}
	const double vThreshold = speedThreshold(p.length);
	if (p.speed > vThreshold) {
		result.directCalcReasons.push_back(
			"Скорость v0 = " + fmt(p.speed) + " уз превышает v = " +
			fmt(vThreshold) + " уз (п. 1.4.1.2.3)");
	}
	if (p.type == ShipType::HighTemperatureCargo) {
		result.directCalcReasons.push_back(
			"Судно перевозит грузы при высокой температуре (п. 1.4.1.2)");
	}
	if (p.type == ShipType::Unusual) {
		result.directCalcReasons.push_back(
			"Судно необычной конструкции и/или назначения (п. 1.4.1.2)");
	}

	// --- Итоговый вердикт ---------------------------------------------------
	if (!result.outOfScopeReasons.empty()) {
		result.verdict = Verdict::NotInScope;
	} else if (!result.directCalcReasons.empty()) {
		result.verdict = Verdict::DirectCalculationRequired;
	} else {
		result.verdict = Verdict::FormulaApplicable;
	}

	return result;
}

} // namespace rmrs
