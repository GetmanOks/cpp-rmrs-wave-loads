#pragma once

#include "ShipParameters.h"

#include <string>
#include <vector>

namespace rmrs {

// Итоговый вердикт по применимости главы 1.4 Правил к судну
enum class Verdict {
	NotInScope,                // судно не подпадает под требования главы 1.4 (п. 1.4.1.1)
	DirectCalculationRequired, // подпадает, но нужен прямой расчёт прочности (п. 1.4.1.2)
	FormulaApplicable          // подпадает, применимы формулы п. 1.4.4
};

// Результат анализа применимости
struct ApplicabilityResult {
	Verdict                  verdict = Verdict::FormulaApplicable;
	// Причины, по которым судно НЕ попадает в область распространения (п. 1.4.1.1)
	std::vector<std::string> outOfScopeReasons;
	// Причины, по которым требуется прямой расчёт прочности (п. 1.4.1.2)
	std::vector<std::string> directCalcReasons;
	// Дополнительные требования главы 1.4, не меняющие вердикт (п. 1.4.1.1)
	std::vector<std::string> additionalNotes;

	bool inScope()               const { return verdict != Verdict::NotInScope; }
	bool formulaApplicable()     const { return verdict == Verdict::FormulaApplicable; }
};

// Проверка судна на соответствие требованиям п. 1.4.1.1 (область распространения)
// и п. 1.4.1.2 (случаи, требующие прямого расчёта прочности)
class ApplicabilityChecker {
public:
	// Минимальная длина по п. 1.4.1.1; максимум 350 м по п. 1.1.1.1 (часть II)
	static double minLength(NavigationArea area);
	static double maxLength();

	// Порог B/D для прямого расчёта (п. 1.4.1.2.1): 2,5 или 4,0 в зависимости от района
	static double directCalcBreadthToDepthLimit(NavigationArea area);

	// Пороговая скорость v = k*sqrt(L), уз (формула 1.4.1.2.3)
	static double speedThreshold(double length);

	// Полный анализ параметров судна
	static ApplicabilityResult evaluate(const ShipParameters& p);
};

} // namespace rmrs
